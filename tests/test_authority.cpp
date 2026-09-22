#include "ente/authority/authority.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace ente;

    authority::AuthorityLineage lineage;
    assert(lineage.empty());

    authority::AuthorityId root_auth("auth-genesis-root");
    auto ep0 = lineage.initialize_root_epoch(root_auth, 0);
    assert(ep0.has_value());
    assert(lineage.verify_lineage_integrity());
    assert(lineage.is_authority_authorized(root_auth, ep0->epoch_id));

    // Double root epoch initialization must be rejected
    auto ep0_dup = lineage.initialize_root_epoch(root_auth, 0);
    assert(!ep0_dup.has_value());

    // Transition to epoch 1
    authority::AuthorityId next_auth("auth-successor-council");
    auto ep1 = lineage.transition_epoch(next_auth, 10);
    assert(ep1.has_value());
    assert(lineage.verify_lineage_integrity());
    assert(lineage.is_authority_authorized(next_auth, ep1->epoch_id));
    assert(!lineage.is_authority_authorized(root_auth, ep0->epoch_id)); // Old epoch retired

    std::cout << "[PASS] test_authority: Authority lineage, epochs & C14 verification passed.\n";
    return 0;
}
