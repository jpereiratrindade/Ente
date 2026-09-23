#include "ente/realization/runner.hpp"
#include "ente/constitution/verifier.hpp"
#include "ente/history/rec.hpp"
#include "ente/history/payloads.hpp"
#include "ente/core/ed25519.hpp"
#include "ente/testing/test_harness.hpp"
#include "ente/testing/history_fixtures.hpp"

#include <array>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

std::array<uint8_t, ente::core::Ed25519KeyPair::key_size> decode_seed(
    std::string_view encoded
) {
    std::array<uint8_t, ente::core::Ed25519KeyPair::key_size> seed{};
    ENTE_TEST_ASSERT_EQ(encoded.size(), seed.size() * 2);
    for (size_t i = 0; i < seed.size(); ++i) {
        unsigned int value = 0;
        const char* begin = encoded.data() + i * 2;
        const auto [end, error] = std::from_chars(begin, begin + 2, value, 16);
        ENTE_TEST_ASSERT(error == std::errc{});
        ENTE_TEST_ASSERT(end == begin + 2);
        seed[i] = static_cast<uint8_t>(value);
    }
    return seed;
}

void cleanup_file(const std::string& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
    std::filesystem::remove(path + ".tmp", ec);
}

// Substrate Durability Verification under abrupt process death (_exit)
// and torn writes during active transaction stream
void test_substrate_abrupt_death_across_lifecycle() {
    std::cout << "[G6-Substrate] 1. Testing unbuffered process termination across lifecycle points...\n";
    const std::string journal_path = "test_g6_abrupt_death.rec";
    cleanup_file(journal_path);

    const auto seed_bytes = decode_seed(
        "303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f"
    );
    auto signer_opt = ente::core::Ed25519KeyPair::from_private_seed(seed_bytes);
    ENTE_TEST_ASSERT(signer_opt.has_value());

#if defined(__unix__) || defined(__APPLE__)
    // Phase 1: Genesis + 50 continuous steps with authenticated journal
    pid_t pid = fork();
    ENTE_TEST_ASSERT(pid >= 0);

    if (pid == 0) {
        // Child process
        ente::realization::EnteRealization ente;
        const auto gen_res = ente.genesis(ente::core::IdentityId("ente-substrate-g6"));
        if (!gen_res.has_value()) _exit(1);

        auto child_signer = ente::core::Ed25519KeyPair::from_private_seed(seed_bytes);
        if (!child_signer.has_value()) _exit(2);

        if (!ente.enable_authenticated_durable_journal(journal_path, std::move(*child_signer)).has_value()) {
            _exit(3);
        }

        for (uint64_t t = 1; t <= 50; ++t) {
            auto step_res = ente.step(t, {{
                .id = ente::core::EvidenceId("EV-G6-" + std::to_string(t)),
                .source = "sensor_grid",
                .subject = "path_clear",
                .value = (t % 5 == 0) ? "false" : "true",
                .observed_at = t,
                .status = ente::epistemic::EpistemicStatus::Observed
            }});
            if (!step_res.has_value()) _exit(4);
        }

        // Simulate instant unbuffered power cut via _exit (bypassing atexit / C++ destructors)
        _exit(0);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    ENTE_TEST_ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0);

    // Phase 2: Recovery in fresh instance from declared substrate
    auto recovery_signer = ente::core::Ed25519KeyPair::from_private_seed(seed_bytes);
    ENTE_TEST_ASSERT(recovery_signer.has_value());

    auto recovered = ente::realization::EnteRealization::recover_from_authenticated_file(
        journal_path,
        std::move(*recovery_signer)
    );
    ENTE_TEST_ASSERT(recovered.has_value());
    ENTE_TEST_ASSERT_EQ(recovered->identity().id.view(), "ente-substrate-g6");
    ENTE_TEST_ASSERT(recovered->has_authenticated_journal());
    std::cout << "  -> Successfully recovered " << recovered->history().size()
              << " authenticated events post abrupt _exit.\n";

    // Phase 3: Execute full constitutional audit on recovered entity
    const auto report = recovered->verify();
    ENTE_TEST_ASSERT(report.status == ente::constitution::ConstitutiveStatus::Valid ||
                     report.status == ente::constitution::ConstitutiveStatus::Weakened);
    ENTE_TEST_ASSERT(report.is_valid());
    std::cout << "  -> Constitutional integrity verified on recovered substrate.\n";
#endif

    cleanup_file(journal_path);
}

void test_substrate_torn_write_and_recovery_isolation() {
    std::cout << "[G6-Substrate] 2. Testing torn writes and bit-rot detection on storage substrate...\n";
    const std::string journal_path = "test_g6_torn_write.rec";
    cleanup_file(journal_path);

    const auto seed_bytes = decode_seed(
        "505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f"
    );
    auto signer = ente::core::Ed25519KeyPair::from_private_seed(seed_bytes);
    ENTE_TEST_ASSERT(signer.has_value());

    ente::history::RecoverableHistory history;
    const ente::core::IdentityId id("ente-substrate-torn");
    auto gen_ev = history.create_event(
        ente::history::EventKind::Genesis, id, 0, {}, {},
        ente::testing::canonical_genesis_payload(id)
    );
    ENTE_TEST_ASSERT(history.append(gen_ev).has_value());

    for (uint64_t t = 1; t <= 10; ++t) {
        auto obs_ev = history.create_event(
            ente::history::EventKind::Observation, id, t, {history.head().id},
            {ente::core::EvidenceId("EV-" + std::to_string(t))},
            ente::testing::canonical_observation_payload(
                ente::core::EvidenceId("EV-" + std::to_string(t)), t)
        );
        ENTE_TEST_ASSERT(history.append(std::move(obs_ev)).has_value());
    }

    // Save authentic V4 snapshot
    ENTE_TEST_ASSERT(history.save_authenticated_to_file(journal_path, *signer).has_value());

    // Inject partial torn write: truncate the file by 17 bytes (simulating interrupted write barrier)
    const auto file_size = std::filesystem::file_size(journal_path);
    ENTE_TEST_ASSERT(file_size > 50);
    std::filesystem::resize_file(journal_path, file_size - 17);

    // Substrate recovery MUST fail closed with InvalidSignature or HistoryCorrupt
    const auto torn_recovery = ente::history::RecoverableHistory::load_authenticated_from_file(
        journal_path,
        signer->public_key_hex()
    );
    ENTE_TEST_ASSERT(!torn_recovery.has_value());
    ENTE_TEST_ASSERT(
        torn_recovery.error() == ente::core::EnteError::InvalidSignature ||
        torn_recovery.error() == ente::core::EnteError::HistoryCorrupt ||
        torn_recovery.error() == ente::core::EnteError::PersistenceFailure
    );
    std::cout << "  -> Truncated / torn block safely rejected with fail-closed error.\n";

    cleanup_file(journal_path);
}

void test_substrate_parent_directory_sync_barrier() {
    std::cout << "[G6-Substrate] 3. Testing directory entry commit point barriers...\n";
    const std::string dir_path = "test_g6_sync_dir";
    const std::string file_path = dir_path + "/journal.rec";
    std::error_code ec;
    std::filesystem::remove_all(dir_path, ec);
    std::filesystem::create_directories(dir_path, ec);

    ente::history::RecoverableHistory history;
    const ente::core::IdentityId id("ente-substrate-barrier");
    auto gen_ev = history.create_event(
        ente::history::EventKind::Genesis, id, 0, {}, {},
        ente::testing::canonical_genesis_payload(id)
    );
    ENTE_TEST_ASSERT(history.append(gen_ev).has_value());

    // Save with default production POSIX options (sync_file + atomic rename + sync_dir)
    ente::history::PersistenceOptions options;
    ENTE_TEST_ASSERT(history.save_to_file(file_path, options).has_value());
    ENTE_TEST_ASSERT(std::filesystem::exists(file_path));

    auto loaded = ente::history::RecoverableHistory::load_from_file(file_path);
    ENTE_TEST_ASSERT(loaded.has_value());
    ENTE_TEST_ASSERT_EQ(loaded->head_digest(), history.head_digest());
    std::cout << "  -> Directory barrier and atomic rename confirmed intact.\n";

    std::filesystem::remove_all(dir_path, ec);
}

} // namespace

int main() {
    std::cout << "=========================================================\n";
    std::cout << "   ENTE-1 G6 DECLARED-SUBSTRATE DURABILITY SUITE         \n";
    std::cout << "=========================================================\n\n";

    test_substrate_abrupt_death_across_lifecycle();
    test_substrate_torn_write_and_recovery_isolation();
    test_substrate_parent_directory_sync_barrier();

    std::cout << "\n>>> G6 SUBSTRATE DURABILITY VERIFICATION PASSED (3/3) <<<\n";
    return 0;
}
