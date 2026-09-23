#include "ente/history/rec.hpp"
#include "ente/testing/test_harness.hpp"
#include "ente/testing/history_fixtures.hpp"
#include <iostream>

int main() {
    using namespace ente::history;
    using namespace ente::core;

    RecoverableHistory rec;
    IdentityId id("ente-test-0");

    ENTE_TEST_ASSERT(rec.empty());
    ENTE_TEST_ASSERT(rec.verify_integrity());

    RecoverableHistory missing_genesis;
    auto non_genesis = missing_genesis.create_event(
        EventKind::Observation,
        id,
        0,
        {},
        {EvidenceId("EV-NO-GENESIS")},
        ente::testing::canonical_observation_payload(EvidenceId("EV-NO-GENESIS"), 0)
    );
    auto missing_genesis_result = missing_genesis.append(std::move(non_genesis));
    ENTE_TEST_ASSERT(!missing_genesis_result.has_value());
    ENTE_TEST_ASSERT(missing_genesis_result.error() == EnteError::GenesisNotEstablished);

    // 1. Append Genesis event
    auto ev0 = rec.create_event(
        EventKind::Genesis, id, 0, {}, {}, ente::testing::canonical_genesis_payload(id));
    auto res0 = rec.append(ev0);
    ENTE_TEST_ASSERT(res0.has_value());
    ENTE_TEST_ASSERT_EQ(rec.size(), 1);
    ENTE_TEST_ASSERT(rec.verify_integrity());

    // 2. Append Observation event
    EvidenceId ev_id("EV-001");
    auto ev1 = rec.create_event(
        EventKind::Observation, id, 1, {ev0.id}, {ev_id},
        ente::testing::canonical_observation_payload(ev_id, 1));
    auto res1 = rec.append(ev1);
    ENTE_TEST_ASSERT(res1.has_value());
    ENTE_TEST_ASSERT_EQ(rec.size(), 2);
    ENTE_TEST_ASSERT(rec.verify_integrity());

    // 3. Append Reassessment event
    auto ev2 = rec.create_event(
        EventKind::Perturbation, id, 2, {ev1.id}, {ev_id},
        serialize_perturbation_payload({
            .compatibility = "WEAKENED",
            .reason = "test perturbation",
            .recommended_action = ente::rcc::EpistemicAction::SuspendAction
        }));
    auto res2 = rec.append(ev2);
    ENTE_TEST_ASSERT(res2.has_value());
    ENTE_TEST_ASSERT_EQ(rec.size(), 3);
    ENTE_TEST_ASSERT(rec.verify_integrity());

    // A self-consistent event from another identity must never enter the lineage.
    auto foreign = rec.create_event(
        EventKind::Observation,
        IdentityId("ente-foreign"),
        3,
        {ev2.id},
        {EvidenceId("EV-FOREIGN")},
        ente::testing::canonical_observation_payload(EvidenceId("EV-FOREIGN"), 3)
    );
    auto foreign_result = rec.append(std::move(foreign));
    ENTE_TEST_ASSERT(!foreign_result.has_value());
    ENTE_TEST_ASSERT(foreign_result.error() == EnteError::IdentityMismatch);

    // 4. Test Duplicate EventId rejection (C10/C12 invariant)
    auto ev_duplicate = rec.create_event(
        EventKind::Observation, id, 3, {ev2.id}, {ev_id},
        ente::testing::canonical_observation_payload(ev_id, 3));
    ev_duplicate.id = ev0.id; // Forge duplicate id of Genesis
    auto dup_res = rec.append(ev_duplicate);
    ENTE_TEST_ASSERT(!dup_res.has_value());
    ENTE_TEST_ASSERT(dup_res.error() == EnteError::DuplicateEventId);

    // 5. Test Tampering Detection (FAIL-002: C10, C12 falsification)
    rec.tamper_event_payload_for_testing(1, "CORRUPTED_TAMPERED_PAYLOAD");
    ENTE_TEST_ASSERT(!rec.verify_integrity());

    std::cout << "[PASS] test_rec: Hash-chain, causal links, duplicate EventId rejection, and tamper detection (C10, C12) verified.\n";
    return 0;
}
