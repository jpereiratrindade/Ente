#include "ente/identity/material_anchor.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace ente;

    identity::MaterialBindingRegistry registry;
    assert(!registry.has_active_binding());

    core::IdentityId ente_id("ente-theseus-0");

    // 1. Initial Binding at Genesis (Hardware Substrate S0 - TPM Chip A)
    identity::MaterialAnchor anchor_0{
        .id = identity::MaterialAnchorId("tpm-chip-alpha"),
        .type = identity::SubstrateType::TpmProtectedDevice,
        .hardware_fingerprint = "fp-alpha-pubkey-001"
    };

    auto b0 = registry.bind_initial_anchor(ente_id, anchor_0, 0);
    assert(b0.has_value());
    assert(registry.has_active_binding());
    assert(registry.active_binding().identity == ente_id);
    assert(registry.active_binding().anchor.id == anchor_0.id);
    assert(!registry.active_binding().previous_anchor.has_value());

    // 2. Hardware Replacement under RIT (Ship of Theseus: S0 -> S1 - Secure Enclave B)
    // Preserves the EXACT SAME Identity without a new Genesis!
    identity::MaterialAnchor anchor_1{
        .id = identity::MaterialAnchorId("secure-enclave-beta"),
        .type = identity::SubstrateType::SecureEnclave,
        .hardware_fingerprint = "fp-beta-attestation-key-002"
    };

    auto b1 = registry.migrate_to_anchor(anchor_1, 100);
    assert(b1.has_value());
    assert(registry.active_binding().identity == ente_id); // SAME IDENTITY
    assert(registry.active_binding().anchor.id == anchor_1.id); // NEW HARDWARE
    assert(registry.active_binding().previous_anchor.has_value());
    assert(*registry.active_binding().previous_anchor == anchor_0.id); // TRACEABLE LINEAGE

    // Verify history contains both bindings linked causally
    assert(registry.history().size() == 2);
    assert(registry.history()[0].anchor.id == anchor_0.id);
    assert(registry.history()[1].anchor.id == anchor_1.id);

    std::cout << "[PASS] test_material_binding: Hardware substrate replacement preserves Identity under RIT (Ship of Theseus demonstrated).\n";
    return 0;
}
