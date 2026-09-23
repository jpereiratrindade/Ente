#include "ente/core/prng.hpp"
#include "ente/testing/test_harness.hpp"
#include <iostream>

int main() {
    using namespace ente;

    core::EventScopedPRNG prng("global-genesis-entropy-seed-42");

    core::EventId ev1("E0001");
    core::EventId ev2("E0002");

    // 1. Derive values for Event 1 and Event 2
    uint64_t val_e1_pA = prng.derive_u64(ev1, "exploration_decision");
    uint64_t val_e2_pA = prng.derive_u64(ev2, "sensor_noise_filter");

    // 2. Determinism check
    ENTE_TEST_ASSERT_EQ(val_e1_pA, prng.derive_u64(ev1, "exploration_decision"));
    ENTE_TEST_ASSERT_EQ(val_e2_pA, prng.derive_u64(ev2, "sensor_noise_filter"));

    // 3. Different events / purposes produce distinct entropy
    ENTE_TEST_ASSERT_NE(val_e1_pA, val_e2_pA);

    // 4. Immunity to interleaved invocations (Adding random calls to E1 DOES NOT shift E2!)
    uint64_t extra_call_1 = prng.derive_u64(ev1, "extra_debug_call_1");
    uint64_t extra_call_2 = prng.derive_u64(ev1, "extra_debug_call_2");
    (void)extra_call_1;
    (void)extra_call_2;

    // E2 remains 100% identical even after intermediate calls to E1
    ENTE_TEST_ASSERT_EQ(val_e2_pA, prng.derive_u64(ev2, "sensor_noise_filter"));

    // Floating-point derivation in [0.0, 1.0)
    double d1 = prng.derive_double(ev1, "exploration_rate");
    ENTE_TEST_ASSERT(d1 >= 0.0 && d1 < 1.0);

    std::cout << "[PASS] test_prng: Event-scoped deterministic entropy verified (immune to sequence shifting).\n";
    return 0;
}

