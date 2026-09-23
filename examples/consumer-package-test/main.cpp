#include <ente/realization/runner.hpp>
#include <ente/constitution/verifier.hpp>
#include <iostream>
#include <cassert>
#include <filesystem>

int main() {
    std::cout << "[Consumer Test] Initializing independent consumer executable linking against Ente::ente_core...\n";

    const std::string journal_path = "consumer_test_journal.rec";
    std::filesystem::remove(journal_path);
    std::filesystem::remove(journal_path + ".tmp");

    {
        ente::realization::EnteRealization ente;
        const auto genesis_res = ente.genesis(ente::core::IdentityId("ente-installed-consumer"));
        assert(genesis_res.has_value());
        std::cout << "  1. Genesis confirmed for: " << ente.identity().id.view() << "\n";

        assert(ente.enable_durable_journal(journal_path).has_value());

        const auto step_res = ente.step(1, {{
            .id = ente::core::EvidenceId("EV-CONSUMER-1"),
            .source = "external_sensor",
            .subject = "path_clear",
            .value = "true",
            .observed_at = 1,
            .status = ente::epistemic::EpistemicStatus::Observed
        }});
        assert(step_res.has_value());
        std::cout << "  2. Factual step executed and persisted.\n";

        const auto report = ente.verify();
        assert(report.status == ente::constitution::ConstitutiveStatus::Valid);
        std::cout << "  3. Constitutional verification: " << (report.is_valid() ? "VALID" : "INVALID") << "\n";
    }

    // Recover in fresh process instance
    {
        auto recovered = ente::realization::EnteRealization::recover_from_file(journal_path);
        assert(recovered.has_value());
        assert(recovered->identity().id.view() == "ente-installed-consumer");
        assert(recovered->history().size() >= 2);
        std::cout << "  4. Recovery verified from exported REC journal: " << recovered->history().size() << " events intact.\n";
    }

    std::filesystem::remove(journal_path);
    std::filesystem::remove(journal_path + ".tmp");

    std::cout << "[SUCCESS] Independent consumer verification complete!\n";
    return 0;
}
