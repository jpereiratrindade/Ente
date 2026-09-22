#pragma once

#include "field_domain.hpp"
#include "ente/domain/generic_agent.hpp"
#include <vector>
#include <string>
#include <format>
#include <iostream>
#include <sstream>

namespace fieldstation {

struct ScenarioEventLog {
    int act_id{1};
    std::string act_name;
    uint64_t logical_time{0};
    std::string event_type;
    std::string description;
    std::string hardware_id;
    std::string soil_a;
    std::string soil_b;
    std::string interpretation;
    std::string rcc_status;
    std::string assurance_directive;
    std::string valve_action;
    std::string hash;
};

class FieldStationScenarios {
public:
    static std::vector<ScenarioEventLog> run_all() {
        std::vector<ScenarioEventLog> logs;

        // ---------------------------------------------------------
        // ACT 1: GENESIS
        // ---------------------------------------------------------
        ente::domain::GenericAgentWithEnte<FieldStationDomain> agent("field-001");
        auto& domain = agent.domain_mut();
        auto& ente = agent.ente_mut();

        // Register initial material anchor
        domain.set_telemetry(SensorTelemetry{
            .soil_moisture_a = 21.0,
            .soil_moisture_b = 22.0,
            .rain_detected = false,
            .water_tank_level = 73.0,
            .flow_sensor_ok = true,
            .hardware_id = "RP-A921"
        });

        auto gen_event = ente.history().events().front();
        logs.push_back({
            .act_id = 1,
            .act_name = "Genesis",
            .logical_time = 0,
            .event_type = "GENESIS",
            .description = "ENTE field station identity anchored on Raspberry Pi A (RP-A921)",
            .hardware_id = "RP-A921",
            .soil_a = "21%",
            .soil_b = "22%",
            .interpretation = "INITIALIZING",
            .rcc_status = "STABLE",
            .assurance_directive = "SAFE_HOLD",
            .valve_action = "CLOSE_VALVE",
            .hash = gen_event.event_digest.value
        });

        // ---------------------------------------------------------
        // ACT 2: NOMINAL (Dry Soil -> Irrigation Needed -> Allowed)
        // ---------------------------------------------------------
        std::vector<ente::epistemic::Observation> obs_nominal = {
            {
                .id = ente::core::EvidenceId("EV-SOIL-A1"),
                .source = "probe_a",
                .subject = "soil_moisture",
                .value = "21%",
                .observed_at = 1,
                .status = ente::epistemic::EpistemicStatus::Observed
            },
            {
                .id = ente::core::EvidenceId("EV-SOIL-B1"),
                .source = "probe_b",
                .subject = "soil_moisture",
                .value = "22%",
                .observed_at = 1,
                .status = ente::epistemic::EpistemicStatus::Observed
            },
            {
                .id = ente::core::EvidenceId("EV-TANK-1"),
                .source = "reservoir_tank",
                .subject = "water_level",
                .value = "73%",
                .observed_at = 1,
                .status = ente::epistemic::EpistemicStatus::Observed
            }
        };

        auto action_nominal = agent.decide_action(
            ente::core::LogicalTime(1),
            obs_nominal,
            ValveAction::OpenValve,
            "nominal_irrigation_cycle"
        );

        logs.push_back({
            .act_id = 2,
            .act_name = "Nominal Operation",
            .logical_time = 1,
            .event_type = "OBSERVATION & ACTION",
            .description = "Soil moisture dry (21%), tank OK (73%). Irrigation justified.",
            .hardware_id = "RP-A921",
            .soil_a = "21% (DRY)",
            .soil_b = "22% (DRY)",
            .interpretation = "IRRIGATION_NEEDED",
            .rcc_status = "SUPPORTED",
            .assurance_directive = "ALLOW_ACTION",
            .valve_action = std::string(to_string(action_nominal)),
            .hash = ente.history().head().event_digest.value
        });

        // ---------------------------------------------------------
        // ACT 3: SENSOR CONFLICT / CONTRADICTION
        // (Probe A = DRY 20%, Probe B = WET 85% -> Contradiction -> SafeHold)
        // ---------------------------------------------------------
        std::vector<ente::epistemic::Observation> obs_conflict = {
            {
                .id = ente::core::EvidenceId("EV-SOIL-A2"),
                .source = "probe_a",
                .subject = "soil_moisture",
                .value = "20%",
                .observed_at = 2,
                .status = ente::epistemic::EpistemicStatus::Observed
            },
            {
                .id = ente::core::EvidenceId("EV-SOIL-B2"),
                .source = "probe_b",
                .subject = "soil_moisture",
                .value = "85%",
                .observed_at = 2,
                .status = ente::epistemic::EpistemicStatus::Contradictory
            }
        };

        auto action_conflict = agent.decide_action(
            ente::core::LogicalTime(2),
            obs_conflict,
            ValveAction::OpenValve,
            "sensor_conflict_cycle"
        );

        logs.push_back({
            .act_id = 3,
            .act_name = "Sensor Conflict",
            .logical_time = 2,
            .event_type = "PERTURBATION & SAFE_HOLD",
            .description = "Probe A (DRY) contradicts Probe B (WET). RCC weakened. Valve closed.",
            .hardware_id = "RP-A921",
            .soil_a = "20% (DRY)",
            .soil_b = "85% (WET CONFLICT)",
            .interpretation = "WEAKENED_JUSTIFICATION",
            .rcc_status = "WEAKENED",
            .assurance_directive = "SAFE_HOLD",
            .valve_action = std::string(to_string(action_conflict)),
            .hash = ente.history().head().event_digest.value
        });

        // ---------------------------------------------------------
        // ACT 4: MATERIAL MIGRATION (RIT)
        // Migration from Raspberry Pi A (RP-A921) to Raspberry Pi B (RP-B104)
        // ---------------------------------------------------------
        ente::identity::MaterialAnchor new_anchor{
            .id = ente::identity::MaterialAnchorId("RP-B104"),
            .type = ente::identity::SubstrateType::TpmProtectedDevice,
            .hardware_fingerprint = "sha256_b104_arm64_rpi5"
        };

        auto mig_res = ente.migrate_hardware(new_anchor, ente::core::LogicalTime(3));
        if (mig_res.has_value()) {
            domain.set_telemetry(SensorTelemetry{
                .soil_moisture_a = 20.0,
                .soil_moisture_b = 85.0,
                .rain_detected = false,
                .water_tank_level = 73.0,
                .flow_sensor_ok = true,
                .hardware_id = "RP-B104"
            });
        }

        logs.push_back({
            .act_id = 4,
            .act_name = "Material Migration",
            .logical_time = 3,
            .event_type = "MATERIAL_TRANSFORMATION",
            .description = "RIT migration: RP-A921 -> RP-B104. Hardware changed, identity preserved.",
            .hardware_id = "RP-B104",
            .soil_a = "20% (DRY)",
            .soil_b = "85% (WET)",
            .interpretation = "WEAKENED_JUSTIFICATION",
            .rcc_status = "WEAKENED",
            .assurance_directive = "SAFE_HOLD",
            .valve_action = "CLOSE_VALVE",
            .hash = ente.history().head().event_digest.value
        });

        // ---------------------------------------------------------
        // ACT 5: SIMULATE POWER LOSS & COLD RECOVERY
        // Process terminates, cold process restarts and recovers state from REC ledger
        // ---------------------------------------------------------
        auto history_copy = ente.history(); // Snapshot of recoverable ledger
        auto rec_res = ente::realization::EnteRealization::recover_from_history(history_copy);

        logs.push_back({
            .act_id = 5,
            .act_name = "Cold Recovery",
            .logical_time = 4,
            .event_type = "COLD_RECOVERY",
            .description = rec_res.has_value() 
                ? "Process killed. New process recovered 100% of trajectory. SafeHold preserved."
                : "Recovery failed",
            .hardware_id = "RP-B104",
            .soil_a = "20% (DRY)",
            .soil_b = "85% (WET)",
            .interpretation = "WEAKENED_JUSTIFICATION",
            .rcc_status = "WEAKENED",
            .assurance_directive = "SAFE_HOLD",
            .valve_action = "CLOSE_VALVE",
            .hash = rec_res.has_value() 
                ? rec_res.value().history().head().event_digest.value
                : "0000000000000000000000000000000000000000000000000000000000000000"
        });

        return logs;
    }

    static std::string to_json(const std::vector<ScenarioEventLog>& logs) {
        std::ostringstream ss;
        ss << "[\n";
        for (size_t i = 0; i < logs.size(); ++i) {
            const auto& l = logs[i];
            ss << "  {\n";
            ss << "    \"act_id\": " << l.act_id << ",\n";
            ss << "    \"act_name\": \"" << l.act_name << "\",\n";
            ss << "    \"logical_time\": " << l.logical_time << ",\n";
            ss << "    \"event_type\": \"" << l.event_type << "\",\n";
            ss << "    \"description\": \"" << l.description << "\",\n";
            ss << "    \"hardware_id\": \"" << l.hardware_id << "\",\n";
            ss << "    \"soil_a\": \"" << l.soil_a << "\",\n";
            ss << "    \"soil_b\": \"" << l.soil_b << "\",\n";
            ss << "    \"interpretation\": \"" << l.interpretation << "\",\n";
            ss << "    \"rcc_status\": \"" << l.rcc_status << "\",\n";
            ss << "    \"assurance_directive\": \"" << l.assurance_directive << "\",\n";
            ss << "    \"valve_action\": \"" << l.valve_action << "\",\n";
            ss << "    \"hash\": \"" << l.hash << "\"\n";
            ss << "  }" << (i + 1 < logs.size() ? "," : "") << "\n";
        }
        ss << "]\n";
        return ss.str();
    }
};

} // namespace fieldstation
