#include "ente/constitution/verifier.hpp"
#include "ente/identity/genesis_service.hpp"
#include "ente/history/rec.hpp"
#include "ente/core/hash.hpp"
#include "ente/testing/test_harness.hpp"
#include "ente/testing/history_fixtures.hpp"
#include <iostream>

int main() {
    using namespace ente;

    constitution::ConstitutionVerifier verifier;
    identity::GenesisService genesis_service;
    history::RecoverableHistory history;

    core::IdentityId id("ente-verifier-test");
    core::Digest const_digest = core::HashUtil::sha256("CONSTITUTION-v0.6.0");
    core::Digest basal_digest = core::HashUtil::sha256("BASAL-STATE-v0.1.0");

    // Before genesis -> verification must fail (C1/C9 violated)
    auto report0 = verifier.verify(genesis_service.state(), genesis_service.record(), history, std::nullopt);
    ENTE_TEST_ASSERT(report0.status == constitution::ConstitutiveStatus::Violated);

    // After genesis and history record -> verification must be valid
    auto gen_res = genesis_service.create_genesis({
        .identity = id,
        .constitution_digest = const_digest,
        .basal_state_digest = basal_digest
    });
    ENTE_TEST_ASSERT(gen_res.has_value());

    auto authority_signer = core::Ed25519KeyPair::generate();
    ENTE_TEST_ASSERT(authority_signer.has_value());
    authority::AuthorityLineage authority_lineage;
    auto root_epoch = authority_lineage.initialize_root_epoch(
        authority::AuthorityId("auth-root"), authority_signer->public_key_hex(), 0);
    ENTE_TEST_ASSERT(root_epoch.has_value());
    const auto genesis_payload = history::serialize_genesis_payload({
        .identity = id,
        .genesis_digest = gen_res->genesis_digest,
        .material_anchor_id = "test-anchor",
        .hardware_fingerprint = "test-fingerprint",
        .substrate_type = "SIMULATED_MEMORY",
        .root_authority_public_key = authority_signer->public_key_hex()
    });
    auto ev_gen = history.create_event(
        history::EventKind::Genesis, id, 0, {}, {}, genesis_payload,
        "auth-root", std::string(root_epoch->epoch_id.view()));
    ENTE_TEST_ASSERT(history.append(ev_gen).has_value());

    auto report1 = verifier.verify(
        genesis_service.state(), genesis_service.record(), history,
        std::nullopt, std::cref(authority_lineage));
    ENTE_TEST_ASSERT(report1.status == constitution::ConstitutiveStatus::Valid);
    ENTE_TEST_ASSERT(report1.is_valid());
    ENTE_TEST_ASSERT(report1.profile == constitution::ConformanceProfile::EnteLocal);

    constitution::ConstitutionVerifier distributed_verifier(
        constitution::ConformanceProfile::EnteDistributed
    );
    auto distributed_report = distributed_verifier.verify(
        genesis_service.state(), genesis_service.record(), history,
        std::nullopt, std::cref(authority_lineage)
    );
    ENTE_TEST_ASSERT(distributed_report.profile == constitution::ConformanceProfile::EnteDistributed);
    ENTE_TEST_ASSERT(distributed_report.status == constitution::ConstitutiveStatus::Suspended);
    size_t distributed_unknowns = 0;
    for (const auto& invariant : distributed_report.invariant_reports) {
        if ((invariant.id == constitution::InvariantId::C11_LineageSingularity ||
             invariant.id == constitution::InvariantId::C13_ConstitutiveFinalitySafety) &&
            invariant.status == constitution::InvariantStatus::Unknown) {
            ++distributed_unknowns;
        }
    }
    ENTE_TEST_ASSERT_EQ(distributed_unknowns, 2U);

    // When history is tampered -> verifier detects and reports Violated (C10/C12)
    history.tamper_event_payload_for_testing(0, "TAMPERED");
    auto report2 = verifier.verify(genesis_service.state(), genesis_service.record(), history, std::nullopt);
    ENTE_TEST_ASSERT(report2.status == constitution::ConstitutiveStatus::Violated);

    // A cryptographically valid event cannot manufacture provenance merely by
    // naming an EvidenceId that was never introduced by an Observation event.
    history::RecoverableHistory forged_provenance;
    auto forged_genesis = forged_provenance.create_event(
        history::EventKind::Genesis, id, 0, {}, {}, genesis_payload,
        "auth-root", std::string(root_epoch->epoch_id.view())
    );
    ENTE_TEST_ASSERT(forged_provenance.append(forged_genesis).has_value());
    auto forged_judgment = forged_provenance.create_event(
        history::EventKind::Judgment,
        id,
        1,
        {forged_genesis.id},
        {core::EvidenceId("EV-NEVER-OBSERVED")},
        history::serialize_judgment_payload({
            .compatibility = "SUPPORTED",
            .rationale = "forged provenance",
            .engine_digest = core::HashUtil::sha256("test-engine")
        })
    );
    const auto forged_result = forged_provenance.append(forged_judgment);
    ENTE_TEST_ASSERT(!forged_result.has_value());
    ENTE_TEST_ASSERT(forged_result.error() == core::EnteError::InsufficientEvidence);

    std::cout << "[PASS] test_verifier: Invariant evaluation (C1..C12) verified.\n";
    return 0;
}
