// ============================================================================
// Codex Glass - Donnees calculees de la heatmap de tokens
// ----------------------------------------------------------------------------
// Ce fichier aligne les jours civils locaux en semaines et normalise leurs
// totaux. Il ne lit aucun fichier et ne conserve aucun etat global.
// ============================================================================

#include "TokenHeatmapData.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <limits>
#include <map>
#include <utility>

namespace {

// ----------------------------------------------------------------------------
// Convertit une cle civile locale en midi local pour eviter les heures DST.
//
// Parametres :
// - key : date au format YYYY-MM-DD.
// - value : structure civile recevant la date normalisee.
//
// Retour :
// - true lorsque la cle est valide et convertible.
// ----------------------------------------------------------------------------
bool ParseLocalDate(const std::string& key, std::tm& value) {
    int year = 0;
    int month = 0;
    int day = 0;
    if (key.size() != 10
        || sscanf_s(key.c_str(), "%d-%d-%d", &year, &month, &day) != 3) {
        return false;
    }
    value = {};
    value.tm_year = year - 1900;
    value.tm_mon = month - 1;
    value.tm_mday = day;
    value.tm_hour = 12;
    value.tm_isdst = -1;
    return _mktime64(&value) >= 0;
}

// ----------------------------------------------------------------------------
// Formate une date civile locale normalisee en cle YYYY-MM-DD.
//
// Parametres :
// - value : structure civile normalisee par le runtime.
//
// Retour :
// - cle de date stable.
// ----------------------------------------------------------------------------
std::string FormatLocalDate(const std::tm& value) {
    char key[11]{};
    std::snprintf(
        key,
        sizeof(key),
        "%04d-%02d-%02d",
        value.tm_year + 1900,
        value.tm_mon + 1,
        value.tm_mday
    );
    return key;
}

// ----------------------------------------------------------------------------
// Decale une date civile d'un nombre de jours et la renormalise a midi.
//
// Parametres :
// - value : date locale de depart.
// - day_offset : nombre signe de jours a appliquer.
//
// Retour :
// - date locale normalisee apres decalage.
// ----------------------------------------------------------------------------
std::tm ShiftLocalDate(std::tm value, int day_offset) {
    value.tm_mday += day_offset;
    value.tm_hour = 12;
    value.tm_isdst = -1;
    const __time64_t shifted = _mktime64(&value);
    std::tm normalized{};
    _localtime64_s(&normalized, &shifted);
    return normalized;
}

} // namespace

// ----------------------------------------------------------------------------
// Calcule un niveau lineaire entre les bornes non nulles visibles.
//
// Parametres :
// - value : total de tokens de la cellule.
// - minimum : minimum non nul de la grille visible.
// - maximum : maximum non nul de la grille visible.
//
// Retour :
// - zero pour une cellule vide, sinon un niveau de un a quatre.
// ----------------------------------------------------------------------------
std::uint8_t CalculateTokenHeatmapLevel(
    std::uint64_t value,
    std::uint64_t minimum,
    std::uint64_t maximum
) {
    if (value == 0 || maximum == 0) {
        return 0;
    }
    if (maximum <= minimum) {
        return kTokenHeatmapActiveLevelCount;
    }
    const double ratio = static_cast<double>(value - std::min(value, minimum))
        / static_cast<double>(maximum - minimum);
    return static_cast<std::uint8_t>(std::clamp(
        1 + static_cast<int>(ratio * kTokenHeatmapActiveLevelCount),
        1,
        static_cast<int>(kTokenHeatmapActiveLevelCount)
    ));
}

// ----------------------------------------------------------------------------
// Construit une grille terminee par la semaine du dernier jour disponible.
//
// Parametres :
// - daily : serie quotidienne ordonnee avec ses jours a zero.
// - week_count : nombre de colonnes de semaines a produire.
//
// Retour :
// - cellules ordonnees par colonne puis du lundi au dimanche.
// ----------------------------------------------------------------------------
std::vector<TokenHeatmapCell> BuildTokenHeatmapCells(
    const std::vector<TokenDailyUsage>& daily,
    std::size_t week_count
) {
    if (daily.empty() || week_count == 0) {
        return {};
    }
    std::tm current{};
    if (!ParseLocalDate(daily.back().date_key, current)) {
        return {};
    }
    const int current_row = (current.tm_wday + 6) % 7;
    const std::tm current_week_monday = ShiftLocalDate(current, -current_row);
    const int first_day_offset = -static_cast<int>((week_count - 1) * kTokenHeatmapRowCount);
    const std::tm first_monday = ShiftLocalDate(current_week_monday, first_day_offset);

    std::map<std::string, const TokenDailyUsage*> indexed_days;
    for (const TokenDailyUsage& day : daily) {
        indexed_days[day.date_key] = &day;
    }
    const std::string coverage_first = daily.front().date_key;
    const std::string coverage_last = daily.back().date_key;
    std::vector<TokenHeatmapCell> cells;
    cells.reserve(week_count * kTokenHeatmapRowCount);
    std::uint64_t visible_minimum = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t visible_maximum = 0;
    for (std::size_t column = 0; column < week_count; ++column) {
        for (std::size_t row = 0; row < kTokenHeatmapRowCount; ++row) {
            const int offset = static_cast<int>(
                (column * kTokenHeatmapRowCount) + row
            );
            const std::string key = FormatLocalDate(ShiftLocalDate(first_monday, offset));
            TokenHeatmapCell cell{};
            cell.date_key = key;
            cell.column = column;
            cell.row = row;
            cell.present = key >= coverage_first && key <= coverage_last;
            const auto day = indexed_days.find(key);
            if (cell.present && day != indexed_days.end()) {
                cell.total_tokens = day->second->counts.Total();
                if (cell.total_tokens > 0) {
                    visible_minimum = std::min(visible_minimum, cell.total_tokens);
                    visible_maximum = std::max(visible_maximum, cell.total_tokens);
                }
            }
            cells.push_back(std::move(cell));
        }
    }
    for (TokenHeatmapCell& cell : cells) {
        cell.level = cell.present
            ? CalculateTokenHeatmapLevel(
                cell.total_tokens,
                visible_minimum == std::numeric_limits<std::uint64_t>::max()
                    ? 0
                    : visible_minimum,
                visible_maximum
            )
            : 0;
    }
    return cells;
}

// ----------------------------------------------------------------------------
// Projette les dernieres tranches de cinq minutes dans une grille lineaire.
// ----------------------------------------------------------------------------
std::vector<TokenHeatmapCell> BuildTokenFiveMinuteHeatmapCells(
    const std::vector<TokenFiveMinuteUsage>& buckets,
    std::size_t cell_count
) {
    if (cell_count == 0) {
        return {};
    }
    const std::size_t visible_count = std::min(cell_count, buckets.size());
    const std::size_t first_bucket = buckets.size() - visible_count;
    const std::size_t leading_empty = cell_count - visible_count;
    std::vector<TokenHeatmapCell> cells(cell_count);
    std::uint64_t visible_minimum = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t visible_maximum = 0;
    for (std::size_t index = 0; index < visible_count; ++index) {
        const TokenFiveMinuteUsage& bucket = buckets[first_bucket + index];
        TokenHeatmapCell& cell = cells[leading_empty + index];
        cell.bucket_start = bucket.bucket_start;
        cell.total_tokens = bucket.counts.Total();
        cell.present = true;
        if (cell.total_tokens > 0) {
            visible_minimum = std::min(visible_minimum, cell.total_tokens);
            visible_maximum = std::max(visible_maximum, cell.total_tokens);
        }
    }
    for (TokenHeatmapCell& cell : cells) {
        cell.level = cell.present
            ? CalculateTokenHeatmapLevel(
                cell.total_tokens,
                visible_minimum == std::numeric_limits<std::uint64_t>::max()
                    ? 0
                    : visible_minimum,
                visible_maximum
            )
            : 0;
    }
    return cells;
}
