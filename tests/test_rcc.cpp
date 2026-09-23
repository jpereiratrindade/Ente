#include "ente/rcc/reassessment.hpp"
#include "ente/judgment/fixture.hpp"
#include "ente/testing/test_harness.hpp"
#include <iostream>

int main() {
    using namespace ente;

    rcc::ContextReassessment rcc_engine;
    judgment::FixtureJudgmentEngine judgment;

    ENTE_TEST_ASSERT(rcc_engine.current_state() == rcc::RCCState::Stable);

    core::InterpretationId interp_id("I0001");
    core::EvidenceId ev_nominal("EV-NOMINAL");
    epistemic::Interpretation current{
        .id = interp_id,
        .subject = "path_clear",
        .proposition = "Caminho livre para trânsito",
        .supporting_evidence = {ev_nominal},
        .challenging_evidence = {},
        .supersedes = std::nullopt,
        .status = epistemic::InterpretationStatus::Current,
        .created_at = 1
    };

    // Cycle 1: Compatible Evidence -> Supported
    std::vector<epistemic::Observation> obs_nominal = {
        {
            .id = ev_nominal,
            .source = "front_camera",
            .subject = "path_clear",
            .value = "true",
            .observed_at = 2,
            .status = epistemic::EpistemicStatus::Observed
        }
    };

    auto res1 = rcc_engine.evaluate(current, obs_nominal, judgment);
    ENTE_TEST_ASSERT(res1.compatibility == judgment::CompatibilityResult::Supported);
    ENTE_TEST_ASSERT(res1.epistemic_action == rcc::EpistemicAction::Keep);
    ENTE_TEST_ASSERT(rcc_engine.current_state() == rcc::RCCState::Stable);
    ENTE_TEST_ASSERT(current.status == epistemic::InterpretationStatus::Supported);

    // Cycle 2: Unexpected Motion Anomaly -> Weakened & SuspendAction
    core::EvidenceId ev_anomaly("EV-ANOMALY");
    std::vector<epistemic::Observation> obs_anomaly = {
        {
            .id = ev_anomaly,
            .source = "side_lidar",
            .subject = "unexpected_motion",
            .value = "true",
            .observed_at = 3,
            .status = epistemic::EpistemicStatus::Unknown // C8: Explicit unknown
        }
    };

    auto res2 = rcc_engine.evaluate(current, obs_anomaly, judgment);
    ENTE_TEST_ASSERT(res2.compatibility == judgment::CompatibilityResult::Weakened);
    ENTE_TEST_ASSERT(res2.epistemic_action == rcc::EpistemicAction::SuspendAction);
    ENTE_TEST_ASSERT(rcc_engine.current_state() == rcc::RCCState::Suspended);
    ENTE_TEST_ASSERT(current.status == epistemic::InterpretationStatus::Weakened);
    ENTE_TEST_ASSERT(!current.challenging_evidence.empty());
    ENTE_TEST_ASSERT_EQ(current.challenging_evidence.back(), ev_anomaly);

    std::cout << "[PASS] test_rcc: Continuous Context Reassessment & Action Suspension (C6, C7) verified.\n";
    return 0;
}

