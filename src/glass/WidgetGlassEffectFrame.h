// ============================================================================
// Codex Glass - Frame GlassEffect
// ----------------------------------------------------------------------------
// Ce fichier declare le format de frame partage entre la capture GlassEffect et le
// renderer Direct2D, sans exposer les types D3D11/DXGI au reste de l'app.
// ============================================================================

#pragma once

#include <cstdint>
#include <vector>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;

// ----------------------------------------------------------------------------
// Snapshot BGRA du fond capture autour du widget, GPU puis CPU a la demande.
// ----------------------------------------------------------------------------
struct WidgetGlassEffectFrame {
    // Largeur du snapshot en pixels.
    uint32_t width = 0;

    // Hauteur du snapshot en pixels.
    uint32_t height = 0;

    // Nombre d'octets entre deux lignes.
    uint32_t stride = 0;

    // Identifiant incrementiel permettant au renderer de recreer son bitmap.
    uint64_t generation = 0;

    // Pixels BGRA 8 bits, remplis uniquement apres une lecture GPU necessaire.
    std::vector<uint8_t> pixels;

    // Texture D3D11 source non possedee correspondant exactement aux pixels.
    ID3D11Texture2D* gpu_texture = nullptr;

    // Device D3D11 non possede ayant cree la texture source.
    ID3D11Device* gpu_device = nullptr;

    // Contexte immediat D3D11 non possede associe au device source.
    ID3D11DeviceContext* gpu_context = nullptr;
};
