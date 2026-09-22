#include "ente/identity/genesis_service.hpp"
#include "ente/core/hash.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace ente;

    identity::GenesisService service;

    assert(!service.has_genesis());
    assert(service.state().lifecycle == identity::LifecycleStatus::PreGenesis);

    core::IdentityId id("ente-test-0");
    core::Digest const_digest = core::HashUtil::sha256("CONSTITUTION-v0.6.0");
    core::Digest basal_digest = core::HashUtil::sha256("BASAL-STATE-v0.1.0");

    auto res1 = service.create_genesis({
        .identity = id,
        .constitution_digest = const_digest,
        .basal_state_digest = basal_digest
    });

    assert(res1.has_value());
    assert(service.has_genesis());
    assert(service.state().lifecycle == identity::LifecycleStatus::LifeActive);
    assert(service.state().id == id);
    assert(service.verify(*res1));

    // FAIL-001: Double genesis on the same entity must be rejected
    auto res2 = service.create_genesis({
        .identity = id,
        .constitution_digest = const_digest,
        .basal_state_digest = basal_digest
    });

    assert(!res2.has_value());
    assert(res2.error() == core::EnteError::GenesisAlreadyExists);

    std::cout << "[PASS] test_genesis: Single Genesis & Invariant Anchor C1/C9 verified.\n";
    return 0;
}
