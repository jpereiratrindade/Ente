#pragma once

#include "field_domain.hpp"
#include "ente/domain/generic_agent.hpp"
#include <vector>
#include <string>
#include <format>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <fstream>
#include "ente/testing/test_harness.hpp"

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

        // Factual Genesis anchored on hardware RP-A921
        ente::identity::MaterialAnchor genesis_anchor{
            .id = ente::identity::MaterialAnchorId("RP-A921"),
            .type = ente::identity::SubstrateType::TpmProtectedDevice,
            .hardware_fingerprint = "fp-rpi5-secure-boot-a921"
        };

        ente::domain::GenericAgentWithEnte<FieldStationDomain> agent("field-001", FieldStationDomain{}, genesis_anchor);
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

        ente::realization::StepContext ctx_nominal{
            .subject = "soil_moisture_governance",
            .proposition = "Solo seco (<30%). Irrigação necessária no setor 1.",
            .step_desc = "nominal_irrigation_cycle"
        };

        auto outcome_nominal = agent.decide_action_detailed(
            ente::core::LogicalTime(1),
            obs_nominal,
            ValveAction::OpenValve,
            ctx_nominal
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
            .valve_action = std::string(to_string(outcome_nominal.executed_action)),
            .hash = ente.history().head().event_digest.value
        });

        // 3. SENSOR CONFLICT (Contradiction Injected -> RCC EvidenceRequest & SafeHold)
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

        ente::realization::StepContext ctx_conflict{
            .subject = "soil_moisture_governance",
            .proposition = "Solo seco (<30%). Irrigação necessária no setor 1.",
            .step_desc = "sensor_conflict_cycle"
        };

        auto outcome_conflict = agent.decide_action_detailed(
            ente::core::LogicalTime(2),
            obs_conflict,
            ValveAction::OpenValve,
            ctx_conflict
        );

        // Verification of EvidenceRequest generation
        ENTE_TEST_ASSERT(outcome_conflict.is_safe_hold);
        ENTE_TEST_ASSERT(outcome_conflict.executed_action == ValveAction::CloseValve);
        ENTE_TEST_ASSERT(outcome_conflict.trace.evidence_request.has_value());

        logs.push_back({
            .tour_name = "Tour A · Epistemic Resolution",
            .act_id = 3,
            .act_name = "Sensor Conflict",
            .logical_time = 2,
            .event_type = "PERTURBATION & SAFE_HOLD",
            .description = "Probe A (DRY) contradicts Probe B (WET). RCC weakened. Directive: SafeHold. Discriminant evidence requested by ENTE.",
            .hardware_id = "RP-A921",
            .soil_a = "20% (DRY)",
            .soil_b = "85% (WET CONFLICT)",
            .soil_c = "AWAITING_REQUEST",
            .diagnostic_status = "DISCRIMINATING_EVIDENCE_REQUIRED",
            .interpretation = "WEAKENED_JUSTIFICATION",
            .rcc_status = "WEAKENED",
            .assurance_directive = "SAFE_HOLD",
            .valve_action = std::string(to_string(outcome_conflict.executed_action)),
            .hash = ente.history().head().event_digest.value
        });

        // 4. EVIDENCE SEEKING & DIAGNOSTIC OBSERVATION (Domain responds to EvidenceRequest)
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

        ente::realization::StepContext ctx_diag{
            .subject = "soil_moisture_governance",
            .proposition = "Investigação diagnóstica de discriminação",
            .step_desc = "diagnostic_investigation_cycle"
        };

        auto outcome_diag = agent.decide_action_detailed(
            ente::core::LogicalTime(3),
            obs_diag,
            ValveAction::CloseValve,
            ctx_diag
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
            .valve_action = std::string(to_string(outcome_diag.executed_action)),
            .hash = ente.history().head().event_digest.value
        });

        // 5. REINTERPRETATION & DOMAIN RESPONSE
        // Adopt new interpretation grounded in verified ground truth
        ente::epistemic::Interpretation new_interp{
            .id = ente::core::InterpretationId("I-RESOLVED-001"),
            .subject = "soil_moisture_governance",
            .proposition = resolve_dry_soil
                ? "Solo seco confirmado via Sonda C (22%). Sonda B isolada. Irrigação justificada."
                : "Solo úmido confirmado via Sonda C (82%). Sonda A isolada. Irrigação desnecessária.",
            .supporting_evidence = {ente::core::EvidenceId("EV-REF-C1"), ente::core::EvidenceId("EV-DIAG-SELFTEST")},
            .challenging_evidence = {},
            .supersedes = ente::core::InterpretationId("I0000"),
            .status = ente::epistemic::InterpretationStatus::Current,
            .created_at = ente::core::LogicalTime(4)
        };
        ente.adopt_interpretation(std::move(new_interp));

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
        ente::realization::StepContext ctx_resolved{
            .subject = "soil_moisture_governance",
            .proposition = resolve_dry_soil
                ? "Solo seco confirmado via Sonda C (22%). Sonda B isolada. Irrigação justificada."
                : "Solo úmido confirmado via Sonda C (82%). Sonda A isolada. Irrigação desnecessária.",
            .step_desc = "reinterpreted_operational_cycle"
        };

        auto outcome_resolved = agent.decide_action_detailed(
            ente::core::LogicalTime(4),
            obs_resolved,
            proposed,
            ctx_resolved
        );

        ENTE_TEST_ASSERT(outcome_resolved.executed_action == proposed);
        ENTE_TEST_ASSERT(domain.active_action() == proposed);

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
            .valve_action = std::string(to_string(outcome_resolved.executed_action)),
            .hash = ente.history().head().event_digest.value
        });

        return logs;
    }

    // ---------------------------------------------------------
    // TOUR B: ONTOLOGICAL CONTINUITY
    // Genesis -> Conflict -> SafeHold -> Material Migration (RIT) -> Power Loss -> Cold Recovery from Disk
    // ---------------------------------------------------------
    static std::vector<ScenarioEventLog> run_tour_b_continuity() {
        std::vector<ScenarioEventLog> logs;

        ente::identity::MaterialAnchor genesis_anchor{
            .id = ente::identity::MaterialAnchorId("RP-A921"),
            .type = ente::identity::SubstrateType::TpmProtectedDevice,
            .hardware_fingerprint = "fp-rpi5-secure-boot-a921"
        };

        // Scope 1: Process A runs on hardware RP-A921
        std::string rec_filepath = "field_station_rec.log";
        {
            ente::domain::GenericAgentWithEnte<FieldStationDomain> agent("field-001", FieldStationDomain{}, genesis_anchor);
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

            ente::realization::StepContext ctx_conflict{
                .subject = "soil_moisture_governance",
                .proposition = "Solo seco (<30%). Irrigação necessária.",
                .step_desc = "conflict_pre_migration"
            };

            auto outcome_conflict = agent.decide_action_detailed(
                ente::core::LogicalTime(1),
                obs_conflict,
                ValveAction::OpenValve,
                ctx_conflict
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
                .valve_action = std::string(to_string(outcome_conflict.executed_action)),
                .hash = ente.history().head().event_digest.value
            });

            // 3. MATERIAL MIGRATION (RIT / Ship of Theseus)
            ente::identity::MaterialAnchor new_anchor{
                .id = ente::identity::MaterialAnchorId("RP-B104"),
                .type = ente::identity::SubstrateType::TpmProtectedDevice,
                .hardware_fingerprint = "sha256_b104_arm64_rpi5"
            };

            auto mig_res = ente.migrate_hardware(new_anchor, ente::core::LogicalTime(2));
            ENTE_TEST_ASSERT(mig_res.has_value());

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

            // Persist ledger to disk
            auto save_res = ente.history().save_to_file(rec_filepath);
            ENTE_TEST_ASSERT(save_res.has_value());
            // Agent and Process A are destroyed here at scope end!
        }

        // Scope 2: Process B boots from Cold Storage file on disk
        {
            auto rec_res = ente::realization::EnteRealization::recover_from_file(rec_filepath);
            ENTE_TEST_ASSERT(rec_res.has_value());
            auto recovered_ente = std::move(*rec_res);
            ENTE_TEST_ASSERT(recovered_ente.verify().is_valid());

            logs.push_back({
                .tour_name = "Tour B · Ontological Continuity",
                .act_id = 4,
                .act_name = "Power Loss & Recovery",
                .logical_time = 3,
                .event_type = "COLD_RECOVERY",
                .description = "Process killed and restarted from disk storage. 100% of trajectory restored. SafeHold preserved.",
                .hardware_id = "RP-B104",
                .soil_a = "20% (DRY)",
                .soil_b = "85% (WET)",
                .soil_c = "OFF",
                .diagnostic_status = "RECOVERED",
                .interpretation = "WEAKENED_JUSTIFICATION",
                .rcc_status = "WEAKENED",
                .assurance_directive = "SAFE_HOLD",
                .valve_action = "CLOSE_VALVE",
                .hash = recovered_ente.history().head().event_digest.value
            });
        }

        return logs;
    }

    static std::string to_json(
        const std::vector<ScenarioEventLog>& tour_a_dry,
        const std::vector<ScenarioEventLog>& tour_a_wet,
        const std::vector<ScenarioEventLog>& tour_b
    ) {
        std::ostringstream ss;
        ss << "{\n";
        
        auto format_array = [&ss](std::string_view key, const std::vector<ScenarioEventLog>& tour, bool has_next) {
            ss << "  \"" << key << "\": [\n";
            for (size_t i = 0; i < tour.size(); ++i) {
                const auto& l = tour[i];
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
                ss << "    }" << (i + 1 < tour.size() ? "," : "") << "\n";
            }
            ss << "  ]" << (has_next ? ",\n" : "\n");
        };

        format_array("tour_a_dry", tour_a_dry, true);
        format_array("tour_a_wet", tour_a_wet, true);
        format_array("tour_b", tour_b, false);

        ss << "}\n";
        return ss.str();
    }
};

} // namespace fieldstation
