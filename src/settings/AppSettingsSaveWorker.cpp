// ============================================================================
// Codex Glass - Worker de sauvegarde des reglages
// ----------------------------------------------------------------------------
// Ce fichier serialise les ecritures settings.ini hors du thread UI et fusionne
// les demandes rapprochees afin de conserver uniquement l'etat le plus recent.
// ============================================================================

#include "AppSettingsSaveWorker.h"

#include <utility>

// ----------------------------------------------------------------------------
// Demarre le thread de sauvegarde sans travail initial.
// ----------------------------------------------------------------------------
AppSettingsSaveWorker::AppSettingsSaveWorker()
    : thread_(&AppSettingsSaveWorker::Run, this) {
}

// ----------------------------------------------------------------------------
// Termine les ecritures en attente puis arrete le thread.
// ----------------------------------------------------------------------------
AppSettingsSaveWorker::~AppSettingsSaveWorker() {
    Flush();
    {
        const std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    condition_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

// ----------------------------------------------------------------------------
// Remplace la sauvegarde en attente par le snapshot le plus recent.
//
// Parametres :
// - settings : copie coherente des reglages a persister.
// ----------------------------------------------------------------------------
void AppSettingsSaveWorker::Schedule(const AppSettings& settings) {
    {
        const std::lock_guard lock(mutex_);
        pending_settings_ = settings;
    }
    condition_.notify_all();
}

// ----------------------------------------------------------------------------
// Attend que toutes les sauvegardes programmees soient terminees.
// ----------------------------------------------------------------------------
void AppSettingsSaveWorker::Flush() {
    std::unique_lock lock(mutex_);
    condition_.wait(lock, [this]() {
        return !writing_ && !pending_settings_.has_value();
    });
}

// ----------------------------------------------------------------------------
// Execute les sauvegardes successives jusqu'a la demande d'arret.
// ----------------------------------------------------------------------------
void AppSettingsSaveWorker::Run() {
    for (;;) {
        AppSettings settings{};
        {
            std::unique_lock lock(mutex_);
            condition_.wait(lock, [this]() {
                return stopping_ || pending_settings_.has_value();
            });
            if (stopping_ && !pending_settings_.has_value()) {
                return;
            }
            settings = std::move(*pending_settings_);
            pending_settings_.reset();
            writing_ = true;
        }

        SaveAppSettings(settings);

        {
            const std::lock_guard lock(mutex_);
            writing_ = false;
        }
        condition_.notify_all();
    }
}
