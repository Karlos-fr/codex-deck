// ============================================================================
// Codex Deck - Implementation du processus app-server
// ----------------------------------------------------------------------------
// Ce fichier encapsule CreateProcessW, les pipes anonymes et le Job Object de
// supervision. Aucun appel n'est destine au thread UI.
// ============================================================================

#include "CodexProcess.h"

#include <string>
#include <vector>

namespace {

// ----------------------------------------------------------------------------
// Ferme un handle Win32 s'il est valide.
//
// Parametres :
// - handle : handle a fermer.
// ----------------------------------------------------------------------------
void CloseIfValid(HANDLE& handle) {
    if (handle != nullptr && handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
    }
    handle = nullptr;
}

// ----------------------------------------------------------------------------
// Construit une erreur Win32 de lancement.
//
// Parametres :
// - code : famille d'erreur.
// - message : diagnostic court.
//
// Retour :
// - erreur avec GetLastError.
// ----------------------------------------------------------------------------
CodexError LastError(CodexErrorCode code, const wchar_t* message) {
    return CodexError{code, message, static_cast<int>(GetLastError())};
}

// ----------------------------------------------------------------------------
// Cree un pipe anonyme dont l'extremite enfant est heritable.
//
// Parametres :
// - parent_end : extremite conservee par le parent.
// - child_end : extremite heritee par l'enfant.
// - parent_reads : true si le parent lit dans le pipe.
//
// Retour :
// - true si le pipe est pret.
// ----------------------------------------------------------------------------
bool CreateChildPipe(HANDLE* parent_end, HANDLE* child_end, bool parent_reads) {
    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = TRUE;

    HANDLE read = nullptr;
    HANDLE write = nullptr;
    if (!CreatePipe(&read, &write, &attributes, 0)) {
        return false;
    }

    if (parent_reads) {
        *parent_end = read;
        *child_end = write;
        SetHandleInformation(*parent_end, HANDLE_FLAG_INHERIT, 0);
    } else {
        *parent_end = write;
        *child_end = read;
        SetHandleInformation(*parent_end, HANDLE_FLAG_INHERIT, 0);
    }
    return true;
}

// ----------------------------------------------------------------------------
// Cree un Job Object qui tue les enfants a la fermeture.
//
// Retour :
// - handle de job ou nullptr.
// ----------------------------------------------------------------------------
HANDLE CreateKillOnCloseJob() {
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (job == nullptr) {
        return nullptr;
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        CloseHandle(job);
        return nullptr;
    }
    return job;
}

}  // namespace

// ----------------------------------------------------------------------------
// Arrete le processus et libere les handles restants.
// ----------------------------------------------------------------------------
CodexProcess::~CodexProcess() {
    Stop();
}

// ----------------------------------------------------------------------------
// Lance le processus enfant selon la specification fournie.
// ----------------------------------------------------------------------------
std::expected<void, CodexError> CodexProcess::Start(const CodexLaunchSpec& spec) {
    Stop();

    HANDLE child_stdin_read = nullptr;
    HANDLE child_stdout_write = nullptr;
    if (!CreateChildPipe(&stdin_write_, &child_stdin_read, false)) {
        return std::unexpected(LastError(CodexErrorCode::ProcessLaunchFailed, L"Creation du pipe stdin impossible"));
    }
    if (!CreateChildPipe(&stdout_read_, &child_stdout_write, true)) {
        CloseIfValid(child_stdin_read);
        CloseIfValid(stdin_write_);
        return std::unexpected(LastError(CodexErrorCode::ProcessLaunchFailed, L"Creation du pipe stdout impossible"));
    }

    job_ = CreateKillOnCloseJob();
    if (job_ == nullptr) {
        CloseIfValid(child_stdin_read);
        CloseIfValid(child_stdout_write);
        CloseIfValid(stdin_write_);
        CloseIfValid(stdout_read_);
        return std::unexpected(LastError(CodexErrorCode::ProcessLaunchFailed, L"Creation du Job Object impossible"));
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = child_stdin_read;
    startup.hStdOutput = child_stdout_write;
    startup.hStdError = child_stdout_write;

    PROCESS_INFORMATION process_info{};
    std::vector<wchar_t> command_line(spec.command_line.begin(), spec.command_line.end());
    command_line.push_back(L'\0');

    const BOOL created = CreateProcessW(
        spec.application_path.c_str(),
        command_line.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startup,
        &process_info
    );

    CloseIfValid(child_stdin_read);
    CloseIfValid(child_stdout_write);

    if (!created) {
        const CodexError error = LastError(CodexErrorCode::ProcessLaunchFailed, L"Lancement de codex app-server impossible");
        CloseIfValid(stdin_write_);
        CloseIfValid(stdout_read_);
        CloseIfValid(job_);
        return std::unexpected(error);
    }

    process_ = process_info.hProcess;
    thread_ = process_info.hThread;
    if (!AssignProcessToJobObject(job_, process_)) {
        Stop();
        return std::unexpected(LastError(CodexErrorCode::ProcessLaunchFailed, L"Association au Job Object impossible"));
    }
    return {};
}

// ----------------------------------------------------------------------------
// Transfere la possession du pipe stdout parent.
// ----------------------------------------------------------------------------
HANDLE CodexProcess::TakeStdoutReadHandle() {
    HANDLE handle = stdout_read_;
    stdout_read_ = nullptr;
    return handle;
}

// ----------------------------------------------------------------------------
// Retourne le pipe stdin parent conserve par le processus.
// ----------------------------------------------------------------------------
HANDLE CodexProcess::StdinWriteHandle() const {
    return stdin_write_;
}

// ----------------------------------------------------------------------------
// Indique si le processus enfant est encore actif.
// ----------------------------------------------------------------------------
bool CodexProcess::IsRunning() const {
    if (process_ == nullptr) {
        return false;
    }
    DWORD exit_code = 0;
    return GetExitCodeProcess(process_, &exit_code) && exit_code == STILL_ACTIVE;
}

// ----------------------------------------------------------------------------
// Arrete le processus et ferme les handles possedes.
// ----------------------------------------------------------------------------
void CodexProcess::Stop() {
    CloseIfValid(stdin_write_);
    CloseIfValid(stdout_read_);

    if (process_ != nullptr && IsRunning()) {
        WaitForSingleObject(process_, 100);
        if (IsRunning()) {
            if (job_ != nullptr) {
                TerminateJobObject(job_, 0);
            } else {
                TerminateProcess(process_, 0);
            }
            WaitForSingleObject(process_, 1000);
        }
    }

    CloseIfValid(thread_);
    CloseIfValid(process_);
    CloseIfValid(job_);
}
