#include "ente/authority/authority.hpp"
#include "ente/testing/test_harness.hpp"
#include <iostream>

int main() {
    using namespace ente;

    authority::AuthorityLineage lineage;
    ENTE_TEST_ASSERT(lineage.empty());

    authority::AuthorityId root_auth("auth-genesis-root");
    auto ep0 = lineage.initialize_root_epoch(root_auth, 0);
    ENTE_TEST_ASSERT(ep0.has_value());
    ENTE_TEST_ASSERT(lineage.verify_lineage_integrity());
    ENTE_TEST_ASSERT(lineage.is_authority_authorized(root_auth, ep0->epoch_id));

    // Double root epoch initialization must be rejected
    auto ep0_dup = lineage.initialize_root_epoch(root_auth, 0);
    ENTE_TEST_ASSERT(!ep0_dup.has_value());

    // Transition to epoch 1
    authority::AuthorityId next_auth("auth-successor-council");
    auto ep1 = lineage.transition_epoch(next_auth, 10);
    ENTE_TEST_ASSERT(ep1.has_value());
    ENTE_TEST_ASSERT(lineage.verify_lineage_integrity());
    ENTE_TEST_ASSERT(lineage.is_authority_authorized(next_auth, ep1->epoch_id));
    ENTE_TEST_ASSERT(!lineage.is_authority_authorized(root_auth, ep0->epoch_id)); // Old epoch retired

    // Temporal regression attack (transitioning to time 5 when current is 10) must be rejected
    authority::AuthorityId rogue_auth("auth-regressive-attacker");
    auto ep_regressive = lineage.transition_epoch(rogue_auth, 5);
    ENTE_TEST_ASSERT(!ep_regressive.has_value());
    ENTE_TEST_ASSERT(ep_regressive.error() == core::EnteError::InvalidLogicalTime);

    // Empty authority ID must be rejected
    auto ep_empty = lineage.transition_epoch(authority::AuthorityId{""}, 20);
    ENTE_TEST_ASSERT(!ep_empty.has_value());

    std::cout << "[PASS] test_authority: Authority lineage, epochs, cryptographic non-regression & C14 verification passed.\n";
    return 0;
}

