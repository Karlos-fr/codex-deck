// ============================================================================
// Codex Glass - Capture du widget vers le presse-papiers
// ----------------------------------------------------------------------------
// Ce fichier demande d'abord a Windows le contenu compose de la fenetre, puis
// le publie au format CF_DIB. Il ne gere ni le menu ni le cycle de vie Win32.
// ============================================================================

#include "WidgetScreenshotClipboard.h"

#include <dwmapi.h>

#include <cstddef>
#include <limits>

namespace {

// Nombre d'octets par pixel de la capture BGRA 32 bits.
constexpr std::size_t kScreenshotBytesPerPixel = 4;

// ----------------------------------------------------------------------------
// Capture la fenetre apres avoir suspendu son exclusion des captures systeme.
//
// GlassEffect marque normalement le widget avec WDA_EXCLUDEFROMCAPTURE afin de
// ne jamais reinjecter l'interface dans son propre fond. La commande de debug
// leve cette exclusion pendant une seule composition DWM, puis la restaure
// avant de rendre la main a la boucle de messages et au moteur de capture Glass.
//
// Parametres :
// - hwnd : fenetre a capturer.
// - target_dc : contexte memoire qui recoit les pixels.
// - window_rect : rectangle ecran de la fenetre.
// - width : largeur de la capture en pixels.
// - height : hauteur de la capture en pixels.
//
// Retour :
// - true si BitBlt a copie les pixels presentes par DWM.
// - false sinon.
// ----------------------------------------------------------------------------
bool CaptureWindowPixels(
    HWND hwnd,
    HDC target_dc,
    const RECT& window_rect,
    int width,
    int height
) {
    DWORD original_affinity = WDA_NONE;
    const bool has_affinity = GetWindowDisplayAffinity(hwnd, &original_affinity) != FALSE;
    if (has_affinity && original_affinity != WDA_NONE) {
        if (!SetWindowDisplayAffinity(hwnd, WDA_NONE)) {
            return false;
        }
    }
    RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    DwmFlush();

    HDC screen_dc = GetDC(nullptr);
    const BOOL captured = screen_dc != nullptr
        ? BitBlt(
            target_dc,
            0,
            0,
            width,
            height,
            screen_dc,
            window_rect.left,
            window_rect.top,
            SRCCOPY | CAPTUREBLT
        )
        : FALSE;
    if (screen_dc != nullptr) {
        ReleaseDC(nullptr, screen_dc);
    }

    if (has_affinity && original_affinity != WDA_NONE) {
        SetWindowDisplayAffinity(hwnd, original_affinity);
        DwmFlush();
    }

    return captured != FALSE;
}

// ----------------------------------------------------------------------------
// Copie un bitmap GDI dans un bloc CF_DIB transferable au presse-papiers.
//
// Parametres :
// - reference_dc : contexte compatible avec le bitmap source.
// - bitmap : bitmap contenant les pixels captures.
// - width : largeur du bitmap en pixels.
// - height : hauteur du bitmap en pixels.
//
// Retour :
// - bloc global verrouillable pret a etre cede au presse-papiers.
// - nullptr si l'allocation ou la lecture des pixels a echoue.
// ----------------------------------------------------------------------------
HGLOBAL CreateClipboardDib(HDC reference_dc, HBITMAP bitmap, int width, int height) {
    const std::size_t pixel_count = static_cast<std::size_t>(width)
        * static_cast<std::size_t>(height);
    if (pixel_count > (std::numeric_limits<std::size_t>::max() - sizeof(BITMAPINFOHEADER))
        / kScreenshotBytesPerPixel) {
        return nullptr;
    }

    const std::size_t pixel_bytes = pixel_count * kScreenshotBytesPerPixel;
    const std::size_t allocation_size = sizeof(BITMAPINFOHEADER) + pixel_bytes;
    HGLOBAL dib = GlobalAlloc(GMEM_MOVEABLE, allocation_size);
    if (dib == nullptr) {
        return nullptr;
    }

    auto* header = static_cast<BITMAPINFOHEADER*>(GlobalLock(dib));
    if (header == nullptr) {
        GlobalFree(dib);
        return nullptr;
    }

    *header = BITMAPINFOHEADER{};
    header->biSize = sizeof(BITMAPINFOHEADER);
    header->biWidth = width;
    header->biHeight = height;
    header->biPlanes = 1;
    header->biBitCount = 32;
    header->biCompression = BI_RGB;
    header->biSizeImage = static_cast<DWORD>(pixel_bytes);

    void* pixels = reinterpret_cast<unsigned char*>(header) + sizeof(BITMAPINFOHEADER);
    const int copied_lines = GetDIBits(
        reference_dc,
        bitmap,
        0,
        static_cast<UINT>(height),
        pixels,
        reinterpret_cast<BITMAPINFO*>(header),
        DIB_RGB_COLORS
    );
    GlobalUnlock(dib);
    if (copied_lines != height) {
        GlobalFree(dib);
        return nullptr;
    }

    return dib;
}

} // namespace

// ----------------------------------------------------------------------------
// Copie l'apparence actuellement composee du widget dans le presse-papiers.
//
// Parametres :
// - hwnd : fenetre visible dont le rectangle complet doit etre capture.
//
// Retour :
// - true si une image CF_DIB a ete placee dans le presse-papiers.
// - false si la capture ou l'acces au presse-papiers a echoue.
// ----------------------------------------------------------------------------
bool CopyWidgetScreenshotToClipboard(HWND hwnd) {
    RECT window_rect{};
    if (hwnd == nullptr || !GetWindowRect(hwnd, &window_rect)) {
        return false;
    }

    const int width = window_rect.right - window_rect.left;
    const int height = window_rect.bottom - window_rect.top;
    if (width <= 0 || height <= 0) {
        return false;
    }

    HDC screen_dc = GetDC(nullptr);
    HDC capture_dc = screen_dc != nullptr ? CreateCompatibleDC(screen_dc) : nullptr;
    HBITMAP bitmap = screen_dc != nullptr
        ? CreateCompatibleBitmap(screen_dc, width, height)
        : nullptr;
    if (screen_dc == nullptr || capture_dc == nullptr || bitmap == nullptr) {
        if (bitmap != nullptr) {
            DeleteObject(bitmap);
        }
        if (capture_dc != nullptr) {
            DeleteDC(capture_dc);
        }
        if (screen_dc != nullptr) {
            ReleaseDC(nullptr, screen_dc);
        }
        return false;
    }

    HGDIOBJ previous_bitmap = SelectObject(capture_dc, bitmap);
    const bool captured = CaptureWindowPixels(
        hwnd,
        capture_dc,
        window_rect,
        width,
        height
    );
    GdiFlush();
    SelectObject(capture_dc, previous_bitmap);

    HGLOBAL dib = captured ? CreateClipboardDib(screen_dc, bitmap, width, height) : nullptr;
    DeleteObject(bitmap);
    DeleteDC(capture_dc);
    ReleaseDC(nullptr, screen_dc);
    if (dib == nullptr) {
        return false;
    }

    if (!OpenClipboard(hwnd)) {
        GlobalFree(dib);
        return false;
    }

    const bool copied = EmptyClipboard() != FALSE
        && SetClipboardData(CF_DIB, dib) != nullptr;
    CloseClipboard();
    if (!copied) {
        GlobalFree(dib);
    }
    return copied;
}

// ----------------------------------------------------------------------------
// Copie une cible Direct2D hors ecran dans le presse-papiers.
//
// Parametres :
// - hwnd : fenetre proprietaire du presse-papiers.
// - render_target : cible bitmap compatible GDI contenant l'image finale.
//
// Retour :
// - true si l'image a ete publiee au format CF_DIB.
// - false si la cible ou le presse-papiers est indisponible.
// ----------------------------------------------------------------------------
bool CopyD2DRenderTargetScreenshotToClipboard(
    HWND hwnd,
    ID2D1BitmapRenderTarget* render_target
) {
    if (hwnd == nullptr || render_target == nullptr) {
        return false;
    }
    ID2D1GdiInteropRenderTarget* gdi_target = nullptr;
    if (FAILED(render_target->QueryInterface(
            __uuidof(ID2D1GdiInteropRenderTarget),
            reinterpret_cast<void**>(&gdi_target)
        ))) {
        return false;
    }

    render_target->BeginDraw();
    HDC source_dc = nullptr;
    const HRESULT dc_result = gdi_target->GetDC(D2D1_DC_INITIALIZE_MODE_COPY, &source_dc);
    if (FAILED(dc_result) || source_dc == nullptr) {
        render_target->EndDraw();
        gdi_target->Release();
        return false;
    }
    const D2D1_SIZE_U pixel_size = render_target->GetPixelSize();
    const int width = static_cast<int>(pixel_size.width);
    const int height = static_cast<int>(pixel_size.height);
    HDC capture_dc = CreateCompatibleDC(source_dc);
    HBITMAP bitmap = CreateCompatibleBitmap(source_dc, width, height);
    bool captured = false;
    if (capture_dc != nullptr && bitmap != nullptr) {
        HGDIOBJ previous_bitmap = SelectObject(capture_dc, bitmap);
        captured = BitBlt(capture_dc, 0, 0, width, height, source_dc, 0, 0, SRCCOPY) != FALSE;
        SelectObject(capture_dc, previous_bitmap);
    }
    gdi_target->ReleaseDC(nullptr);
    const HRESULT draw_result = render_target->EndDraw();
    gdi_target->Release();

    HGLOBAL dib = captured && SUCCEEDED(draw_result)
        ? CreateClipboardDib(capture_dc, bitmap, width, height)
        : nullptr;
    if (bitmap != nullptr) {
        DeleteObject(bitmap);
    }
    if (capture_dc != nullptr) {
        DeleteDC(capture_dc);
    }
    if (dib == nullptr) {
        return false;
    }
    if (!OpenClipboard(hwnd)) {
        GlobalFree(dib);
        return false;
    }
    const bool copied = EmptyClipboard() != FALSE
        && SetClipboardData(CF_DIB, dib) != nullptr;
    CloseClipboard();
    if (!copied) {
        GlobalFree(dib);
    }
    return copied;
}
