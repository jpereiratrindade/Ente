#include "ente/identity/genesis_service.hpp"
#include "ente/core/hash.hpp"
#include "ente/testing/test_harness.hpp"
#include "ente/realization/runner.hpp"
#include <iostream>

int main() {
    using namespace ente;

    identity::GenesisService service;

    ENTE_TEST_ASSERT(!service.has_genesis());
    ENTE_TEST_ASSERT_EQ(service.state().lifecycle, identity::LifecycleStatus::PreGenesis);

    core::IdentityId id("ente-test-0");
    core::Digest const_digest = core::HashUtil::sha256("CONSTITUTION-v0.6.0");
    core::Digest basal_digest = core::HashUtil::sha256("BASAL-STATE-v0.1.0");

    auto res1 = service.create_genesis({
        .identity = id,
        .constitution_digest = const_digest,
        .basal_state_digest = basal_digest
    });

    ENTE_TEST_ASSERT(res1.has_value());
    ENTE_TEST_ASSERT(service.has_genesis());
    ENTE_TEST_ASSERT_EQ(service.state().lifecycle, identity::LifecycleStatus::LifeActive);
    ENTE_TEST_ASSERT_EQ(service.state().id, id);
    ENTE_TEST_ASSERT(service.verify(*res1));

    // FAIL-001: Double genesis on the same entity must be rejected
    auto res2 = service.create_genesis({
        .identity = id,
        .constitution_digest = const_digest,
        .basal_state_digest = basal_digest
    });

    ENTE_TEST_ASSERT(!res2.has_value());
    ENTE_TEST_ASSERT_EQ(res2.error(), core::EnteError::GenesisAlreadyExists);

    // A failed composed genesis must not leave a partially born realization.
    realization::EnteRealization realization;
    identity::MaterialAnchor invalid_anchor{
        .id = identity::MaterialAnchorId(""),
        .type = identity::SubstrateType::SimulatedMemory,
        .hardware_fingerprint = "fingerprint"
    };
    auto failed_genesis = realization.genesis(
        core::IdentityId("ente-atomic-genesis"), invalid_anchor);
    ENTE_TEST_ASSERT(!failed_genesis.has_value());
    ENTE_TEST_ASSERT(realization.history().empty());
    ENTE_TEST_ASSERT(realization.identity().lifecycle == identity::LifecycleStatus::PreGenesis);
    ENTE_TEST_ASSERT(realization.genesis(core::IdentityId("ente-atomic-genesis")).has_value());

    ENTE_TEST_ASSERT(realization.step(10, {{
        .id = core::EvidenceId("EV-ATOMIC-MIGRATION"),
        .source = "clocked-sensor",
        .subject = "path_clear",
        .value = "true",
        .observed_at = 10,
        .status = epistemic::EpistemicStatus::Observed
    }}).has_value());
    const auto history_size_before = realization.history().size();
    const auto bindings_before = realization.material_bindings().history().size();
    identity::MaterialAnchor next_anchor{
        .id = identity::MaterialAnchorId("anchor-next"),
        .type = identity::SubstrateType::SecureEnclave,
        .hardware_fingerprint = "fingerprint-next"
    };
    auto failed_migration = realization.migrate_hardware(next_anchor, 5);
    ENTE_TEST_ASSERT(!failed_migration.has_value());
    ENTE_TEST_ASSERT_EQ(realization.history().size(), history_size_before);
    ENTE_TEST_ASSERT_EQ(realization.material_bindings().history().size(), bindings_before);

    std::cout << "[PASS] test_genesis: Single Genesis & Invariant Anchor C1/C9 verified.\n";
    return 0;
}
