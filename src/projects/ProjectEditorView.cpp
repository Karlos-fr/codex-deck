// ============================================================================
// Codex Deck - Implementation de la vue editeur de projet
// ----------------------------------------------------------------------------
// Ce fichier dessine un dialogue Deck non systeme avec champs compacts et une
// confirmation explicite qui ne suggere aucune suppression de fichiers.
// ============================================================================

#include "ProjectEditorView.h"

#include <dwrite.h>
#include <wrl/client.h>

#include <algorithm>

using Microsoft::WRL::ComPtr;

namespace {

// Largeur maximale de l'overlay projet.
constexpr float kEditorWidth = 520.0F;
// Rayon des surfaces de l'overlay.
constexpr float kEditorRadius = 8.0F;

// Calcule le panneau stable partage par le rendu et le hit testing.
D2D1_RECT_F PanelBounds(const D2D1_RECT_F& bounds, ProjectEditorMode mode) {
    const float width = std::min(kEditorWidth, bounds.right - bounds.left - 40.0F);
    const float height = mode == ProjectEditorMode::ConfirmDelete ? 224.0F : 278.0F;
    const float left = bounds.left + (bounds.right - bounds.left - width) * 0.5F;
    const float top = bounds.top + std::max(24.0F, (bounds.bottom - bounds.top - height) * 0.28F);
    return D2D1::RectF(left, top, left + width, top + height);
}

// Indique si un point appartient a un rectangle.
bool Contains(const D2D1_RECT_F& rect, float x, float y) {
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

// Convertit un chemin en texte d'affichage.
std::wstring PathText(const std::filesystem::path& path) {
    return path.wstring();
}

}  // namespace

// Dessine l'overlay correspondant au modele courant.
void ProjectEditorView::Render(
    ID2D1DeviceContext* context,
    const D2D1_RECT_F& bounds,
    const ProjectEditorModel& model,
    const ThemePalette& palette
) {
    if (context == nullptr) {
        return;
    }
    ComPtr<IDWriteFactory> factory;
    ComPtr<IDWriteTextFormat> title_format;
    ComPtr<IDWriteTextFormat> text_format;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.GetAddressOf())))) {
        return;
    }
    factory->CreateTextFormat(L"Segoe UI Variable Display", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 20.0F, L"", title_format.GetAddressOf());
    factory->CreateTextFormat(L"Segoe UI Variable Text", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0F, L"", text_format.GetAddressOf());
    ComPtr<ID2D1SolidColorBrush> scrim;
    ComPtr<ID2D1SolidColorBrush> surface;
    ComPtr<ID2D1SolidColorBrush> field;
    ComPtr<ID2D1SolidColorBrush> border;
    ComPtr<ID2D1SolidColorBrush> text;
    ComPtr<ID2D1SolidColorBrush> muted;
    ComPtr<ID2D1SolidColorBrush> accent;
    context->CreateSolidColorBrush(palette.window_background, scrim.GetAddressOf());
    context->CreateSolidColorBrush(palette.surface, surface.GetAddressOf());
    context->CreateSolidColorBrush(palette.window_background, field.GetAddressOf());
    context->CreateSolidColorBrush(palette.border, border.GetAddressOf());
    context->CreateSolidColorBrush(palette.text, text.GetAddressOf());
    context->CreateSolidColorBrush(palette.text_muted, muted.GetAddressOf());
    context->CreateSolidColorBrush(palette.accent, accent.GetAddressOf());
    scrim->SetOpacity(0.74F);
    context->FillRectangle(bounds, scrim.Get());

    const D2D1_RECT_F panel_bounds = PanelBounds(bounds, model.mode);
    const float width = panel_bounds.right - panel_bounds.left;
    const float height = panel_bounds.bottom - panel_bounds.top;
    const float left = panel_bounds.left;
    const float top = panel_bounds.top;
    const D2D1_ROUNDED_RECT panel = D2D1::RoundedRect(D2D1::RectF(left, top, left + width, top + height), kEditorRadius, kEditorRadius);
    context->FillRoundedRectangle(panel, surface.Get());
    context->DrawRoundedRectangle(panel, border.Get(), 1.0F);

    const wchar_t* title = model.mode == ProjectEditorMode::Create ? L"New project"
        : model.mode == ProjectEditorMode::Rename ? L"Rename project" : L"Remove project?";
    context->DrawTextW(title, static_cast<UINT32>(wcslen(title)), title_format.Get(), D2D1::RectF(left + 22.0F, top + 18.0F, left + width - 22.0F, top + 50.0F), text.Get());

    if (model.mode == ProjectEditorMode::ConfirmDelete) {
        const wchar_t* warning = L"Only the local project grouping will be removed. Files, Git repositories, and Codex sessions stay untouched.";
        context->DrawTextW(warning, static_cast<UINT32>(wcslen(warning)), text_format.Get(), D2D1::RectF(left + 22.0F, top + 66.0F, left + width - 22.0F, top + 132.0F), muted.Get());
        context->DrawTextW(L"Cancel", 6, text_format.Get(), D2D1::RectF(left + 280.0F, top + 170.0F, left + 350.0F, top + 204.0F), muted.Get());
        context->DrawTextW(L"Remove project", 14, text_format.Get(), D2D1::RectF(left + 366.0F, top + 170.0F, left + width - 18.0F, top + 204.0F), accent.Get());
        return;
    }

    const std::wstring root = PathText(model.root);
    const wchar_t* labels[] = {L"Name", L"Root folder"};
    const std::wstring values[] = {model.name, root};
    for (std::size_t index = 0; index < 2; ++index) {
        const float y = top + 66.0F + static_cast<float>(index) * 68.0F;
        context->DrawTextW(labels[index], static_cast<UINT32>(wcslen(labels[index])), text_format.Get(), D2D1::RectF(left + 22.0F, y, left + 112.0F, y + 26.0F), muted.Get());
        const D2D1_ROUNDED_RECT input = D2D1::RoundedRect(D2D1::RectF(left + 116.0F, y - 6.0F, left + width - 22.0F, y + 34.0F), 5.0F, 5.0F);
        context->FillRoundedRectangle(input, field.Get());
        context->DrawRoundedRectangle(input, index == model.active_field ? accent.Get() : border.Get(), index == model.active_field ? 1.25F : 1.0F);
        context->DrawTextW(values[index].c_str(), static_cast<UINT32>(values[index].size()), text_format.Get(), D2D1::RectF(input.rect.left + 12.0F, y + 3.0F, input.rect.right - 12.0F, y + 28.0F), text.Get());
    }
    if (!model.error.empty()) {
        context->DrawTextW(model.error.c_str(), static_cast<UINT32>(model.error.size()), text_format.Get(), D2D1::RectF(left + 22.0F, top + 204.0F, left + width - 130.0F, top + 238.0F), accent.Get());
    }
    context->DrawTextW(L"Cancel", 6, text_format.Get(), D2D1::RectF(left + width - 190.0F, top + 232.0F, left + width - 120.0F, top + 266.0F), muted.Get());
    const wchar_t* action = model.mode == ProjectEditorMode::Create ? L"Create" : L"Rename";
    context->DrawTextW(action, static_cast<UINT32>(wcslen(action)), text_format.Get(), D2D1::RectF(left + width - 94.0F, top + 232.0F, left + width - 22.0F, top + 266.0F), accent.Get());
}

// Retourne la zone interactive sous un point client.
ProjectEditorHitTarget ProjectEditorView::HitTest(
    const D2D1_RECT_F& bounds,
    const ProjectEditorModel& model,
    float x,
    float y
) const {
    const D2D1_RECT_F panel = PanelBounds(bounds, model.mode);
    if (model.mode != ProjectEditorMode::ConfirmDelete
        && Contains(D2D1::RectF(panel.left + 116.0F, panel.top + 60.0F, panel.right - 22.0F, panel.top + 100.0F), x, y)) {
        return ProjectEditorHitTarget::Name;
    }
    const float actions_top = model.mode == ProjectEditorMode::ConfirmDelete ? panel.top + 158.0F : panel.top + 220.0F;
    if (Contains(D2D1::RectF(panel.right - 208.0F, actions_top, panel.right - 112.0F, panel.bottom), x, y)) {
        return ProjectEditorHitTarget::Cancel;
    }
    if (Contains(D2D1::RectF(panel.right - 112.0F, actions_top, panel.right, panel.bottom), x, y)) {
        return ProjectEditorHitTarget::Submit;
    }
    return ProjectEditorHitTarget::None;
}
