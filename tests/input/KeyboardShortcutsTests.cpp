// ============================================================================
// Codex Deck - Tests des raccourcis clavier
// ----------------------------------------------------------------------------
// Ce fichier valide la traduction pure des accords clavier vers les commandes
// applicatives, sans fenetre Win32 ni session Codex reelle.
// ============================================================================

#include "input/KeyboardShortcuts.h"

namespace {

// ----------------------------------------------------------------------------
// Verifie qu'un raccourci produit la commande attendue.
//
// Parametres :
// - chord : accord source.
// - expected : commande attendue.
//
// Retour :
// - true si la traduction correspond.
// ----------------------------------------------------------------------------
bool MapsTo(const KeyChord& chord, DeckCommandKind expected) {
    const auto command = TranslateShortcut(chord);
    return command.has_value() && *command == expected;
}

}  // namespace

// ----------------------------------------------------------------------------
// Lance les validations de mapping clavier.
//
// Retour :
// - zero si tous les raccourcis de navigation sont traduits.
// ----------------------------------------------------------------------------
int main() {
    if (!MapsTo(KeyChord{'K', true, false}, DeckCommandKind::OpenCommandPalette)) {
        return 1;
    }
    if (!MapsTo(KeyChord{'P', true, false}, DeckCommandKind::OpenSearch)) {
        return 2;
    }
    if (!MapsTo(KeyChord{'N', true, false}, DeckCommandKind::NewSession)) {
        return 3;
    }
    if (!MapsTo(KeyChord{'N', true, true}, DeckCommandKind::NewSessionInCurrentProject)) {
        return 4;
    }
    if (!MapsTo(KeyChord{VK_F2, false, false}, DeckCommandKind::RenameThread)) {
        return 5;
    }
    if (!MapsTo(KeyChord{VK_DELETE, false, false}, DeckCommandKind::ArchiveThread)) {
        return 6;
    }
    if (!MapsTo(KeyChord{'D', true, true}, DeckCommandKind::DetachWorkbench)) {
        return 7;
    }
    if (!MapsTo(KeyChord{'3', true, false}, DeckCommandKind::OpenRuntimeSlot)) {
        return 8;
    }
    if (!MapsTo(KeyChord{VK_TAB, true, false}, DeckCommandKind::CycleRuntimeSession)) {
        return 9;
    }
    return TranslateShortcut(KeyChord{'X', false, false}).has_value() ? 10 : 0;
}
