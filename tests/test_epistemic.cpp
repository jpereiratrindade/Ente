#include "ente/epistemic/status.hpp"
#include "ente/epistemic/value.hpp"
#include "ente/epistemic/observation.hpp"
#include "ente/testing/test_harness.hpp"
#include <iostream>

int main() {
    using namespace ente::epistemic;
    using namespace ente::core;

    // Test C5: Epistemic distinction
    ENTE_TEST_ASSERT_EQ(to_string(EpistemicStatus::Observed), "OBSERVED");
    ENTE_TEST_ASSERT_EQ(to_string(EpistemicStatus::Derived), "DERIVED");
    ENTE_TEST_ASSERT_EQ(to_string(EpistemicStatus::Inferred), "INFERRED");
    ENTE_TEST_ASSERT_EQ(to_string(EpistemicStatus::Unknown), "UNKNOWN");
    ENTE_TEST_ASSERT_EQ(to_string(EpistemicStatus::Contradictory), "CONTRADICTORY");

    // Test C8: Unknown representability as a first-class state with provenance
    EvidenceId ev_id("EV-001");
    auto unknown_val = EpistemicValue<std::string>::make_unknown(ev_id, "Unclassified motion sensor event");

    ENTE_TEST_ASSERT(unknown_val.is_unknown());
    ENTE_TEST_ASSERT(!unknown_val.value.has_value());
    ENTE_TEST_ASSERT_EQ(unknown_val.evidence_origin, ev_id);
    ENTE_TEST_ASSERT_EQ(unknown_val.qualification, "Unclassified motion sensor event");

    // Test Observed value
    auto obs_val = EpistemicValue<bool>::make_observed(true, ev_id);
    ENTE_TEST_ASSERT(obs_val.is_observed());
    ENTE_TEST_ASSERT(obs_val.value.has_value() && *obs_val.value == true);

    // Test Contradictory value
    auto contra_val = EpistemicValue<bool>::make_contradictory(ev_id, "Lidar and Camera disagree");
    ENTE_TEST_ASSERT(contra_val.is_contradictory());

    std::cout << "[PASS] test_epistemic: Epistemic distinction & UNKNOWN representation (C5, C8) verified.\n";
    return 0;
}

