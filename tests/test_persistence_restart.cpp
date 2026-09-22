#include "ente/realization/runner.hpp"
#include "ente/history/rec.hpp"
#include <cassert>
#include <iostream>
#include <filesystem>

int main() {
    using namespace ente;

    std::string test_file = "test_persistence_restart.rec";
    if (std::filesystem::exists(test_file)) {
        std::filesystem::remove(test_file);
    }

    core::IdentityId id("ente-persistent-0");

    // ==========================================
    // Phase 1: Process A runs, creates Genesis,
    // records history and persists to disk.
    // ==========================================
    {
        realization::EnteRealization process_a;
        auto gen_res = process_a.genesis(id);
        assert(gen_res.has_value());

        // Step 1
        core::EvidenceId ev1("EV-P1");
        auto s1 = process_a.step(1, {{
            .id = ev1,
            .source = "sensor_alpha",
            .subject = "path_clear",
            .value = "true",
            .observed_at = 1,
            .status = epistemic::EpistemicStatus::Observed
        }}, "Nominal step in Process A");
        assert(s1.has_value());

        // Save history to disk
        auto save_res = process_a.history().save_to_file(test_file);
        assert(save_res.has_value());
        assert(process_a.history().size() >= 3); // Genesis, Obs, Interp, ActionExecution
    } // Process A terminates completely here

    // ==========================================
    // Phase 2: Fresh Process B starts with clean memory.
    // Recovers from disk and rejects 2nd Genesis.
    // ==========================================
    {
        // 1. Cold recovery: Instantiate Process B directly from the persisted REC file
        auto recover_res = realization::EnteRealization::recover_from_file(test_file);
        assert(recover_res.has_value());

        auto& process_b = *recover_res;
        size_t initial_b_size = process_b.history().size();
        assert(initial_b_size >= 3);
        assert(process_b.identity().id == id);
        assert(process_b.identity().lifecycle == identity::LifecycleStatus::LifeActive);
        assert(process_b.history().verify_integrity());

        // 2. ATTEMPT 2ND GENESIS ON RECOVERED ENTE -> MUST BE REJECTED!
        auto gen2_res = process_b.genesis(id);
        assert(!gen2_res.has_value());
        assert(gen2_res.error() == core::EnteError::GenesisAlreadyExists);

        // 3. Process B continues life: executes Step 2
        core::EvidenceId ev2("EV-P2");
        auto s2 = process_b.step(2, {{
            .id = ev2,
            .source = "sensor_alpha",
            .subject = "path_clear",
            .value = "true",
            .observed_at = 2,
            .status = epistemic::EpistemicStatus::Observed
        }}, "Step in Process B after cold recovery");
        assert(s2.has_value());
        assert(process_b.history().size() > initial_b_size); // Continuous uninterrupted history

        // 4. Verification on recovered and evolved ENTE
        auto rep = process_b.verify();
        assert(rep.is_valid());
    }

    // Cleanup
    std::filesystem::remove(test_file);

    std::cout << "[PASS] test_persistence_restart: Cold recovery from file, 2nd Genesis rejection & life continuation verified.\n";
    return 0;
}
