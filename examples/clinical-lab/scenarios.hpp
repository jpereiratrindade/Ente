#pragma once

#include "clinical_domain.hpp"
#include <vector>
#include <string>

namespace clinicallab {

struct ClinicalScenario {
    std::string id;
    std::string title;
    std::string description;
    bool should_hold_titration_for_safety;
    std::vector<ClinicalObservation> observations;
};

inline std::vector<ClinicalScenario> get_clinical_lab_scenarios() {
    return {
        // C0: Nominal Titration (Hypotension with consistent clear vitals)
        {
            .id = "C0",
            .title = "C0: Nominal Hypotension",
            .description = "PAM = 58 mmHg, sinais vitais consistentes. Titulação permitida.",
            .should_hold_titration_for_safety = false,
            .observations = {
                {.source = "arterial_line", .subject = "map_below_target", .value = "true", .epistemic_tag = "OBSERVED"},
                {.source = "patient_monitor", .subject = "path_clear", .value = "true", .epistemic_tag = "OBSERVED"}
            }
        },
        // C1: Irrelevant Ambient Fluctuation
        {
            .id = "C1",
            .title = "C1: Ambient Temperature Fluctuation",
            .description = "Temperatura da sala oscila levemente. Não deve causar interrupção falsa.",
            .should_hold_titration_for_safety = false,
            .observations = {
                {.source = "arterial_line", .subject = "map_below_target", .value = "true", .epistemic_tag = "OBSERVED"},
                {.source = "patient_monitor", .subject = "path_clear", .value = "true", .epistemic_tag = "OBSERVED"},
                {.source = "room_sensor", .subject = "ambient_temp_shift", .value = "true", .epistemic_tag = "OBSERVED"}
            }
        },
        // C2: Arterial Line vs NIBP Conflict (Explicit Contradiction)
        {
            .id = "C2",
            .title = "C2: Invasive vs Non-Invasive Pressure Contradiction",
            .description = "Cateter arterial diz PAM=55, manguito diz PAM=90. Conflito explícito -> deve suspender titulação!",
            .should_hold_titration_for_safety = true,
            .observations = {
                {.source = "arterial_line", .subject = "map_below_target", .value = "true", .epistemic_tag = "OBSERVED"},
                {.source = "arterial_line", .subject = "path_clear", .value = "true", .epistemic_tag = "OBSERVED"},
                {.source = "nibp_cuff", .subject = "path_clear", .value = "false", .epistemic_tag = "CONTRADICTORY"}
            }
        },
        // C3: Unclassified ECG Morphological Anomaly (Novel Unknown Pattern)
        {
            .id = "C3",
            .title = "C3: Unclassified ECG Waveform Anomaly",
            .description = "Morfologia de QRS anômala não catalogada (classe desconhecida). Deve suspender titulação para segurança!",
            .should_hold_titration_for_safety = true,
            .observations = {
                {.source = "arterial_line", .subject = "map_below_target", .value = "true", .epistemic_tag = "OBSERVED"},
                {.source = "patient_monitor", .subject = "path_clear", .value = "true", .epistemic_tag = "OBSERVED"},
                {.source = "ecg_lead_ii", .subject = "unexpected_motion", .value = "true", .epistemic_tag = "UNKNOWN"}
            }
        },
        // C4: Recalibrated Telemetry -> Resumed Coherence
        {
            .id = "C4",
            .title = "C4: Telemetry Recalibrated & Coherence Restored",
            .description = "Cateter purgado e ECG estabilizado; suporte restabelecido. Pode retomar a titulação.",
            .should_hold_titration_for_safety = false,
            .observations = {
                {.source = "arterial_line", .subject = "map_below_target", .value = "true", .epistemic_tag = "OBSERVED"},
                {.source = "arterial_line", .subject = "path_clear", .value = "true", .epistemic_tag = "OBSERVED"},
                {.source = "all_icu_monitors", .subject = "path_clear", .value = "true", .epistemic_tag = "OBSERVED"}
            }
        }
    };
}

} // namespace clinicallab
