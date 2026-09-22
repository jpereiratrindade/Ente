#include "ente/constitution/verifier.hpp"
#include "ente/identity/genesis_service.hpp"
#include "ente/history/rec.hpp"
#include "ente/core/hash.hpp"
#include <cassert>
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
    assert(report0.status == constitution::ConstitutiveStatus::Violated);

    // After genesis and history record -> verification must be valid
    auto gen_res = genesis_service.create_genesis({
        .identity = id,
        .constitution_digest = const_digest,
        .basal_state_digest = basal_digest
    });
    assert(gen_res.has_value());

    auto ev_gen = history.create_event(history::EventKind::Genesis, id, 0, {}, {}, "GENESIS");
    assert(history.append(ev_gen).has_value());

    auto report1 = verifier.verify(genesis_service.state(), genesis_service.record(), history, std::nullopt);
    assert(report1.status == constitution::ConstitutiveStatus::Valid);
    assert(report1.is_valid());

    // When history is tampered -> verifier detects and reports Violated (C10/C12)
    history.tamper_event_payload_for_testing(0, "TAMPERED");
    auto report2 = verifier.verify(genesis_service.state(), genesis_service.record(), history, std::nullopt);
    assert(report2.status == constitution::ConstitutiveStatus::Violated);

    std::cout << "[PASS] test_verifier: Invariant evaluation (C1..C12) verified.\n";
    return 0;
}
