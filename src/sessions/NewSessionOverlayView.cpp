// ============================================================================
// Codex Deck - Implementation de l'overlay nouvelle session
// ----------------------------------------------------------------------------
// Ce fichier fournit un formulaire compact adapte au clavier et conserve les
// choix de projet, workspace, modele et prompt clairement lisibles.
// ============================================================================

#include "NewSessionOverlayView.h"

#include <dwrite.h>
#include <wrl/client.h>

#include <algorithm>
#include <cwchar>

using Microsoft::WRL::ComPtr;

namespace {

// Largeur maximale du formulaire.
constexpr float kOverlayWidth = 620.0F;
// Rayon commun des surfaces.
constexpr float kOverlayRadius = 8.0F;

// Calcule le panneau stable partage par le rendu et le hit testing.
D2D1_RECT_F PanelBounds(const D2D1_RECT_F& bounds) {
    const float width = std::min(kOverlayWidth, bounds.right - bounds.left - 40.0F);
    const float height = 410.0F;
    const float left = bounds.left + (bounds.right - bounds.left - width) * 0.5F;
    const float top = std::max(18.0F, bounds.top + (bounds.bottom - bounds.top - height) * 0.24F);
    return D2D1::RectF(left, top, left + width, top + height);
}

// Indique si un point appartient a un rectangle.
bool Contains(const D2D1_RECT_F& rect, float x, float y) {
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

// Convertit une chaine UTF-8 en texte d'affichage minimal.
std::wstring Widen(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(std::max(0, size)), L'\0');
    if (size > 0) {
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), size);
    }
    return result;
}

// Retourne le nom du projet selectionne.
std::wstring ProjectName(const NewSessionOverlayModel& model) {
    if (!model.project_id) {
        return L"Unassigned";
    }
    const auto project = std::ranges::find_if(model.projects, [&model](const Project& candidate) {
        return candidate.id == *model.project_id;
    });
    return project == model.projects.end() ? L"Unassigned" : Widen(project->name);
}

// Retourne le nom du modele selectionne.
std::wstring ModelName(const NewSessionOverlayModel& model) {
    if (!model.model) {
        return L"Default";
    }
    const auto found = std::ranges::find_if(model.models, [&model](const CodexModelInfo& candidate) {
        return candidate.id == *model.model;
    });
    return found == model.models.end() ? Widen(*model.model) : Widen(found->display_name);
}

}  // namespace

// Dessine le formulaire et son champ actif.
void NewSessionOverlayView::Render(
    ID2D1DeviceContext* context,
    const D2D1_RECT_F& bounds,
    const NewSessionOverlayModel& model,
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
    factory->CreateTextFormat(L"Segoe UI Variable Display", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 21.0F, L"", title_format.GetAddressOf());
    factory->CreateTextFormat(L"Segoe UI Variable Text", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0F, L"", text_format.GetAddressOf());
    ComPtr<ID2D1SolidColorBrush> scrim, surface, field, border, text, muted, accent;
    context->CreateSolidColorBrush(palette.window_background, scrim.GetAddressOf());
    context->CreateSolidColorBrush(palette.surface, surface.GetAddressOf());
    context->CreateSolidColorBrush(palette.window_background, field.GetAddressOf());
    context->CreateSolidColorBrush(palette.border, border.GetAddressOf());
    context->CreateSolidColorBrush(palette.text, text.GetAddressOf());
    context->CreateSolidColorBrush(palette.text_muted, muted.GetAddressOf());
    context->CreateSolidColorBrush(palette.accent, accent.GetAddressOf());
    scrim->SetOpacity(0.76F);
    context->FillRectangle(bounds, scrim.Get());

    const D2D1_RECT_F panel_bounds = PanelBounds(bounds);
    const float width = panel_bounds.right - panel_bounds.left;
    const float height = panel_bounds.bottom - panel_bounds.top;
    const float left = panel_bounds.left;
    const float top = panel_bounds.top;
    const D2D1_ROUNDED_RECT panel = D2D1::RoundedRect(D2D1::RectF(left, top, left + width, top + height), kOverlayRadius, kOverlayRadius);
    context->FillRoundedRectangle(panel, surface.Get());
    context->DrawRoundedRectangle(panel, border.Get(), 1.0F);
    context->DrawTextW(L"New session", 11, title_format.Get(), D2D1::RectF(left + 22.0F, top + 18.0F, left + width - 22.0F, top + 52.0F), text.Get());

    const wchar_t* labels[] = {L"Project", L"Workspace", L"Model", L"Prompt"};
    const std::wstring values[] = {ProjectName(model), model.workspace.wstring(), ModelName(model), model.prompt};
    const float heights[] = {42.0F, 42.0F, 42.0F, 92.0F};
    float y = top + 68.0F;
    for (std::size_t index = 0; index < 4; ++index) {
        context->DrawTextW(labels[index], static_cast<UINT32>(std::wcslen(labels[index])), text_format.Get(), D2D1::RectF(left + 22.0F, y + 9.0F, left + 112.0F, y + 34.0F), muted.Get());
        const float input_right = index == 1 ? left + width - 104.0F : left + width - 22.0F;
        const D2D1_ROUNDED_RECT input = D2D1::RoundedRect(D2D1::RectF(left + 116.0F, y, input_right, y + heights[index]), 5.0F, 5.0F);
        context->FillRoundedRectangle(input, field.Get());
        context->DrawRoundedRectangle(input, model.active_field == index ? accent.Get() : border.Get(), model.active_field == index ? 1.25F : 1.0F);
        context->DrawTextW(values[index].c_str(), static_cast<UINT32>(values[index].size()), text_format.Get(), D2D1::RectF(input.rect.left + 12.0F, input.rect.top + 10.0F, input.rect.right - 12.0F, input.rect.bottom - 6.0F), values[index].empty() ? muted.Get() : text.Get());
        if (index == 1) {
            const D2D1_ROUNDED_RECT browse = D2D1::RoundedRect(D2D1::RectF(left + width - 96.0F, y, left + width - 22.0F, y + heights[index]), 5.0F, 5.0F);
            context->FillRoundedRectangle(browse, field.Get());
            context->DrawRoundedRectangle(browse, border.Get(), 1.0F);
            context->DrawTextW(L"Browse", 6, text_format.Get(), D2D1::RectF(browse.rect.left + 12.0F, browse.rect.top + 10.0F, browse.rect.right, browse.rect.bottom), text.Get());
        }
        y += heights[index] + 14.0F;
    }
    if (!model.error.empty()) {
        context->DrawTextW(model.error.c_str(), static_cast<UINT32>(model.error.size()), text_format.Get(), D2D1::RectF(left + 22.0F, top + 350.0F, left + width - 150.0F, top + 382.0F), accent.Get());
    }
    context->DrawTextW(L"Cancel", 6, text_format.Get(), D2D1::RectF(left + width - 180.0F, top + 366.0F, left + width - 112.0F, top + 398.0F), muted.Get());
    context->DrawTextW(model.submitting ? L"Creating..." : L"Create", model.submitting ? 11 : 6, text_format.Get(), D2D1::RectF(left + width - 96.0F, top + 366.0F, left + width - 20.0F, top + 398.0F), accent.Get());
}

// Retourne la zone interactive situee sous un point client.
NewSessionOverlayHitTarget NewSessionOverlayView::HitTest(const D2D1_RECT_F& bounds, float x, float y) const {
    const D2D1_RECT_F panel = PanelBounds(bounds);
    const float heights[] = {42.0F, 42.0F, 42.0F, 92.0F};
    float row_y = panel.top + 68.0F;
    for (std::size_t index = 0; index < 4; ++index) {
        const float right = index == 1 ? panel.right - 104.0F : panel.right - 22.0F;
        if (Contains(D2D1::RectF(panel.left + 116.0F, row_y, right, row_y + heights[index]), x, y)) {
            constexpr NewSessionOverlayHitTarget targets[]{
                NewSessionOverlayHitTarget::Project,
                NewSessionOverlayHitTarget::Workspace,
                NewSessionOverlayHitTarget::Model,
                NewSessionOverlayHitTarget::Prompt,
            };
            return targets[index];
        }
        if (index == 1 && Contains(D2D1::RectF(panel.right - 96.0F, row_y, panel.right - 22.0F, row_y + heights[index]), x, y)) {
            return NewSessionOverlayHitTarget::Browse;
        }
        row_y += heights[index] + 14.0F;
    }
    if (Contains(D2D1::RectF(panel.right - 180.0F, panel.top + 360.0F, panel.right - 108.0F, panel.bottom), x, y)) {
        return NewSessionOverlayHitTarget::Cancel;
    }
    if (Contains(D2D1::RectF(panel.right - 104.0F, panel.top + 360.0F, panel.right, panel.bottom), x, y)) {
        return NewSessionOverlayHitTarget::Create;
    }
    return NewSessionOverlayHitTarget::None;
}
