#include "ente/identity/material_anchor.hpp"
#include "ente/testing/test_harness.hpp"
#include <iostream>

int main() {
    using namespace ente;

    identity::MaterialBindingRegistry registry;
    ENTE_TEST_ASSERT(!registry.has_active_binding());

    core::IdentityId ente_id("ente-theseus-0");

    // 1. Initial Binding at Genesis (Hardware Substrate S0 - TPM Chip A)
    identity::MaterialAnchor anchor_0{
        .id = identity::MaterialAnchorId("tpm-chip-alpha"),
        .type = identity::SubstrateType::TpmProtectedDevice,
        .hardware_fingerprint = "fp-alpha-pubkey-001"
    };

    auto b0 = registry.bind_initial_anchor(ente_id, anchor_0, 0);
    ENTE_TEST_ASSERT(b0.has_value());
    ENTE_TEST_ASSERT(registry.has_active_binding());
    ENTE_TEST_ASSERT_EQ(registry.active_binding().identity, ente_id);
    ENTE_TEST_ASSERT_EQ(registry.active_binding().anchor.id, anchor_0.id);
    ENTE_TEST_ASSERT(!registry.active_binding().previous_anchor.has_value());

    // 2. Hardware Replacement under RIT (Ship of Theseus: S0 -> S1 - Secure Enclave B)
    // Preserves the EXACT SAME Identity without a new Genesis!
    identity::MaterialAnchor anchor_1{
        .id = identity::MaterialAnchorId("secure-enclave-beta"),
        .type = identity::SubstrateType::SecureEnclave,
        .hardware_fingerprint = "fp-beta-attestation-key-002"
    };

    auto b1 = registry.migrate_to_anchor(anchor_1, 100);
    ENTE_TEST_ASSERT(b1.has_value());
    ENTE_TEST_ASSERT_EQ(registry.active_binding().identity, ente_id); // SAME IDENTITY
    ENTE_TEST_ASSERT_EQ(registry.active_binding().anchor.id, anchor_1.id); // NEW HARDWARE
    ENTE_TEST_ASSERT(registry.active_binding().previous_anchor.has_value());
    ENTE_TEST_ASSERT_EQ(*registry.active_binding().previous_anchor, anchor_0.id); // TRACEABLE LINEAGE

    // Verify history contains both bindings linked causally
    ENTE_TEST_ASSERT_EQ(registry.history().size(), 2);
    ENTE_TEST_ASSERT_EQ(registry.history()[0].anchor.id, anchor_0.id);
    ENTE_TEST_ASSERT_EQ(registry.history()[1].anchor.id, anchor_1.id);

    std::cout << "[PASS] test_material_binding: Hardware substrate replacement preserves Identity under RIT (Ship of Theseus demonstrated).\n";
    return 0;
}
