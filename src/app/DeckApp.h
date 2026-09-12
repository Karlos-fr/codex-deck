// ============================================================================
// Codex Deck - Orchestration applicative
// ----------------------------------------------------------------------------
// Ce module pilote le cycle de vie Win32 de la coquille. Il delegue la creation
// de fenetre a DeckWindow et le dessin a DeckRenderer.
// ============================================================================

#pragma once

#include "../codex/CodexSupervisor.h"
#include "../model/SessionCatalog.h"
#include "../rendering/DeckRenderer.h"
#include "../settings/DeckSettings.h"
#include "../theme/Theme.h"

#include <windows.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

// ----------------------------------------------------------------------------
// Application native principale de Codex Deck.
// ----------------------------------------------------------------------------
class DeckApp {
public:
    // ------------------------------------------------------------------------
    // Libere les ressources applicatives possedees.
    // ------------------------------------------------------------------------
    ~DeckApp();

    // ------------------------------------------------------------------------
    // Lance la boucle de messages de l'application.
    //
    // Parametres :
    // - instance : instance du module executable.
    // - command_show : mode d'affichage initial demande par Windows.
    //
    // Retour :
    // - code de sortie du processus.
    // ------------------------------------------------------------------------
    int Run(HINSTANCE instance, int command_show);

    // ------------------------------------------------------------------------
    // Procedure Win32 statique redirigeant vers l'instance applicative.
    // ------------------------------------------------------------------------
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

private:
    // ------------------------------------------------------------------------
    // Traite un message Win32 pour la fenetre principale.
    // ------------------------------------------------------------------------
    LRESULT HandleWindowMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

    // ------------------------------------------------------------------------
    // Recalcule le theme courant et applique les attributs systeme.
    // ------------------------------------------------------------------------
    void RefreshTheme(HWND hwnd);

    // ------------------------------------------------------------------------
    // Demarre le worker de stockage cache-first.
    // ------------------------------------------------------------------------
    void StartStorageWorker();

    // ------------------------------------------------------------------------
    // Arrete le worker de stockage cache-first.
    // ------------------------------------------------------------------------
    void StopStorageWorker();

    // ------------------------------------------------------------------------
    // Demande une reconciliation Codex/local au worker de stockage.
    // ------------------------------------------------------------------------
    void RequestSessionRefresh();

    // ------------------------------------------------------------------------
    // Rafraichit le catalogue depuis un client Codex connecte.
    //
    // Parametres :
    // - client : client app-server connecte.
    // ------------------------------------------------------------------------
    void RefreshSessionsFromCodex(CodexClient& client);

    // Renderer Direct2D minimal de la coquille.
    DeckRenderer renderer_;

    // Superviseur worker de l'unique app-server Codex.
    CodexSupervisor codex_supervisor_;

    // Etat visuel affiche par le renderer.
    DeckVisualState visual_state_{};

    // Catalogue local publie par le worker de stockage.
    SessionCatalog session_catalog_;

    // Reglages locaux de la coquille.
    DeckSettings settings_{};

    // Theme concret actuellement applique.
    ResolvedTheme resolved_theme_ = ResolvedTheme::Light;

    // Worker responsable de SQLite et de la synchronisation cache-first.
    std::jthread storage_worker_;

    // Protege les demandes de refresh destinees au worker de stockage.
    std::mutex storage_mutex_;

    // Reveille le worker lorsqu'une sync est demandee ou lors de l'arret.
    std::condition_variable storage_condition_;

    // Indique qu'un refresh Codex/local est demande.
    bool storage_refresh_requested_ = false;

    // Indique que le worker de stockage doit s'arreter.
    std::atomic_bool storage_stopping_ = false;
};
