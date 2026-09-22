#pragma once

#include "case_domain.hpp"
#include <vector>
#include <string>

namespace caselab {

struct CaseScenario {
    std::string id;
    std::string title;
    std::string description;
    bool should_hold_for_safety; // Ground truth
    std::vector<CaseObservation> observations;
};

inline std::vector<CaseScenario> get_kitkat_lab_scenarios() {
    return {
        // S0: Nominal departure
        {
            .id = "S0",
            .title = "S0: Nominal Departure",
            .description = "Embarque concluído, área 100% livre. O veículo deve partir.",
            .should_hold_for_safety = false,
            .observations = {
                {.source = "cabin_sensor", .subject = "pickup_complete", .value = "true", .epistemic_tag = "OBSERVED"},
                {.source = "front_cam", .subject = "path_clear", .value = "true", .epistemic_tag = "OBSERVED"}
            }
        },
        // S1: Irrelevant novelty (plastic bag blowing in the wind far from departure path)
        {
            .id = "S1",
            .title = "S1: Irrelevant Distant Novelty",
            .description = "Saco plástico flutuando a 15m da lateral. Não deve causar paralisia falsa.",
            .should_hold_for_safety = false,
            .observations = {
                {.source = "cabin_sensor", .subject = "pickup_complete", .value = "true", .epistemic_tag = "OBSERVED"},
                {.source = "front_cam", .subject = "path_clear", .value = "true", .epistemic_tag = "OBSERVED"},
                {.source = "far_cam", .subject = "distant_wind_object", .value = "true", .epistemic_tag = "OBSERVED"}
            }
        },
        // S2: KitKat Part 1 - Unclassified motion in critical blindzone near wheels
        {
            .id = "S2",
            .title = "S2: Near-Wheel Anomaly (KitKat Analog Pt. 1)",
            .description = "Movimento não classificado próximo à roda dianteira (classe desconhecida). Deve suspender a partida!",
            .should_hold_for_safety = true,
            .observations = {
                {.source = "cabin_sensor", .subject = "pickup_complete", .value = "true", .epistemic_tag = "OBSERVED"},
                {.source = "front_cam", .subject = "path_clear", .value = "true", .epistemic_tag = "OBSERVED"},
                {.source = "wheel_ground_lidar", .subject = "unexpected_motion", .value = "true", .epistemic_tag = "UNKNOWN"} // UNKNOWN class!
            }
        },
        // S3: Sensor Contradiction
        {
            .id = "S3",
            .title = "S3: Sensor Contradiction",
            .description = "Câmera frontal diz livre, sensor de solo próximo indica ocupado. Conflito explícito -> deve suspender!",
            .should_hold_for_safety = true,
            .observations = {
                {.source = "cabin_sensor", .subject = "pickup_complete", .value = "true", .epistemic_tag = "OBSERVED"},
                {.source = "front_cam", .subject = "path_clear", .value = "true", .epistemic_tag = "OBSERVED"},
                {.source = "ground_proximity_sensor", .subject = "path_clear", .value = "false", .epistemic_tag = "CONTRADICTORY"}
            }
        },
        // S4: KitKat Part 2 - Human crouched near vehicle envelope
        {
            .id = "S4",
            .title = "S4: Human Crouched Near Wheel (KitKat Analog Pt. 2)",
            .description = "Pessoa agachada rente à lateral tentando resgatar animal. Área frontal desobstruída, mas partida é perigosa!",
            .should_hold_for_safety = true,
            .observations = {
                {.source = "cabin_sensor", .subject = "pickup_complete", .value = "true", .epistemic_tag = "OBSERVED"},
                {.source = "front_cam", .subject = "path_clear", .value = "true", .epistemic_tag = "OBSERVED"},
                {.source = "side_cam", .subject = "unexpected_motion", .value = "true", .epistemic_tag = "UNKNOWN"}
            }
        },
        // S5: Resolution -> Area Clear
        {
            .id = "S5",
            .title = "S5: Evidence Recovery -> Path Fully Cleared",
            .description = "Pessoa e animal se afastam; verificação confirma caminho livre. O veículo pode retomar a partida.",
            .should_hold_for_safety = false,
            .observations = {
                {.source = "cabin_sensor", .subject = "pickup_complete", .value = "true", .epistemic_tag = "OBSERVED"},
                {.source = "front_cam", .subject = "path_clear", .value = "true", .epistemic_tag = "OBSERVED"},
                {.source = "all_perimeter_sensors", .subject = "path_clear", .value = "true", .epistemic_tag = "OBSERVED"}
            }
        },
        // S6: Resolution -> Occupied underbody
        {
            .id = "S6",
            .title = "S6: Evidence Recovery -> Underbody Occupied",
            .description = "Sensor sob o chassi confirma presença de objeto estranho. Deve manter a suspensão!",
            .should_hold_for_safety = true,
            .observations = {
                {.source = "cabin_sensor", .subject = "pickup_complete", .value = "true", .epistemic_tag = "OBSERVED"},
                {.source = "front_cam", .subject = "path_clear", .value = "true", .epistemic_tag = "OBSERVED"},
                {.source = "underbody_sensor", .subject = "unexpected_motion", .value = "true", .epistemic_tag = "UNKNOWN"}
            }
        }
    };
}

} // namespace caselab
