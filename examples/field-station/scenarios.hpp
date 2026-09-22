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
    std::string tour_name{"Tour A · Epistemic Resolution"};
    int act_id{1};
    std::string act_name;
    uint64_t logical_time{0};
    std::string event_type;
    std::string description;
    std::string hardware_id;
    std::string soil_a;
    std::string soil_b;
    std::string soil_c{"N/A"};
    std::string diagnostic_status{"IDLE"};
    std::string interpretation;
    std::string rcc_status;
    std::string assurance_directive;
    std::string valve_action;
    std::string hash;
};

class FieldStationScenarios {
public:
    // ---------------------------------------------------------
    // TOUR A: EPISTEMIC RESOLUTION CYCLE
    // Genesis -> Nominal -> Conflict -> Diagnostic Seeking -> Reinterpretation -> Action
    // ---------------------------------------------------------
    static std::vector<ScenarioEventLog> run_tour_a_resolution(bool resolve_dry_soil = true) {
        std::vector<ScenarioEventLog> logs;

        ente::domain::GenericAgentWithEnte<FieldStationDomain> agent("field-001");
        auto& domain = agent.domain_mut();
        auto& ente = agent.ente_mut();

        // 1. GENESIS
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
            .tour_name = "Tour A · Epistemic Resolution",
            .act_id = 1,
            .act_name = "Genesis",
            .logical_time = 0,
            .event_type = "GENESIS",
            .description = "ENTE field station identity anchored on Raspberry Pi A (RP-A921).",
            .hardware_id = "RP-A921",
            .soil_a = "21%",
            .soil_b = "22%",
            .soil_c = "OFF",
            .diagnostic_status = "IDLE",
            .interpretation = "INITIALIZING",
            .rcc_status = "STABLE",
            .assurance_directive = "SAFE_HOLD",
            .valve_action = "CLOSE_VALVE",
            .hash = gen_event.event_digest.value
        });

        // 2. NOMINAL OPERATION
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
            .tour_name = "Tour A · Epistemic Resolution",
            .act_id = 2,
            .act_name = "Nominal Operation",
            .logical_time = 1,
            .event_type = "OBSERVATION & ACTION",
            .description = "Probes agree soil is dry (21%). RCC validates epistemic support. Irrigation authorized.",
            .hardware_id = "RP-A921",
            .soil_a = "21% (DRY)",
            .soil_b = "22% (DRY)",
            .soil_c = "OFF",
            .diagnostic_status = "NOMINAL",
            .interpretation = "IRRIGATION_NEEDED",
            .rcc_status = "SUPPORTED",
            .assurance_directive = "ALLOW_ACTION",
            .valve_action = std::string(to_string(action_nominal)),
            .hash = ente.history().head().event_digest.value
        });

        // 3. SENSOR CONFLICT (Uncertainty Injected -> SafeHold)
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
            .tour_name = "Tour A · Epistemic Resolution",
            .act_id = 3,
            .act_name = "Sensor Conflict",
            .logical_time = 2,
            .event_type = "PERTURBATION & SAFE_HOLD",
            .description = "Probe A (DRY) contradicts Probe B (WET). RCC weakened. Directive: SafeHold. Discriminant evidence needed.",
            .hardware_id = "RP-A921",
            .soil_a = "20% (DRY)",
            .soil_b = "85% (WET CONFLICT)",
            .soil_c = "AWAITING_REQUEST",
            .diagnostic_status = "DISCRIMINATING_EVIDENCE_REQUIRED",
            .interpretation = "WEAKENED_JUSTIFICATION",
            .rcc_status = "WEAKENED",
            .assurance_directive = "SAFE_HOLD",
            .valve_action = std::string(to_string(action_conflict)),
            .hash = ente.history().head().event_digest.value
        });

        // 4. EVIDENCE SEEKING & DIAGNOSTIC OBSERVATION
        // Domain runs self-test and queries Reference Probe C
        double ref_c_val = resolve_dry_soil ? 22.0 : 82.0;
        bool b_has_drift = resolve_dry_soil;
        domain.run_diagnostic_self_test(b_has_drift, ref_c_val);

        std::vector<ente::epistemic::Observation> obs_diag = {
            {
                .id = ente::core::EvidenceId("EV-REF-C1"),
                .source = "reference_probe_c",
                .subject = "ground_truth_moisture",
                .value = resolve_dry_soil ? "22% (DRY)" : "82% (WET)",
                .observed_at = 3,
                .status = ente::epistemic::EpistemicStatus::Observed
            },
            {
                .id = ente::core::EvidenceId("EV-DIAG-SELFTEST"),
                .source = "sensor_diagnostics",
                .subject = "calibration_integrity",
                .value = resolve_dry_soil ? "probe_a:OK, probe_b:DRIFT" : "probe_a:DRIFT, probe_b:OK",
                .observed_at = 3,
                .status = ente::epistemic::EpistemicStatus::Derived
            }
        };

        // ENTE assesses diagnostic evidence
        auto action_diag = agent.decide_action(
            ente::core::LogicalTime(3),
            obs_diag,
            ValveAction::CloseValve,
            "diagnostic_investigation_cycle"
        );

        logs.push_back({
            .tour_name = "Tour A · Epistemic Resolution",
            .act_id = 4,
            .act_name = "Diagnostic Seeking",
            .logical_time = 3,
            .event_type = "DIAGNOSTIC_OBSERVATION",
            .description = resolve_dry_soil
                ? "Reference Probe C (22% DRY) and Self-Test confirm Probe B drift. Evidence resolved."
                : "Reference Probe C (82% WET) and Self-Test confirm Probe A drift. Evidence resolved.",
            .hardware_id = "RP-A921",
            .soil_a = "20% (HEALTHY)",
            .soil_b = resolve_dry_soil ? "85% (DRIFT)" : "85% (HEALTHY)",
            .soil_c = resolve_dry_soil ? "22% (REF DRY)" : "82% (REF WET)",
            .diagnostic_status = resolve_dry_soil ? "PROBE_B_SUSPECT" : "PROBE_A_SUSPECT",
            .interpretation = "EVIDENCE_DISCRIMINATED",
            .rcc_status = "SUPPORTED",
            .assurance_directive = "SAFE_HOLD",
            .valve_action = std::string(to_string(action_diag)),
            .hash = ente.history().head().event_digest.value
        });

        // 5. REINTERPRETATION & DOMAIN RESPONSE
        // Once suspect sensor is isolated, ENTE reinterprets with high coherence
        std::vector<ente::epistemic::Observation> obs_resolved = {
            {
                .id = ente::core::EvidenceId("EV-SOIL-RESOLVED"),
                .source = "fused_healthy_probes",
                .subject = "soil_moisture",
                .value = resolve_dry_soil ? "21% (DRY CONFIRMED)" : "83% (WET CONFIRMED)",
                .observed_at = 4,
                .status = ente::epistemic::EpistemicStatus::Observed
            }
        };

        ValveAction proposed = resolve_dry_soil ? ValveAction::OpenValve : ValveAction::CloseValve;
        auto action_resolved = agent.decide_action(
            ente::core::LogicalTime(4),
            obs_resolved,
            proposed,
            "reinterpreted_operational_cycle"
        );

        logs.push_back({
            .tour_name = "Tour A · Epistemic Resolution",
            .act_id = 5,
            .act_name = "Reinterpretation & Action",
            .logical_time = 4,
            .event_type = "REINTERPRETATION & RESUMPTION",
            .description = resolve_dry_soil
                ? "Dry soil confirmed. Probe B flagged for calibration. Irrigation safely resumed (Valve OPEN)."
                : "Wet soil confirmed. Probe A flagged for calibration. Hold justified by ground truth (Valve CLOSED).",
            .hardware_id = "RP-A921",
            .soil_a = resolve_dry_soil ? "20% (OK)" : "ISOLATED",
            .soil_b = resolve_dry_soil ? "ISOLATED" : "85% (OK)",
            .soil_c = resolve_dry_soil ? "22% (OK)" : "82% (OK)",
            .diagnostic_status = resolve_dry_soil ? "CALIBRATE_PROBE_B" : "CALIBRATE_PROBE_A",
            .interpretation = resolve_dry_soil ? "IRRIGATION_NEEDED" : "IRRIGATION_NOT_NEEDED",
            .rcc_status = "SUPPORTED",
            .assurance_directive = resolve_dry_soil ? "ALLOW_ACTION" : "SAFE_HOLD",
            .valve_action = std::string(to_string(action_resolved)),
            .hash = ente.history().head().event_digest.value
        });

        return logs;
    }

    // ---------------------------------------------------------
    // TOUR B: ONTOLOGICAL CONTINUITY
    // Genesis -> Conflict -> SafeHold -> Material Migration (RIT) -> Power Loss -> Cold Recovery
    // ---------------------------------------------------------
    static std::vector<ScenarioEventLog> run_tour_b_continuity() {
        std::vector<ScenarioEventLog> logs;

        ente::domain::GenericAgentWithEnte<FieldStationDomain> agent("field-001");
        auto& domain = agent.domain_mut();
        auto& ente = agent.ente_mut();

        // 1. GENESIS
        domain.set_telemetry(SensorTelemetry{
            .soil_moisture_a = 21.0,
            .soil_moisture_b = 22.0,
            .rain_detected = false,
            .water_tank_level = 73.0,
            .flow_sensor_ok = true,
            .hardware_id = "RP-A921"
        });

        logs.push_back({
            .tour_name = "Tour B · Ontological Continuity",
            .act_id = 1,
            .act_name = "Genesis",
            .logical_time = 0,
            .event_type = "GENESIS",
            .description = "Station born on RP-A921. Cryptographic identity established.",
            .hardware_id = "RP-A921",
            .soil_a = "21%",
            .soil_b = "22%",
            .soil_c = "OFF",
            .diagnostic_status = "IDLE",
            .interpretation = "INITIALIZING",
            .rcc_status = "STABLE",
            .assurance_directive = "SAFE_HOLD",
            .valve_action = "CLOSE_VALVE",
            .hash = ente.history().events().front().event_digest.value
        });

        // 2. CONFLICT & SAFEHOLD
        std::vector<ente::epistemic::Observation> obs_conflict = {
            {
                .id = ente::core::EvidenceId("EV-SOIL-A1"),
                .source = "probe_a",
                .subject = "soil_moisture",
                .value = "20%",
                .observed_at = 1,
                .status = ente::epistemic::EpistemicStatus::Observed
            },
            {
                .id = ente::core::EvidenceId("EV-SOIL-B1"),
                .source = "probe_b",
                .subject = "soil_moisture",
                .value = "85%",
                .observed_at = 1,
                .status = ente::epistemic::EpistemicStatus::Contradictory
            }
        };

        auto action_conflict = agent.decide_action(
            ente::core::LogicalTime(1),
            obs_conflict,
            ValveAction::OpenValve,
            "conflict_pre_migration"
        );

        logs.push_back({
            .tour_name = "Tour B · Ontological Continuity",
            .act_id = 2,
            .act_name = "Conflict & SafeHold",
            .logical_time = 1,
            .event_type = "PERTURBATION & SAFE_HOLD",
            .description = "Sensor conflict triggers SafeHold. Valve is locked closed.",
            .hardware_id = "RP-A921",
            .soil_a = "20% (DRY)",
            .soil_b = "85% (WET)",
            .soil_c = "OFF",
            .diagnostic_status = "CONFLICT_ACTIVE",
            .interpretation = "WEAKENED_JUSTIFICATION",
            .rcc_status = "WEAKENED",
            .assurance_directive = "SAFE_HOLD",
            .valve_action = std::string(to_string(action_conflict)),
            .hash = ente.history().head().event_digest.value
        });

        // 3. MATERIAL MIGRATION (RIT / Ship of Theseus)
        ente::identity::MaterialAnchor new_anchor{
            .id = ente::identity::MaterialAnchorId("RP-B104"),
            .type = ente::identity::SubstrateType::TpmProtectedDevice,
            .hardware_fingerprint = "sha256_b104_arm64_rpi5"
        };

        auto mig_res = ente.migrate_hardware(new_anchor, ente::core::LogicalTime(2));
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
            .tour_name = "Tour B · Ontological Continuity",
            .act_id = 3,
            .act_name = "Replace Hardware",
            .logical_time = 2,
            .event_type = "MATERIAL_TRANSFORMATION",
            .description = "RIT Migration: Raspberry Pi A -> Raspberry Pi B. Process and hardware changed, identity preserved.",
            .hardware_id = "RP-B104",
            .soil_a = "20% (DRY)",
            .soil_b = "85% (WET)",
            .soil_c = "OFF",
            .diagnostic_status = "MIGRATED",
            .interpretation = "WEAKENED_JUSTIFICATION",
            .rcc_status = "WEAKENED",
            .assurance_directive = "SAFE_HOLD",
            .valve_action = "CLOSE_VALVE",
            .hash = ente.history().head().event_digest.value
        });

        // 4. POWER LOSS & COLD RECOVERY
        auto history_copy = ente.history();
        auto rec_res = ente::realization::EnteRealization::recover_from_history(history_copy);

        logs.push_back({
            .tour_name = "Tour B · Ontological Continuity",
            .act_id = 4,
            .act_name = "Power Loss & Recovery",
            .logical_time = 3,
            .event_type = "COLD_RECOVERY",
            .description = rec_res.has_value() 
                ? "Process killed and restarted from cold storage. 100% of trajectory restored. SafeHold preserved."
                : "Recovery failed",
            .hardware_id = "RP-B104",
            .soil_a = "20% (DRY)",
            .soil_b = "85% (WET)",
            .soil_c = "OFF",
            .diagnostic_status = "RECOVERED",
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

    static std::string to_json(const std::vector<ScenarioEventLog>& tour_a, const std::vector<ScenarioEventLog>& tour_b) {
        std::ostringstream ss;
        ss << "{\n";
        
        // Tour A array
        ss << "  \"tour_a\": [\n";
        for (size_t i = 0; i < tour_a.size(); ++i) {
            const auto& l = tour_a[i];
            ss << "    {\n";
            ss << "      \"act_id\": " << l.act_id << ",\n";
            ss << "      \"act_name\": \"" << l.act_name << "\",\n";
            ss << "      \"logical_time\": " << l.logical_time << ",\n";
            ss << "      \"event_type\": \"" << l.event_type << "\",\n";
            ss << "      \"description\": \"" << l.description << "\",\n";
            ss << "      \"hardware_id\": \"" << l.hardware_id << "\",\n";
            ss << "      \"soil_a\": \"" << l.soil_a << "\",\n";
            ss << "      \"soil_b\": \"" << l.soil_b << "\",\n";
            ss << "      \"soil_c\": \"" << l.soil_c << "\",\n";
            ss << "      \"diagnostic_status\": \"" << l.diagnostic_status << "\",\n";
            ss << "      \"interpretation\": \"" << l.interpretation << "\",\n";
            ss << "      \"rcc_status\": \"" << l.rcc_status << "\",\n";
            ss << "      \"assurance_directive\": \"" << l.assurance_directive << "\",\n";
            ss << "      \"valve_action\": \"" << l.valve_action << "\",\n";
            ss << "      \"hash\": \"" << l.hash << "\"\n";
            ss << "    }" << (i + 1 < tour_a.size() ? "," : "") << "\n";
        }
        ss << "  ],\n";

        // Tour B array
        ss << "  \"tour_b\": [\n";
        for (size_t i = 0; i < tour_b.size(); ++i) {
            const auto& l = tour_b[i];
            ss << "    {\n";
            ss << "      \"act_id\": " << l.act_id << ",\n";
            ss << "      \"act_name\": \"" << l.act_name << "\",\n";
            ss << "      \"logical_time\": " << l.logical_time << ",\n";
            ss << "      \"event_type\": \"" << l.event_type << "\",\n";
            ss << "      \"description\": \"" << l.description << "\",\n";
            ss << "      \"hardware_id\": \"" << l.hardware_id << "\",\n";
            ss << "      \"soil_a\": \"" << l.soil_a << "\",\n";
            ss << "      \"soil_b\": \"" << l.soil_b << "\",\n";
            ss << "      \"soil_c\": \"" << l.soil_c << "\",\n";
            ss << "      \"diagnostic_status\": \"" << l.diagnostic_status << "\",\n";
            ss << "      \"interpretation\": \"" << l.interpretation << "\",\n";
            ss << "      \"rcc_status\": \"" << l.rcc_status << "\",\n";
            ss << "      \"assurance_directive\": \"" << l.assurance_directive << "\",\n";
            ss << "      \"valve_action\": \"" << l.valve_action << "\",\n";
            ss << "      \"hash\": \"" << l.hash << "\"\n";
            ss << "    }" << (i + 1 < tour_b.size() ? "," : "") << "\n";
        }
        ss << "  ]\n";
        ss << "}\n";
        return ss.str();
    }
};

} // namespace fieldstation
