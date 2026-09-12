// ============================================================================
// Codex Glass - Worker de sauvegarde des reglages
// ----------------------------------------------------------------------------
// Ce fichier declare le worker qui regroupe et ecrit les snapshots AppSettings
// hors du thread UI. Le format INI reste exclusivement gere par AppSettings.
// ============================================================================

#pragma once

#include "AppSettings.h"

#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

// ----------------------------------------------------------------------------
// Serialise les sauvegardes INI sur un thread persistant dedie.
// ----------------------------------------------------------------------------
class AppSettingsSaveWorker {
public:
    // ------------------------------------------------------------------------
    // Demarre le thread de sauvegarde sans travail initial.
    // ------------------------------------------------------------------------
    AppSettingsSaveWorker();

    // ------------------------------------------------------------------------
    // Termine les ecritures en attente puis arrete le thread.
    // ------------------------------------------------------------------------
    ~AppSettingsSaveWorker();

    // ------------------------------------------------------------------------
    // Interdit la copie car le worker possede un thread et une synchronisation.
    // ------------------------------------------------------------------------
    AppSettingsSaveWorker(const AppSettingsSaveWorker&) = delete;

    // ------------------------------------------------------------------------
    // Interdit l'affectation par copie pour la meme raison.
    // ------------------------------------------------------------------------
    AppSettingsSaveWorker& operator=(const AppSettingsSaveWorker&) = delete;

    // ------------------------------------------------------------------------
    // Remplace la sauvegarde en attente par le snapshot le plus recent.
    //
    // Parametres :
    // - settings : copie coherente des reglages a persister.
    // ------------------------------------------------------------------------
    void Schedule(const AppSettings& settings);

    // ------------------------------------------------------------------------
    // Attend que toutes les sauvegardes programmees soient terminees.
    // ------------------------------------------------------------------------
    void Flush();

private:
    // ------------------------------------------------------------------------
    // Execute les sauvegardes successives jusqu'a la demande d'arret.
    // ------------------------------------------------------------------------
    void Run();

    // Protege la file, l'activite et la demande d'arret.
    std::mutex mutex_;

    // Reveille le worker ou les appels Flush en attente.
    std::condition_variable condition_;

    // Dernier snapshot demande ; les snapshots intermediaires sont regroupes.
    std::optional<AppSettings> pending_settings_;

    // Indique qu'une ecriture INI est actuellement en cours.
    bool writing_ = false;

    // Indique que le worker doit terminer apres avoir vide sa file.
    bool stopping_ = false;

    // Thread persistant qui execute SaveAppSettings.
    std::thread thread_;
};
