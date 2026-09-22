#include "ente/history/rec.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace ente::history;
    using namespace ente::core;

    RecoverableHistory rec;
    IdentityId id("ente-test-0");

    assert(rec.empty());
    assert(rec.verify_integrity());

    // 1. Append Genesis event
    auto ev0 = rec.create_event(EventKind::Genesis, id, 0, {}, {}, "GENESIS_PAYLOAD");
    auto res0 = rec.append(ev0);
    assert(res0.has_value());
    assert(rec.size() == 1);
    assert(rec.verify_integrity());

    // 2. Append Observation event
    EvidenceId ev_id("EV-001");
    auto ev1 = rec.create_event(EventKind::Observation, id, 1, {ev0.id}, {ev_id}, "OBSERVE_PAYLOAD");
    auto res1 = rec.append(ev1);
    assert(res1.has_value());
    assert(rec.size() == 2);
    assert(rec.verify_integrity());

    // 3. Append Reassessment event
    auto ev2 = rec.create_event(EventKind::Perturbation, id, 2, {ev1.id}, {ev_id}, "PERTURBATION_PAYLOAD");
    auto res2 = rec.append(ev2);
    assert(res2.has_value());
    assert(rec.size() == 3);
    assert(rec.verify_integrity());

    // 4. Test Duplicate EventId rejection (C10/C12 invariant)
    auto ev_duplicate = rec.create_event(EventKind::Observation, id, 3, {ev2.id}, {ev_id}, "DUPLICATE_PAYLOAD");
    ev_duplicate.id = ev0.id; // Forge duplicate id of Genesis
    auto dup_res = rec.append(ev_duplicate);
    assert(!dup_res.has_value());
    assert(dup_res.error() == EnteError::DuplicateEventId);

    // 5. Test Tampering Detection (FAIL-002: C10, C12 falsification)
    rec.tamper_event_payload_for_testing(1, "CORRUPTED_TAMPERED_PAYLOAD");
    assert(!rec.verify_integrity());

    std::cout << "[PASS] test_rec: Hash-chain, causal links, duplicate EventId rejection, and tamper detection (C10, C12) verified.\n";
    return 0;
}
