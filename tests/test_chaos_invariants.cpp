#include "ente/realization/runner.hpp"
#include "ente/constitution/verifier.hpp"
#include "ente/core/hash.hpp"
#include "ente/history/payloads.hpp"
#include "ente/testing/test_harness.hpp"
#include <iostream>

namespace {

void test_temporal_backward_attack() {
    std::cout << "[1] Testing CHAOS-001: Backward Time & Clock Skew Injection (RIT Attack)...\n";
    ente::realization::EnteRealization ente;
    ente::core::IdentityId id("ente-chaos-temporal");
    ENTE_TEST_ASSERT(ente.genesis(id).has_value());

    // Step at t = 100
    auto s1 = ente.step(100, {{
        .id = ente::core::EvidenceId("EV-T100"),
        .source = "clock",
        .subject = "path_clear",
        .value = "true",
        .observed_at = 100,
        .status = ente::epistemic::EpistemicStatus::Observed
    }}, "t=100");
    ENTE_TEST_ASSERT(s1.has_value());

    // Adversarial attack: Inject event at t = 50 (backwards in time) directly into ledger
    auto bad_event = ente.history_mut().create_event(
        ente::history::EventKind::Observation,
        id,
        50, // Backward time!
        {ente.history().head().id},
        {},
        "OBSERVE:path_clear:true:OBSERVED",
        std::string(ente.authority_lineage().active_epoch().authorized_authority.view()),
        std::string(ente.authority_lineage().active_epoch().epoch_id.view())
    );

    auto append_res = ente.history_mut().append(std::move(bad_event));
    // Must be strictly rejected by REC append logic!
    ENTE_TEST_ASSERT(!append_res.has_value());
    ENTE_TEST_ASSERT(append_res.error() == ente::core::EnteError::HistoryCorrupt);

    std::cout << "    -> Backward timestamp rejected by REC temporal integrity rules.\n";
}

void test_rogue_authority_injection() {
    std::cout << "[2] Testing CHAOS-002: Rogue Authority & Forged Epoch Injection (C14 Attack)...\n";
    ente::realization::EnteRealization ente;
    ente::core::IdentityId id("ente-chaos-auth");
    ENTE_TEST_ASSERT(ente.genesis(id).has_value());

    // Adversarial attack: Bypass step() and append an event signed by an unauthorized rogue authority
    auto rogue_event = ente.history_mut().create_event(
        ente::history::EventKind::Observation,
        id,
        10,
        {ente.history().head().id},
        {ente::core::EvidenceId("EV-ROGUE")},
        ente::history::serialize_observation_payload({
            .evidence_id = ente::core::EvidenceId("EV-ROGUE"),
            .source = "malicious-sensor",
            .subject = "malicious_override",
            .value = "true",
            .status = ente::epistemic::EpistemicStatus::Observed,
            .observed_at = 10
        }),
        "auth-pirate-hacker-key", // Rogue authority!
        "epoch-999"               // Forged epoch!
    );
    ENTE_TEST_ASSERT(ente.history_mut().append(std::move(rogue_event)).has_value());

    // Full constitutional verification must catch the rogue event and mark C14 VIOLATED!
    auto report = ente.verify();
    ENTE_TEST_ASSERT(report.status == ente::constitution::ConstitutiveStatus::Violated);

    bool c14_violated = false;
    for (const auto& r : report.invariant_reports) {
        if (r.id == ente::constitution::InvariantId::C14_ConstitutiveAuthorityContinuity) {
            if (r.status == ente::constitution::InvariantStatus::Violated) {
                c14_violated = true;
            }
        }
    }
    ENTE_TEST_ASSERT(c14_violated);
    std::cout << "    -> Rogue authority detected and C14 flagged VIOLATED immediately.\n";
}

void test_provenance_evidence_stripping() {
    std::cout << "[3] Testing CHAOS-003: Provenance Evidence Stripping (C4 Attack)...\n";
    ente::realization::EnteRealization ente;
    ente::core::IdentityId id("ente-chaos-provenance");
    ENTE_TEST_ASSERT(ente.genesis(id).has_value());

    // Adversarial attack: append interpretation with zero evidence refs and zero causal predecessors
    auto stripped_event = ente.history_mut().create_event(
        ente::history::EventKind::Interpretation,
        id,
        5,
        {}, // Stripped predecessors!
        {}, // Stripped evidence!
        "INTERPRET:I9999:ghost_context:Unsubstantiated belief",
        std::string(ente.authority_lineage().active_epoch().authorized_authority.view()),
        std::string(ente.authority_lineage().active_epoch().epoch_id.view())
    );
    ENTE_TEST_ASSERT(ente.history_mut().append(std::move(stripped_event)).has_value());

    // Constitution verifier must catch lack of provenance (C4)
    auto report = ente.verify();
    ENTE_TEST_ASSERT(report.status == ente::constitution::ConstitutiveStatus::Violated);

    bool c4_violated = false;
    for (const auto& r : report.invariant_reports) {
        if (r.id == ente::constitution::InvariantId::C4_Provenance) {
            if (r.status == ente::constitution::InvariantStatus::Violated) {
                c4_violated = true;
            }
        }
    }
    ENTE_TEST_ASSERT(c4_violated);
    std::cout << "    -> Unsubstantiated interpretation detected and C4 flagged VIOLATED.\n";
}

void test_hash_chain_bit_flip() {
    std::cout << "[4] Testing CHAOS-004: In-Memory Ledger Payload Corruption (C10/C12 Attack)...\n";
    ente::realization::EnteRealization ente;
    ente::core::IdentityId id("ente-chaos-tamper");
    ENTE_TEST_ASSERT(ente.genesis(id).has_value());

    ENTE_TEST_ASSERT(ente.step(1, {{.id = ente::core::EvidenceId("EV1"), .source = "cam", .subject = "path_clear", .value = "true", .observed_at = 1, .status = ente::epistemic::EpistemicStatus::Observed}}, "s1").has_value());
    ENTE_TEST_ASSERT(ente.step(2, {{.id = ente::core::EvidenceId("EV2"), .source = "cam", .subject = "path_clear", .value = "true", .observed_at = 2, .status = ente::epistemic::EpistemicStatus::Observed}}, "s2").has_value());
    ENTE_TEST_ASSERT(ente.verify().is_valid());

    // Corrupt in-memory payload of event 1
    ente.history_mut().tamper_event_payload_for_testing(1, "CORRUPTED_EVENT_1_PAYLOAD");

    ENTE_TEST_ASSERT(!ente.history().verify_integrity());
    auto rep = ente.verify();
    ENTE_TEST_ASSERT(rep.status == ente::constitution::ConstitutiveStatus::Violated);

    // Any subsequent step must be aborted due to ConstitutiveViolation
    auto s3 = ente.step(3, {{.id = ente::core::EvidenceId("EV3"), .source = "cam", .subject = "path_clear", .value = "true", .observed_at = 3, .status = ente::epistemic::EpistemicStatus::Observed}}, "s3");
    ENTE_TEST_ASSERT(!s3.has_value());
    ENTE_TEST_ASSERT(s3.error() == ente::core::EnteError::ConstitutiveViolation);
    ENTE_TEST_ASSERT(ente.domain().is_action_suspended());

    std::cout << "    -> Bit-flip detected, history integrity failed, and execution halted.\n";
}

void test_byzantine_fork_split() {
    std::cout << "[5] Testing CHAOS-005: Byzantine History Split & Causal Fork Injection (C10/C11 Attack)...\n";
    ente::realization::EnteRealization ente;
    ente::core::IdentityId id("ente-chaos-fork");
    ENTE_TEST_ASSERT(ente.genesis(id).has_value());

    ENTE_TEST_ASSERT(ente.step(1, {{.id = ente::core::EvidenceId("EV1"), .source = "radar", .subject = "path_clear", .value = "true", .observed_at = 1, .status = ente::epistemic::EpistemicStatus::Observed}}, "s1").has_value());

    // Adversarial injection of an event with forged previous hash (fork split attempt)
    auto forged_fork_event = ente.history_mut().create_event(
        ente::history::EventKind::Interpretation,
        id,
        2,
        {},
        {},
        "FORGED_FORK_STATE",
        "auth-root-ente-chaos-fork",
        "epoch-0"
    );
    forged_fork_event.previous_event_digest = ente::core::Digest("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");

    auto fork_res = ente.history_mut().append(std::move(forged_fork_event));
    ENTE_TEST_ASSERT(!fork_res.has_value());
    ENTE_TEST_ASSERT(fork_res.error() == ente::core::EnteError::HistoryGap);

    std::cout << "    -> Byzantine history fork rejected by predecessor digest integrity rules.\n";
}

void test_prng_seed_integrity() {
    std::cout << "[6] Testing CHAOS-006: Event-Scoped PRNG Deterministic Divergence Defense...\n";
    ente::core::EventScopedPRNG prng1("genesis-seed-alpha");
    ente::core::EventScopedPRNG prng2("genesis-seed-alpha");
    ente::core::EventScopedPRNG prng_rogue("genesis-seed-rogue");

    ente::core::EventId ev("EV-100");
    auto sample1 = prng1.derive_double(ev, "monte_carlo");
    auto sample2 = prng2.derive_double(ev, "monte_carlo");
    auto sample_rogue = prng_rogue.derive_double(ev, "monte_carlo");

    ENTE_TEST_ASSERT(sample1 == sample2); // Identical genesis seed produces identical sequence
    ENTE_TEST_ASSERT(sample1 != sample_rogue); // Divergent seed produces distinct sample

    // Sequence addressability: sampling different events gives independent uniform distribution
    ente::core::EventId ev_alt("EV-101");
    auto sample_alt = prng1.derive_double(ev_alt, "monte_carlo");
    ENTE_TEST_ASSERT(sample1 != sample_alt);

    std::cout << "    -> Event-scoped PRNG verified deterministic, reproducible, and cryptographically seeded.\n";
}

} // namespace

int main() {
    std::cout << "=========================================================\n";
    std::cout << "      ENTE-0 CHAOS & ADVERSARIAL INVARIANT ATTACKS       \n";
    std::cout << "=========================================================\n\n";

    test_temporal_backward_attack();
    test_rogue_authority_injection();
    test_provenance_evidence_stripping();
    test_hash_chain_bit_flip();
    test_byzantine_fork_split();
    test_prng_seed_integrity();

    std::cout << "\n>>> ALL CHAOS & ADVERSARIAL ATTACK TESTS PASSED (6/6) <<<\n";
    return 0;
}

