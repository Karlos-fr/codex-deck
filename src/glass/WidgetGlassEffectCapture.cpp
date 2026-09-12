// ============================================================================
// Codex Glass - Implementation de la capture GlassEffect
// ----------------------------------------------------------------------------
// Ce fichier capture le fond du widget via DXGI Desktop Duplication, hors du
// chemin WM_PAINT. La texture produite sera consommee par le rendu en phase H.
// ============================================================================

#include "WidgetGlassEffectCapture.h"

#include "WidgetGlassEffectFrame.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <algorithm>
#include <chrono>
#include <memory>

using Microsoft::WRL::ComPtr;

namespace {

// Nombre d'echecs consecutifs avant recreation complete de la session DXGI.
constexpr int kMaximumConsecutiveFailures = 30;

// Delai de reprise apres la perte d'une session Desktop Duplication.
constexpr std::chrono::milliseconds kCaptureRecoveryDelay{250};

// Temps d'attente nul pour sonder DXGI sans bloquer l'UI.
constexpr UINT kAcquireTimeoutMs = 0;

// Nombre maximal de frames DXGI lues par tick pour garder la plus recente.
constexpr int kMaximumFramesPerTick = 3;

// ----------------------------------------------------------------------------
// Convertit une taille de rectangle en largeur positive.
// ----------------------------------------------------------------------------
UINT RectWidth(const RECT& rect) {
    return static_cast<UINT>(std::max<LONG>(0, rect.right - rect.left));
}

// ----------------------------------------------------------------------------
// Convertit une taille de rectangle en hauteur positive.
// ----------------------------------------------------------------------------
UINT RectHeight(const RECT& rect) {
    return static_cast<UINT>(std::max<LONG>(0, rect.bottom - rect.top));
}

// ----------------------------------------------------------------------------
// Indique si deux rectangles ecran se croisent.
// ----------------------------------------------------------------------------
bool Intersects(const RECT& first, const RECT& second) {
    return first.left < second.right
        && first.right > second.left
        && first.top < second.bottom
        && first.bottom > second.top;
}

// ----------------------------------------------------------------------------
// Indique si une frame DXGI ne contient qu'une mise a jour du pointeur.
//
// Parametres :
// - frame_info : metadonnees DXGI de la frame acquise.
//
// Retour :
// - true si la frame ne represente pas une nouvelle image du bureau.
// ----------------------------------------------------------------------------
bool IsPointerOnlyFrame(const DXGI_OUTDUPL_FRAME_INFO& frame_info) {
    return frame_info.LastPresentTime.QuadPart == 0;
}

// ----------------------------------------------------------------------------
// Retourne l'intersection de deux rectangles ecran.
// ----------------------------------------------------------------------------
RECT IntersectRects(const RECT& first, const RECT& second) {
    return RECT{
        std::max(first.left, second.left),
        std::max(first.top, second.top),
        std::min(first.right, second.right),
        std::min(first.bottom, second.bottom),
    };
}

}  // namespace

// ----------------------------------------------------------------------------
// Implementation privee contenant les ressources DXGI/D3D11.
// ----------------------------------------------------------------------------
struct WidgetGlassEffectCapture::Impl {
    // Fenetre du widget dont le fond doit etre echantillonne.
    HWND hwnd = nullptr;

    // Fenetre compagne croppee depuis la meme acquisition du bureau.
    HWND companion_hwnd = nullptr;

    // Device D3D11 partage avec le runtime GlassEffect.
    ComPtr<ID3D11Device> device;

    // Contexte immediat utilise pour copier le crop.
    ComPtr<ID3D11DeviceContext> context;

    // Duplication DXGI de l'output courant.
    ComPtr<IDXGIOutputDuplication> duplication;

    // Moniteur associe a la duplication courante.
    HMONITOR monitor = nullptr;

    // Rectangle ecran de l'output courant.
    RECT output_rect{};

    // Derniere texture capturee et possedee par l'application.
    ComPtr<ID3D11Texture2D> latest_texture;

    // Derniere frame CPU extraite depuis la texture capturee.
    WidgetGlassEffectFrame latest_frame;

    // Derniere texture capturee pour la fenetre compagne.
    ComPtr<ID3D11Texture2D> latest_companion_texture;

    // Derniere frame CPU extraite pour la fenetre compagne.
    WidgetGlassEffectFrame latest_companion_frame;

    // Rectangle ecran correspondant a la derniere texture.
    RECT latest_rect{};

    // Indique si la fenetre a ete exclue des captures systeme.
    bool capture_exclusion_applied = false;

    // Indique si la fenetre compagne a ete exclue des captures systeme.
    bool companion_capture_exclusion_applied = false;

    // Nombre d'echecs consecutifs de capture.
    int consecutive_failures = 0;

    // Instant avant lequel une session DXGI perdue ne doit pas etre recreee.
    std::chrono::steady_clock::time_point retry_after{};

#if defined(_DEBUG)
    // Nombre total de captures reussies en debug.
    unsigned long long successful_captures = 0;

    // Nombre total d'echecs en debug.
    unsigned long long failed_captures = 0;
#endif
};

// ----------------------------------------------------------------------------
// Efface les textures et metadonnees qui ne correspondent plus au bureau actif.
//
// Parametres :
// - impl : etat de capture dont les frames doivent etre invalidees.
//
// Effet de bord :
// - les appels LatestFrame retournent nullptr jusqu'a la prochaine capture.
// ----------------------------------------------------------------------------
static void ClearCapturedFrames(WidgetGlassEffectCapture::Impl& impl) {
    impl.latest_texture.Reset();
    impl.latest_frame = WidgetGlassEffectFrame{};
    impl.latest_companion_texture.Reset();
    impl.latest_companion_frame = WidgetGlassEffectFrame{};
    impl.latest_rect = RECT{};
}

// ----------------------------------------------------------------------------
// Ferme la duplication courante et prepare une tentative de recreation.
//
// Parametres :
// - impl : etat de capture a reinitialiser.
// - delay : delai avant la prochaine tentative DXGI.
//
// Effet de bord :
// - invalide les anciennes frames afin de ne jamais afficher un bureau fige.
// ----------------------------------------------------------------------------
static void ResetCaptureSession(
    WidgetGlassEffectCapture::Impl& impl,
    std::chrono::milliseconds delay
) {
    impl.duplication.Reset();
    impl.monitor = nullptr;
    impl.output_rect = RECT{};
    impl.consecutive_failures = 0;
    impl.retry_after = delay.count() > 0
        ? std::chrono::steady_clock::now() + delay
        : std::chrono::steady_clock::time_point{};
    ClearCapturedFrames(impl);
}

// ----------------------------------------------------------------------------
// Comptabilise un echec et recycle une session devenue durablement inutilisable.
//
// Parametres :
// - impl : etat de capture ayant rencontre l'echec.
//
// Effet de bord :
// - declenche une reprise temporisee au lieu d'arreter definitivement la capture.
// ----------------------------------------------------------------------------
static void RecordCaptureFailure(WidgetGlassEffectCapture::Impl& impl) {
    ++impl.consecutive_failures;
#if defined(_DEBUG)
    ++impl.failed_captures;
#endif
    if (impl.consecutive_failures >= kMaximumConsecutiveFailures) {
        ResetCaptureSession(impl, kCaptureRecoveryDelay);
    }
}

// ----------------------------------------------------------------------------
// Cree le module sans ouvrir de duplication DXGI.
// ----------------------------------------------------------------------------
WidgetGlassEffectCapture::WidgetGlassEffectCapture() = default;

// ----------------------------------------------------------------------------
// Detruit le module et libere les ressources DXGI.
// ----------------------------------------------------------------------------
WidgetGlassEffectCapture::~WidgetGlassEffectCapture() = default;

// ----------------------------------------------------------------------------
// Initialise la capture avec les ressources D3D11 partagees.
// ----------------------------------------------------------------------------
bool WidgetGlassEffectCapture::Initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context) {
    if (hwnd == nullptr || device == nullptr || context == nullptr) {
        Shutdown();
        return false;
    }

    if (impl_ == nullptr) {
        impl_ = std::make_unique<Impl>();
    }

    const bool graphics_resources_changed = impl_->device.Get() != device
        || impl_->context.Get() != context;
    if (graphics_resources_changed) {
        ResetCaptureSession(*impl_, std::chrono::milliseconds{0});
    }
    impl_->hwnd = hwnd;
    impl_->device = device;
    impl_->context = context;
    if (!impl_->capture_exclusion_applied) {
        impl_->capture_exclusion_applied = SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE) != FALSE;
    }
    return true;
}

// ----------------------------------------------------------------------------
// Definit une fenetre compagne capturee depuis la meme frame DXGI.
//
// Parametres :
// - hwnd : fenetre compagne, ou nullptr pour la retirer.
// ----------------------------------------------------------------------------
void WidgetGlassEffectCapture::SetCompanionWindow(HWND hwnd) {
    if (impl_ == nullptr || impl_->companion_hwnd == hwnd) {
        return;
    }

    if (impl_->companion_capture_exclusion_applied && impl_->companion_hwnd != nullptr) {
        SetWindowDisplayAffinity(impl_->companion_hwnd, WDA_NONE);
    }

    impl_->companion_hwnd = hwnd;
    impl_->companion_capture_exclusion_applied = hwnd != nullptr
        && SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE) != FALSE;
    impl_->latest_companion_texture.Reset();
    impl_->latest_companion_frame = WidgetGlassEffectFrame{};
}

// ----------------------------------------------------------------------------
// Libere les ressources de capture.
// ----------------------------------------------------------------------------
void WidgetGlassEffectCapture::Shutdown() {
    if (impl_ != nullptr && impl_->companion_capture_exclusion_applied && impl_->companion_hwnd != nullptr) {
        SetWindowDisplayAffinity(impl_->companion_hwnd, WDA_NONE);
    }
    if (impl_ != nullptr && impl_->capture_exclusion_applied && impl_->hwnd != nullptr) {
        SetWindowDisplayAffinity(impl_->hwnd, WDA_NONE);
    }

    impl_.reset();
}

// ----------------------------------------------------------------------------
// Ouvre une duplication DXGI pour le moniteur demande.
// ----------------------------------------------------------------------------
static bool EnsureDuplicationForMonitor(WidgetGlassEffectCapture::Impl& impl, HMONITOR monitor) {
    if (impl.duplication && impl.monitor == monitor) {
        return true;
    }

    impl.duplication.Reset();
    impl.monitor = nullptr;
    impl.output_rect = RECT{};

    ComPtr<IDXGIDevice> dxgi_device;
    if (FAILED(impl.device.As(&dxgi_device))) {
        return false;
    }

    ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgi_device->GetAdapter(adapter.GetAddressOf()))) {
        return false;
    }

    for (UINT output_index = 0;; ++output_index) {
        ComPtr<IDXGIOutput> output;
        const HRESULT enum_result = adapter->EnumOutputs(output_index, output.GetAddressOf());
        if (enum_result == DXGI_ERROR_NOT_FOUND) {
            return false;
        }

        if (FAILED(enum_result)) {
            return false;
        }

        DXGI_OUTPUT_DESC output_desc{};
        if (FAILED(output->GetDesc(&output_desc)) || output_desc.Monitor != monitor) {
            continue;
        }

        ComPtr<IDXGIOutput1> output1;
        if (FAILED(output.As(&output1))) {
            return false;
        }

        if (FAILED(output1->DuplicateOutput(impl.device.Get(), impl.duplication.GetAddressOf()))) {
            return false;
        }

        impl.monitor = monitor;
        impl.output_rect = output_desc.DesktopCoordinates;
        return true;
    }
}

// ----------------------------------------------------------------------------
// Cree la texture possedee qui recevra le crop de fond.
// ----------------------------------------------------------------------------
static bool CreateOwnedCropTexture(
    WidgetGlassEffectCapture::Impl& impl,
    ID3D11Texture2D* source_texture,
    const RECT& source_rect,
    ID3D11Texture2D** target_texture
) {
    D3D11_TEXTURE2D_DESC source_desc{};
    source_texture->GetDesc(&source_desc);

    D3D11_TEXTURE2D_DESC target_desc = source_desc;
    target_desc.Width = RectWidth(source_rect);
    target_desc.Height = RectHeight(source_rect);
    target_desc.MipLevels = 1;
    target_desc.ArraySize = 1;
    target_desc.Usage = D3D11_USAGE_DEFAULT;
    target_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    target_desc.CPUAccessFlags = 0;
    target_desc.MiscFlags = 0;

    if (target_desc.Width == 0 || target_desc.Height == 0) {
        return false;
    }

    ComPtr<ID3D11Texture2D> owned_texture;
    if (FAILED(impl.device->CreateTexture2D(&target_desc, nullptr, owned_texture.GetAddressOf()))) {
        return false;
    }

    D3D11_BOX source_box{};
    source_box.left = static_cast<UINT>(source_rect.left);
    source_box.top = static_cast<UINT>(source_rect.top);
    source_box.front = 0;
    source_box.right = static_cast<UINT>(source_rect.right);
    source_box.bottom = static_cast<UINT>(source_rect.bottom);
    source_box.back = 1;

    impl.context->CopySubresourceRegion(
        owned_texture.Get(),
        0,
        0,
        0,
        0,
        source_texture,
        0,
        &source_box
    );

    *target_texture = owned_texture.Detach();
    return true;
}

// ----------------------------------------------------------------------------
// Met a jour les metadonnees d'une frame depuis sa texture GPU possedee.
//
// Parametres :
// - impl : capture proprietaire du device et du contexte.
// - texture : texture GPU gardee en vie par la capture.
// - frame : metadonnees remplacees par la nouvelle generation.
//
// Retour :
// - true si la texture possede une taille exploitable.
// ----------------------------------------------------------------------------
static bool UpdateGpuFrame(
    WidgetGlassEffectCapture::Impl& impl,
    ID3D11Texture2D* texture,
    WidgetGlassEffectFrame& frame
) {
    D3D11_TEXTURE2D_DESC texture_desc{};
    texture->GetDesc(&texture_desc);
    if (texture_desc.Width == 0 || texture_desc.Height == 0) {
        return false;
    }

    WidgetGlassEffectFrame next_frame{};
    next_frame.width = texture_desc.Width;
    next_frame.height = texture_desc.Height;
    next_frame.stride = texture_desc.Width * 4U;
    next_frame.generation = frame.generation + 1U;
    next_frame.gpu_texture = texture;
    next_frame.gpu_device = impl.device.Get();
    next_frame.gpu_context = impl.context.Get();
    frame = std::move(next_frame);
    return true;
}

// ----------------------------------------------------------------------------
// Capture le fond courant autour du widget.
// ----------------------------------------------------------------------------
bool WidgetGlassEffectCapture::CaptureNextFrame() {
    if (impl_ == nullptr || std::chrono::steady_clock::now() < impl_->retry_after) {
        return false;
    }

    RECT window_rect{};
    if (!GetWindowRect(impl_->hwnd, &window_rect)) {
        RecordCaptureFailure(*impl_);
        return false;
    }

    const HMONITOR monitor = MonitorFromRect(&window_rect, MONITOR_DEFAULTTONEAREST);
    if (monitor == nullptr || !EnsureDuplicationForMonitor(*impl_, monitor) || !Intersects(window_rect, impl_->output_rect)) {
        RecordCaptureFailure(*impl_);
        return false;
    }

    const RECT crop_screen_rect = IntersectRects(window_rect, impl_->output_rect);
    const RECT crop_source_rect{
        crop_screen_rect.left - impl_->output_rect.left,
        crop_screen_rect.top - impl_->output_rect.top,
        crop_screen_rect.right - impl_->output_rect.left,
        crop_screen_rect.bottom - impl_->output_rect.top,
    };

    RECT companion_crop_screen_rect{};
    RECT companion_crop_source_rect{};
    bool capture_companion = false;
    if (impl_->companion_hwnd != nullptr) {
        RECT companion_window_rect{};
        if (GetWindowRect(impl_->companion_hwnd, &companion_window_rect)
            && MonitorFromRect(&companion_window_rect, MONITOR_DEFAULTTONEAREST) == monitor
            && Intersects(companion_window_rect, impl_->output_rect)) {
            companion_crop_screen_rect = IntersectRects(companion_window_rect, impl_->output_rect);
            companion_crop_source_rect = RECT{
                companion_crop_screen_rect.left - impl_->output_rect.left,
                companion_crop_screen_rect.top - impl_->output_rect.top,
                companion_crop_screen_rect.right - impl_->output_rect.left,
                companion_crop_screen_rect.bottom - impl_->output_rect.top,
            };
            capture_companion = true;
        }
    }

    bool captured = false;
    for (int attempt = 0; attempt < kMaximumFramesPerTick; ++attempt) {
        DXGI_OUTDUPL_FRAME_INFO frame_info{};
        ComPtr<IDXGIResource> desktop_resource;
        const HRESULT acquire_result = impl_->duplication->AcquireNextFrame(
            attempt == 0 ? kAcquireTimeoutMs : 0,
            &frame_info,
            desktop_resource.GetAddressOf()
        );

        if (acquire_result == DXGI_ERROR_WAIT_TIMEOUT) {
            return captured;
        }

        if (acquire_result == DXGI_ERROR_ACCESS_LOST) {
            ResetCaptureSession(*impl_, kCaptureRecoveryDelay);
            return captured;
        }

        if (FAILED(acquire_result)) {
            RecordCaptureFailure(*impl_);
            return captured;
        }

        if (IsPointerOnlyFrame(frame_info)) {
            impl_->duplication->ReleaseFrame();
            continue;
        }

        ComPtr<ID3D11Texture2D> desktop_texture;
        const HRESULT texture_result = desktop_resource.As(&desktop_texture);
        if (FAILED(texture_result)) {
            impl_->duplication->ReleaseFrame();
            RecordCaptureFailure(*impl_);
            return captured;
        }

        ComPtr<ID3D11Texture2D> owned_texture;
        const bool copied = CreateOwnedCropTexture(
            *impl_,
            desktop_texture.Get(),
            crop_source_rect,
            owned_texture.GetAddressOf()
        );
        ComPtr<ID3D11Texture2D> companion_texture;
        const bool companion_copied = !capture_companion || CreateOwnedCropTexture(
            *impl_,
            desktop_texture.Get(),
            companion_crop_source_rect,
            companion_texture.GetAddressOf()
        );

        impl_->duplication->ReleaseFrame();

        if (!copied) {
            RecordCaptureFailure(*impl_);
            return captured;
        }

        impl_->latest_texture = owned_texture;
        if (!UpdateGpuFrame(*impl_, impl_->latest_texture.Get(), impl_->latest_frame)) {
            RecordCaptureFailure(*impl_);
            return captured;
        }
        impl_->latest_rect = crop_screen_rect;
        if (capture_companion && companion_copied) {
            impl_->latest_companion_texture = companion_texture;
            UpdateGpuFrame(
                *impl_,
                impl_->latest_companion_texture.Get(),
                impl_->latest_companion_frame
            );
        }
        impl_->consecutive_failures = 0;
        impl_->retry_after = std::chrono::steady_clock::time_point{};
        captured = true;
#if defined(_DEBUG)
        ++impl_->successful_captures;
#endif
    }

    return captured;
}

// ----------------------------------------------------------------------------
// Indique si une capture est active.
// ----------------------------------------------------------------------------
bool WidgetGlassEffectCapture::IsActive() const {
    return impl_ != nullptr && impl_->latest_texture;
}

// ----------------------------------------------------------------------------
// Retourne le dernier snapshot GPU disponible.
// ----------------------------------------------------------------------------
const WidgetGlassEffectFrame* WidgetGlassEffectCapture::LatestFrame() const {
    if (impl_ == nullptr || impl_->latest_frame.gpu_texture == nullptr) {
        return nullptr;
    }

    return &impl_->latest_frame;
}

// ----------------------------------------------------------------------------
// Retourne le dernier snapshot GPU de la fenetre compagne.
//
// Retour :
// - pointeur non possede, ou nullptr si aucune frame n'est disponible.
// ----------------------------------------------------------------------------
const WidgetGlassEffectFrame* WidgetGlassEffectCapture::LatestCompanionFrame() const {
    if (impl_ == nullptr || impl_->latest_companion_frame.gpu_texture == nullptr) {
        return nullptr;
    }

    return &impl_->latest_companion_frame;
}
