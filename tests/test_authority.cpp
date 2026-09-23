#include "ente/authority/authority.hpp"
#include "ente/realization/runner.hpp"
#include "ente/testing/test_harness.hpp"
#include <iostream>

int main() {
    using namespace ente;

    authority::AuthorityLineage lineage;
    ENTE_TEST_ASSERT(lineage.empty());

    auto root_signer = core::Ed25519KeyPair::generate();
    auto next_signer = core::Ed25519KeyPair::generate();
    ENTE_TEST_ASSERT(root_signer.has_value());
    ENTE_TEST_ASSERT(next_signer.has_value());

    authority::AuthorityId root_auth("auth-genesis-root");
    auto ep0 = lineage.initialize_root_epoch(root_auth, root_signer->public_key_hex(), 0);
    ENTE_TEST_ASSERT(ep0.has_value());
    ENTE_TEST_ASSERT(lineage.verify_lineage_integrity());
    ENTE_TEST_ASSERT(lineage.is_authority_authorized(root_auth, ep0->epoch_id));

    // Double root epoch initialization must be rejected
    auto ep0_dup = lineage.initialize_root_epoch(root_auth, root_signer->public_key_hex(), 0);
    ENTE_TEST_ASSERT(!ep0_dup.has_value());

    // Transition to epoch 1
    authority::AuthorityId next_auth("auth-successor-council");
    const auto delegation = lineage.delegation_message(
        next_auth, next_signer->public_key_hex(), 10);
    const auto signature = root_signer->sign_hex(delegation);
    ENTE_TEST_ASSERT(signature.has_value());
    auto ep1 = lineage.transition_epoch(
        next_auth, next_signer->public_key_hex(), 10, *signature);
    ENTE_TEST_ASSERT(ep1.has_value());
    ENTE_TEST_ASSERT(lineage.verify_lineage_integrity());
    ENTE_TEST_ASSERT(lineage.is_authority_authorized(next_auth, ep1->epoch_id));
    ENTE_TEST_ASSERT(!lineage.is_authority_authorized(root_auth, ep0->epoch_id)); // Old epoch retired
    ENTE_TEST_ASSERT(lineage.is_epoch_legitimate_at(root_auth, ep0->epoch_id, 5));
    ENTE_TEST_ASSERT(!lineage.is_epoch_legitimate_at(root_auth, ep0->epoch_id, 10));
    ENTE_TEST_ASSERT(lineage.is_epoch_legitimate_at(next_auth, ep1->epoch_id, 10));

    // Equal activation times are ambiguous and must also be rejected.
    auto third_signer = core::Ed25519KeyPair::generate();
    ENTE_TEST_ASSERT(third_signer.has_value());
    auto ep_same_time = lineage.transition_epoch(
        authority::AuthorityId("auth-same-time"),
        third_signer->public_key_hex(),
        10,
        *signature
    );
    ENTE_TEST_ASSERT(!ep_same_time.has_value());
    ENTE_TEST_ASSERT(ep_same_time.error() == core::EnteError::InvalidLogicalTime);

    // Temporal regression attack (transitioning to time 5 when current is 10) must be rejected
    authority::AuthorityId rogue_auth("auth-regressive-attacker");
    auto ep_regressive = lineage.transition_epoch(
        rogue_auth, third_signer->public_key_hex(), 5, *signature);
    ENTE_TEST_ASSERT(!ep_regressive.has_value());
    ENTE_TEST_ASSERT(ep_regressive.error() == core::EnteError::InvalidLogicalTime);

    // Empty authority ID must be rejected
    auto ep_empty = lineage.transition_epoch(
        authority::AuthorityId{""}, third_signer->public_key_hex(), 20, *signature);
    ENTE_TEST_ASSERT(!ep_empty.has_value());

    // A signature cannot be replayed for a different epoch, authority or key.
    auto replay = lineage.transition_epoch(
        authority::AuthorityId("auth-replay"),
        third_signer->public_key_hex(),
        20,
        *signature
    );
    ENTE_TEST_ASSERT(!replay.has_value());
    ENTE_TEST_ASSERT(replay.error() == core::EnteError::InvalidSignature);
    ENTE_TEST_ASSERT(lineage.active_epoch().epoch_id == ep1->epoch_id);

    // End-to-end C14: delegation is recorded, recovered and audited from REC.
    realization::EnteRealization process;
    ENTE_TEST_ASSERT(process.genesis(core::IdentityId("ente-authority-e2e")).has_value());
    auto successor_signer = core::Ed25519KeyPair::generate();
    ENTE_TEST_ASSERT(successor_signer.has_value());
    auto transitioned = process.transition_authority(
        authority::AuthorityId("auth-operational-successor"),
        std::move(*successor_signer),
        10
    );
    ENTE_TEST_ASSERT(transitioned.has_value());
    ENTE_TEST_ASSERT(process.history().head().kind == history::EventKind::AuthorityTransition);
    ENTE_TEST_ASSERT(process.verify().is_valid());

    auto recovered = realization::EnteRealization::recover_from_history(process.history());
    ENTE_TEST_ASSERT(recovered.has_value());
    ENTE_TEST_ASSERT(
        recovered->authority_lineage().active_epoch().authorized_authority ==
        authority::AuthorityId("auth-operational-successor"));
    ENTE_TEST_ASSERT(recovered->verify().is_valid());

    auto wrong_signer = core::Ed25519KeyPair::generate();
    ENTE_TEST_ASSERT(wrong_signer.has_value());
    auto attached = recovered->attach_authority_signer(std::move(*wrong_signer));
    ENTE_TEST_ASSERT(!attached.has_value());
    ENTE_TEST_ASSERT(attached.error() == core::EnteError::InvalidSignature);

    std::cout << "[PASS] test_authority: Authority lineage, epochs, cryptographic non-regression & C14 verification passed.\n";
    return 0;
}
