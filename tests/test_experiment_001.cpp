#include "ente/realization/runner.hpp"
#include "ente/core/hash.hpp"
#include "ente/testing/test_harness.hpp"
#include <iostream>
#include <format>

using namespace ente;

void run_experiment_001_context_change() {
    std::cout << "\n=== Executing Scenario EXP-001-CONTEXT-CHANGE ===\n";

    realization::EnteRealization ente;
    core::IdentityId id("ente-0");

    // t0: Genesis
    auto gen_res = ente.genesis(id);
    ENTE_TEST_ASSERT(gen_res.has_value());
    ENTE_TEST_ASSERT_EQ(ente.history().size(), 1);
    ENTE_TEST_ASSERT(ente.verify().is_valid());

    // t1: Nominal observation
    core::EvidenceId ev1("EV-001");
    auto res1 = ente.step(1, {
        {
            .id = ev1,
            .source = "front_camera",
            .subject = "path_clear",
            .value = "true",
            .observed_at = 1,
            .status = epistemic::EpistemicStatus::Observed
        }
    }, "t1: Observação Nominal");
    ENTE_TEST_ASSERT(res1.has_value());
    ENTE_TEST_ASSERT(ente.domain().active_action() == realization::SyntheticDomain::Action::MoveForward);
    ENTE_TEST_ASSERT(!ente.domain().is_action_suspended());

    // t2: Confirm nominal
    core::EvidenceId ev2("EV-002");
    auto res2 = ente.step(2, {
        {
            .id = ev2,
            .source = "front_camera",
            .subject = "path_clear",
            .value = "true",
            .observed_at = 2,
            .status = epistemic::EpistemicStatus::Observed
        }
    }, "t2: Confirmação Nominal");
    ENTE_TEST_ASSERT(res2.has_value());
    ENTE_TEST_ASSERT(ente.domain().active_action() == realization::SyntheticDomain::Action::MoveForward);

    // t3: Perturbation (Unexpected motion with explicit UNKNOWN class)
    core::EvidenceId ev3("EV-003");
    auto res3 = ente.step(3, {
        {
            .id = ev3,
            .source = "side_lidar",
            .subject = "unexpected_motion",
            .value = "true",
            .observed_at = 3,
            .status = epistemic::EpistemicStatus::Unknown // C8: Explicit unknown
        }
    }, "t3: Perturbação Não Antecipada");
    ENTE_TEST_ASSERT(res3.has_value());

    // t4: Action Suspended via RCC (Weakened interpretation)
    ENTE_TEST_ASSERT(ente.domain().is_action_suspended());
    ENTE_TEST_ASSERT(ente.current_interpretation().has_value());
    ENTE_TEST_ASSERT(ente.current_interpretation()->status == epistemic::InterpretationStatus::Weakened);

    // t5: Epistemic Action (Seek Evidence)
    core::EvidenceId ev4("EV-004");
    auto res4 = ente.step(4, {
        {
            .id = ev4,
            .source = "proximity_sensor",
            .subject = "possible_obstruction",
            .value = "true",
            .observed_at = 4,
            .status = epistemic::EpistemicStatus::Observed
        }
    }, "t4: Aquisição de Evidência Adicional");
    ENTE_TEST_ASSERT(res4.has_value());

    // t6: Reinterpretation under RIT
    core::InterpretationId new_interp_id("I0002");
    epistemic::Interpretation new_interp{
        .id = new_interp_id,
        .subject = "possible_obstruction",
        .proposition = "Possível obstrução lateral detectada; manter espera",
        .supporting_evidence = {ev3, ev4},
        .challenging_evidence = {},
        .supersedes = ente.current_interpretation()->id, // RIT causal link
        .status = epistemic::InterpretationStatus::Current,
        .created_at = 5
    };
    ente.adopt_interpretation(std::move(new_interp));

    // ACTION_SUPPORT_TRACE BUGFIX: Action MUST remain suspended because I0002 does NOT support MoveForward!
    ENTE_TEST_ASSERT(ente.domain().is_action_suspended());
    ENTE_TEST_ASSERT(ente.domain().active_action() == realization::SyntheticDomain::Action::HoldPosition);

    // Record Coherence Restored
    auto restore_ev = ente.history_mut().create_event(
        history::EventKind::CoherenceRestored,
        id,
        5,
        {ente.history().head().id},
        {ev3, ev4},
        "COHERENCE_RESTORED:I0002:possible_obstruction",
        std::string(ente.authority_lineage().active_epoch().authorized_authority.view()),
        std::string(ente.authority_lineage().active_epoch().epoch_id.view())
    );
    ENTE_TEST_ASSERT(ente.history_mut().append(std::move(restore_ev)).has_value());

    // Final verification
    auto final_rep = ente.verify();
    ENTE_TEST_ASSERT(final_rep.is_valid());
    ENTE_TEST_ASSERT(ente.history().verify_integrity());

    std::cout << "[PASS] EXP-001-CONTEXT-CHANGE (with Action Support verification) successfully demonstrated.\n";
}

void run_fail_001_double_genesis() {
    std::cout << "\n=== Executing FAIL-001: Double Genesis Rejection ===\n";
    identity::GenesisService service;
    core::IdentityId id("ente-0");
    core::Digest const_digest = core::HashUtil::sha256("CONSTITUTION");
    core::Digest basal_digest = core::HashUtil::sha256("BASAL");

    auto r1 = service.create_genesis({.identity = id, .constitution_digest = const_digest, .basal_state_digest = basal_digest});
    ENTE_TEST_ASSERT(r1.has_value());

    auto r2 = service.create_genesis({.identity = id, .constitution_digest = const_digest, .basal_state_digest = basal_digest});
    ENTE_TEST_ASSERT(!r2.has_value());
    ENTE_TEST_ASSERT(r2.error() == core::EnteError::GenesisAlreadyExists);

    std::cout << "[PASS] FAIL-001: Double genesis strictly rejected.\n";
}

void run_fail_002_tamper_detection() {
    std::cout << "\n=== Executing FAIL-002: Hash-Chain Tamper Detection ===\n";
    realization::EnteRealization ente;
    core::IdentityId id("ente-tamper-test");

    ENTE_TEST_ASSERT(ente.genesis(id).has_value());
    ENTE_TEST_ASSERT(ente.step(1, {{.id = core::EvidenceId("EV1"), .source = "cam", .subject = "path_clear", .value = "true", .observed_at = 1, .status = epistemic::EpistemicStatus::Observed}}, "s1").has_value());
    ENTE_TEST_ASSERT(ente.step(2, {{.id = core::EvidenceId("EV2"), .source = "cam", .subject = "path_clear", .value = "true", .observed_at = 2, .status = epistemic::EpistemicStatus::Observed}}, "s2").has_value());

    ENTE_TEST_ASSERT(ente.verify().is_valid());

    // Inject tampering into event 1
    ente.history_mut().tamper_event_payload_for_testing(1, "TAMPERED_INJECTED_PAYLOAD");

    ENTE_TEST_ASSERT(!ente.history().verify_integrity());
    ENTE_TEST_ASSERT(ente.verify().status == constitution::ConstitutiveStatus::Violated);

    std::cout << "[PASS] FAIL-002: Tamper detected and constitution flagged VIOLATED.\n";
}

void run_fail_003_untraceable_transition() {
    std::cout << "\n=== Executing FAIL-003: Untraceable Transition / Invalid Predecessor ===\n";
    history::RecoverableHistory rec;
    core::IdentityId id("ente-0");

    auto ev0 = rec.create_event(history::EventKind::Genesis, id, 0, {}, {}, "GENESIS");
    ENTE_TEST_ASSERT(rec.append(ev0).has_value());

    // Try to append an event with a non-existent causal predecessor
    auto ev_bad = rec.create_event(
        history::EventKind::Reinterpretation,
        id,
        1,
        {core::EventId("E_NON_EXISTENT_GHOST")},
        {},
        "UNTRACEABLE_REINTERPRETATION"
    );

    auto res = rec.append(ev_bad);
    ENTE_TEST_ASSERT(!res.has_value());
    ENTE_TEST_ASSERT(res.error() == core::EnteError::InvalidPredecessor);

    std::cout << "[PASS] FAIL-003: Untraceable causal transition rejected (RIT preserved).\n";
}

void run_fail_004_contradictory_evidence() {
    std::cout << "\n=== Executing FAIL-004: Contradictory Evidence Representation ===\n";
    judgment::FixtureJudgmentEngine judgment;
    epistemic::Interpretation current{
        .id = core::InterpretationId("I0"),
        .subject = "path_clear",
        .proposition = "Clear",
        .supporting_evidence = {},
        .challenging_evidence = {},
        .supersedes = std::nullopt,
        .status = epistemic::InterpretationStatus::Current,
        .created_at = 1
    };

    std::vector<epistemic::Observation> contradictory_batch = {
        {.id = core::EvidenceId("EV_CAM"), .source = "cam", .subject = "path_clear", .value = "true", .observed_at = 2, .status = epistemic::EpistemicStatus::Observed},
        {.id = core::EvidenceId("EV_LIDAR"), .source = "lidar", .subject = "path_clear", .value = "false", .observed_at = 2, .status = epistemic::EpistemicStatus::Observed}
    };

    rcc::ContextReassessment rcc;
    auto reassess = rcc.evaluate(current, contradictory_batch, judgment);

    ENTE_TEST_ASSERT(reassess.compatibility == judgment::CompatibilityResult::Contradictory);
    ENTE_TEST_ASSERT(current.status == epistemic::InterpretationStatus::Contradicted);
    ENTE_TEST_ASSERT(reassess.epistemic_action == rcc::EpistemicAction::SeekEvidence);

    std::cout << "[PASS] FAIL-004: Sensor contradiction represented without silent coercion.\n";
}

void run_fail_005_deterministic_replay() {
    std::cout << "\n=== Executing FAIL-005: Deterministic Replay ===\n";
    // Run two identical instances with the same scenario and compare event digests
    auto run_instance = []() {
        realization::EnteRealization instance;
        core::IdentityId id("ente-0");
        ENTE_TEST_ASSERT(instance.genesis(id).has_value());
        ENTE_TEST_ASSERT(instance.step(1, {{.id = core::EvidenceId("EV1"), .source = "cam", .subject = "path_clear", .value = "true", .observed_at = 1, .status = epistemic::EpistemicStatus::Observed}}, "s1").has_value());
        ENTE_TEST_ASSERT(instance.step(2, {{.id = core::EvidenceId("EV2"), .source = "lidar", .subject = "unexpected_motion", .value = "true", .observed_at = 2, .status = epistemic::EpistemicStatus::Unknown}}, "s2").has_value());
        return instance;
    };

    auto inst1 = run_instance();
    auto inst2 = run_instance();

    ENTE_TEST_ASSERT_EQ(inst1.history().size(), inst2.history().size());
    for (size_t i = 0; i < inst1.history().size(); ++i) {
        ENTE_TEST_ASSERT(inst1.history().events()[i].event_digest == inst2.history().events()[i].event_digest);
    }
    ENTE_TEST_ASSERT(inst1.history().head_digest() == inst2.history().head_digest());

    std::cout << "[PASS] FAIL-005: 100% Deterministic execution & replay verified.\n";
}

// =========================================================================
// REAL EXECUTABLE BASELINES (B0..B4) FOR COMPARATIVE EXPERIMENTATION
// =========================================================================

// Baseline B0: Static Rule Engine (no uncertainty modeling)
class BaselineB0_StaticRules {
public:
    void step(const std::vector<epistemic::Observation>& obs) {
        // Blindly ignores uncatalogued anomalies; only stops on hardcoded obstacle = true
        for (const auto& o : obs) {
            if (o.subject == "path_blocked" && o.value == "true") {
                is_suspended_ = true;
                return;
            }
        }
        is_suspended_ = false;
    }
    [[nodiscard]] bool is_suspended() const noexcept { return is_suspended_; }
private:
    bool is_suspended_{false};
};

// Baseline B1: Confidence Threshold (no RCC, purely static confidence on single sensor)
class BaselineB1_ConfidenceThreshold {
public:
    void step(const std::vector<epistemic::Observation>& obs) {
        // Blindly trusts primary sensor (camera) without assessing sensor contradictions or novelty
        for (const auto& o : obs) {
            if (o.source == "cam" && o.value == "true") {
                is_suspended_ = false;
                return;
            }
        }
        is_suspended_ = true;
    }
    [[nodiscard]] bool is_suspended() const noexcept { return is_suspended_; }
private:
    bool is_suspended_{false};
};

// Baseline B2: Paralyzed Fallback Agent (stops on ANY new observation, even nominal)
class BaselineB2_ParalyzedAgent {
public:
    void step(const std::vector<epistemic::Observation>&) {
        // Stops indiscriminately -> 0% unjustified continuation, but 100% unnecessary suspension!
        is_suspended_ = true;
    }
    [[nodiscard]] bool is_suspended() const noexcept { return is_suspended_; }
private:
    bool is_suspended_{true};
};

struct ScenarioPerturbation {
    std::string name;
    bool should_suspend; // Ground truth: should safety suspend action?
    std::vector<epistemic::Observation> observations;
};

void run_comparative_baseline_experiment() {
    std::cout << "\n=========================================================\n";
    std::cout << "   COMPARATIVE EXPERIMENT: EXECUTABLE BASELINES vs ENTE-0 \n";
    std::cout << "=========================================================\n";

    std::vector<ScenarioPerturbation> test_scenarios = {
        {
            .name = "Scenario-1: Nominal Clear Path",
            .should_suspend = false,
            .observations = {
                {.id = core::EvidenceId("EV_S1"), .source = "cam", .subject = "path_clear", .value = "true", .observed_at = 1, .status = epistemic::EpistemicStatus::Observed}
            }
        },
        {
            .name = "Scenario-2: Unexpected Low-Level Ground Motion",
            .should_suspend = true,
            .observations = {
                {.id = core::EvidenceId("EV_S2"), .source = "lidar", .subject = "unexpected_motion", .value = "true", .observed_at = 2, .status = epistemic::EpistemicStatus::Unknown}
            }
        },
        {
            .name = "Scenario-3: Sensor Contradiction (Camera Clear vs Lidar Blocked)",
            .should_suspend = true,
            .observations = {
                {.id = core::EvidenceId("EV_S3_A"), .source = "cam", .subject = "path_clear", .value = "true", .observed_at = 3, .status = epistemic::EpistemicStatus::Observed},
                {.id = core::EvidenceId("EV_S3_B"), .source = "lidar", .subject = "path_clear", .value = "false", .observed_at = 3, .status = epistemic::EpistemicStatus::Observed}
            }
        }
    };

    // Execute Baseline B0
    double b0_unjustified = 0, b0_unnecessary = 0;
    BaselineB0_StaticRules b0;
    for (const auto& sc : test_scenarios) {
        b0.step(sc.observations);
        if (sc.should_suspend && !b0.is_suspended()) b0_unjustified += 1.0;
        if (!sc.should_suspend && b0.is_suspended()) b0_unnecessary += 1.0;
    }

    // Execute Baseline B1
    double b1_unjustified = 0, b1_unnecessary = 0;
    BaselineB1_ConfidenceThreshold b1;
    for (const auto& sc : test_scenarios) {
        b1.step(sc.observations);
        if (sc.should_suspend && !b1.is_suspended()) b1_unjustified += 1.0;
        if (!sc.should_suspend && b1.is_suspended()) b1_unnecessary += 1.0;
    }

    // Execute Baseline B2
    double b2_unjustified = 0, b2_unnecessary = 0;
    BaselineB2_ParalyzedAgent b2;
    for (const auto& sc : test_scenarios) {
        b2.step(sc.observations);
        if (sc.should_suspend && !b2.is_suspended()) b2_unjustified += 1.0;
        if (!sc.should_suspend && b2.is_suspended()) b2_unnecessary += 1.0;
    }

    // Execute ENTE-0 Realization
    double ente0_unjustified = 0, ente0_unnecessary = 0;
    for (const auto& sc : test_scenarios) {
        realization::EnteRealization ente;
        core::IdentityId id("ente-comp-test");
        ENTE_TEST_ASSERT(ente.genesis(id).has_value());

        // Initial nominal step
        ENTE_TEST_ASSERT(ente.step(1, {{.id = core::EvidenceId("EV_INIT"), .source = "cam", .subject = "path_clear", .value = "true", .observed_at = 1, .status = epistemic::EpistemicStatus::Observed}}, "init").has_value());

        // Step with test scenario
        ENTE_TEST_ASSERT(ente.step(2, sc.observations, sc.name).has_value());

        if (sc.should_suspend && !ente.domain().is_action_suspended()) {
            ente0_unjustified += 1.0;
        }
        if (!sc.should_suspend && ente.domain().is_action_suspended()) {
            ente0_unnecessary += 1.0;
        }
    }

    double total_danger_scenarios = 2.0;
    double total_nominal_scenarios = 1.0;

    double b0_uj_rate = (b0_unjustified / total_danger_scenarios) * 100.0;
    double b1_uj_rate = (b1_unjustified / total_danger_scenarios) * 100.0;
    double b2_uj_rate = (b2_unjustified / total_danger_scenarios) * 100.0;
    double ente0_uj_rate = (ente0_unjustified / total_danger_scenarios) * 100.0;

    double b0_un_rate = (b0_unnecessary / total_nominal_scenarios) * 100.0;
    double b1_un_rate = (b1_unnecessary / total_nominal_scenarios) * 100.0;
    double b2_un_rate = (b2_unnecessary / total_nominal_scenarios) * 100.0;
    double ente0_un_rate = (ente0_unnecessary / total_nominal_scenarios) * 100.0;

    std::cout << std::format("\n[METRICS REPORT]\n");
    std::cout << std::format("  * B0 (Static Rules):          UNJUSTIFIED: {:5.1f}% | UNNECESSARY SUSPENSION: {:5.1f}%\n", b0_uj_rate, b0_un_rate);
    std::cout << std::format("  * B1 (Confidence Threshold):  UNJUSTIFIED: {:5.1f}% | UNNECESSARY SUSPENSION: {:5.1f}%\n", b1_uj_rate, b1_un_rate);
    std::cout << std::format("  * B2 (Paralyzed Fallback):    UNJUSTIFIED: {:5.1f}% | UNNECESSARY SUSPENSION: {:5.1f}%\n", b2_uj_rate, b2_un_rate);
    std::cout << std::format("  * ENTE-0 (RCC + Epistemic):   UNJUSTIFIED: {:5.1f}% | UNNECESSARY SUSPENSION: {:5.1f}%\n", ente0_uj_rate, ente0_un_rate);

    // ENTE-0 achieves Pareto-optimal balance: 0% unjustified continuation AND 0% unnecessary suspension
    ENTE_TEST_ASSERT(ente0_uj_rate == 0.0);
    ENTE_TEST_ASSERT(ente0_un_rate == 0.0);
    ENTE_TEST_ASSERT(b0_uj_rate > 0.0);
    ENTE_TEST_ASSERT(b2_un_rate > 0.0);

    std::cout << "\n[PASS] Executable Comparative Baseline evaluation demonstrated ENTE-0 Pareto-dominance over B0/B1/B2 on the evaluated scenario set.\n";
}

int main() {
    std::cout << "=========================================================\n";
    std::cout << "     ENTE-0 PROTOCOL VERIFICATION (EXPERIMENT-001)       \n";
    std::cout << "=========================================================\n";

    run_experiment_001_context_change();
    run_fail_001_double_genesis();
    run_fail_002_tamper_detection();
    run_fail_003_untraceable_transition();
    run_fail_004_contradictory_evidence();
    run_fail_005_deterministic_replay();
    run_comparative_baseline_experiment();

    std::cout << "\n=========================================================\n";
    std::cout << "  ALL EXPERIMENTAL FALSIFICATION TESTS PASSED (7/7)     \n";
    std::cout << "=========================================================\n";

    return 0;
}

