// ============================================================================
// Codex Deck - Implementation des raccourcis clavier applicatifs
// ----------------------------------------------------------------------------
// Ce fichier garde le mapping clavier centralise afin que DeckApp et les tests
// utilisent les memes commandes sans lancer d'action concrete.
// ============================================================================

#include "KeyboardShortcuts.h"

// ----------------------------------------------------------------------------
// Traduit un accord clavier en commande applicative.
// ----------------------------------------------------------------------------
std::optional<DeckCommandKind> TranslateShortcut(const KeyChord& chord) {
    if (chord.control && !chord.shift && chord.virtual_key == 'K') {
        return DeckCommandKind::OpenCommandPalette;
    }
    if (chord.control && !chord.shift && chord.virtual_key == 'P') {
        return DeckCommandKind::OpenSearch;
    }
    if (chord.control && !chord.shift && chord.virtual_key == 'N') {
        return DeckCommandKind::NewSession;
    }
    if (chord.control && chord.shift && chord.virtual_key == 'N') {
        return DeckCommandKind::NewSessionInCurrentProject;
    }
    if (!chord.control && !chord.shift && chord.virtual_key == VK_F2) {
        return DeckCommandKind::RenameThread;
    }
    if (!chord.control && !chord.shift && chord.virtual_key == VK_DELETE) {
        return DeckCommandKind::ArchiveThread;
    }
    if (chord.control && chord.shift && chord.virtual_key == 'D') {
        return DeckCommandKind::DetachWorkbench;
    }
    if (chord.control && !chord.shift && chord.virtual_key >= '1' && chord.virtual_key <= '9') {
        return DeckCommandKind::OpenRuntimeSlot;
    }
    if (chord.control && !chord.shift && chord.virtual_key == VK_TAB) {
        return DeckCommandKind::CycleRuntimeSession;
    }
    return std::nullopt;
}
