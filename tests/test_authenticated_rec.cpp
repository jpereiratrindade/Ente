#include "ente/core/ed25519.hpp"
#include "ente/history/rec.hpp"
#include "ente/realization/runner.hpp"
#include "ente/testing/test_harness.hpp"
#include "ente/testing/history_fixtures.hpp"

#include <array>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

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

std::string read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    ENTE_TEST_ASSERT(input.is_open());
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()
    };
}

void write_file(const std::string& path, std::string_view contents) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    ENTE_TEST_ASSERT(output.is_open());
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    output.close();
    ENTE_TEST_ASSERT(output.good());
}

ente::history::RecoverableHistory make_history(std::string_view payload) {
    ente::history::RecoverableHistory history;
    const ente::core::IdentityId identity("ente-authenticated-rec");
    auto genesis = history.create_event(
        ente::history::EventKind::Genesis,
        identity,
        0,
        {},
        {},
        ente::testing::canonical_genesis_payload(identity)
    );
    ENTE_TEST_ASSERT(history.append(genesis).has_value());
    auto observation = history.create_event(
        ente::history::EventKind::Observation,
        identity,
        1,
        {genesis.id},
        {ente::core::EvidenceId("EV-AUTH-1")},
        ente::testing::canonical_observation_payload(
            ente::core::EvidenceId("EV-AUTH-1"), 1, payload)
    );
    ENTE_TEST_ASSERT(history.append(std::move(observation)).has_value());
    return history;
}

void test_rfc8032_vector() {
    const auto seed = decode_seed(
        "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60"
    );
    auto key = ente::core::Ed25519KeyPair::from_private_seed(seed);
    ENTE_TEST_ASSERT(key.has_value());
    ENTE_TEST_ASSERT_EQ(
        key->public_key_hex(),
        "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a"
    );

    const auto signature = key->sign_hex("");
    ENTE_TEST_ASSERT(signature.has_value());
    ENTE_TEST_ASSERT_EQ(
        *signature,
        "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e06522490155"
        "5fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b"
    );
    ENTE_TEST_ASSERT(ente::core::Ed25519KeyPair::verify_hex(
        key->public_key_hex(),
        "",
        *signature
    ));
}

void test_authenticated_snapshot_rejects_valid_recomputed_chain() {
    const std::string authenticated_path = "test_authenticated_rec.rec";
    const std::string forged_v1_path = "test_authenticated_rec_forged_v1.rec";
    std::filesystem::remove(authenticated_path);
    std::filesystem::remove(authenticated_path + ".tmp");
    std::filesystem::remove(forged_v1_path);

    const auto seed = decode_seed(
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
    );
    auto signer = ente::core::Ed25519KeyPair::from_private_seed(seed);
    ENTE_TEST_ASSERT(signer.has_value());
    const auto public_key = signer->public_key_hex();

    const auto authentic = make_history("OBSERVED_TRUE");
    ENTE_TEST_ASSERT(authentic.save_authenticated_to_file(
        authenticated_path,
        *signer
    ).has_value());
    const auto loaded = ente::history::RecoverableHistory::load_authenticated_from_file(
        authenticated_path,
        public_key
    );
    ENTE_TEST_ASSERT(loaded.has_value());
    ENTE_TEST_ASSERT_EQ(loaded->head_digest(), authentic.head_digest());

    // Authentication cannot be silently downgraded to hash-only loading.
    const auto downgrade = ente::history::RecoverableHistory::load_from_file(authenticated_path);
    ENTE_TEST_ASSERT(!downgrade.has_value());

    auto wrong_seed = seed;
    wrong_seed[0] ^= 0xff;
    auto wrong_signer = ente::core::Ed25519KeyPair::from_private_seed(wrong_seed);
    ENTE_TEST_ASSERT(wrong_signer.has_value());
    const auto wrong_key = ente::history::RecoverableHistory::load_authenticated_from_file(
        authenticated_path,
        wrong_signer->public_key_hex()
    );
    ENTE_TEST_ASSERT(!wrong_key.has_value());
    ENTE_TEST_ASSERT(wrong_key.error() == ente::core::EnteError::InvalidSignature);

    // The attacker recomputes a completely valid SHA-256 chain with different
    // facts, then reuses the authentic signature header. Hash integrity alone
    // accepts the forged body; Ed25519 authentication must reject it.
    const auto forged = make_history("OBSERVED_FALSE_RECOMPUTED_CHAIN");
    ENTE_TEST_ASSERT(forged.save_to_file(forged_v1_path).has_value());
    const std::string authentic_file = read_file(authenticated_path);
    const std::string forged_v1 = read_file(forged_v1_path);
    const size_t authentic_body = authentic_file.find('\n', authentic_file.find('\n') + 1) + 1;
    const size_t forged_body = forged_v1.find('\n') + 1;
    ENTE_TEST_ASSERT(authentic_body > 1);
    ENTE_TEST_ASSERT(forged_body > 0);
    write_file(
        authenticated_path,
        authentic_file.substr(0, authentic_body) + forged_v1.substr(forged_body)
    );

    const auto forged_hash_only = ente::history::RecoverableHistory::load_from_file(
        forged_v1_path
    );
    ENTE_TEST_ASSERT(forged_hash_only.has_value());
    ENTE_TEST_ASSERT(forged_hash_only->verify_integrity());
    const auto rejected = ente::history::RecoverableHistory::load_authenticated_from_file(
        authenticated_path,
        public_key
    );
    ENTE_TEST_ASSERT(!rejected.has_value());
    ENTE_TEST_ASSERT(rejected.error() == ente::core::EnteError::InvalidSignature);

    std::filesystem::remove(authenticated_path);
    std::filesystem::remove(authenticated_path + ".tmp");
    std::filesystem::remove(forged_v1_path);
}

void test_authenticated_journal_survives_restart() {
    const std::string path = "test_authenticated_journal.rec";
    std::filesystem::remove(path);
    std::filesystem::remove(path + ".tmp");

    const auto seed = decode_seed(
        "101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f"
    );
    auto signer = ente::core::Ed25519KeyPair::from_private_seed(seed);
    ENTE_TEST_ASSERT(signer.has_value());
    const std::string trusted_public_key = signer->public_key_hex();

    ente::realization::EnteRealization process;
    ENTE_TEST_ASSERT(process.genesis(
        ente::core::IdentityId("ente-authenticated-journal")
    ).has_value());
    ENTE_TEST_ASSERT(process.enable_authenticated_durable_journal(
        path,
        std::move(*signer)
    ).has_value());
    ENTE_TEST_ASSERT(process.has_authenticated_journal());
    ENTE_TEST_ASSERT(process.step(1, {{
        .id = ente::core::EvidenceId("EV-AUTH-JOURNAL"),
        .source = "trusted_sensor",
        .subject = "path_clear",
        .value = "true",
        .observed_at = 1,
        .status = ente::epistemic::EpistemicStatus::Observed
    }}).has_value());

    const auto prepared = process.prepare_action(
        1,
        "START",
        "START",
        ente::assurance::SafetyDirective::AllowAction,
        "IDLE"
    );
    ENTE_TEST_ASSERT(prepared.has_value());
    ENTE_TEST_ASSERT(ente::history::RecoverableHistory::load_authenticated_from_file(
        path,
        trusted_public_key
    ).has_value());
    ENTE_TEST_ASSERT(process.dispatch_action(prepared->payload.action_id, 1).has_value());
    ENTE_TEST_ASSERT(ente::history::RecoverableHistory::load_authenticated_from_file(
        path,
        trusted_public_key
    ).has_value());

    // The non-authenticated recovery path cannot downgrade an authenticated journal.
    const auto downgrade = ente::realization::EnteRealization::recover_from_file(path);
    ENTE_TEST_ASSERT(!downgrade.has_value());

    auto wrong_seed = seed;
    wrong_seed[0] ^= 0x80;
    auto wrong_signer = ente::core::Ed25519KeyPair::from_private_seed(wrong_seed);
    ENTE_TEST_ASSERT(wrong_signer.has_value());
    const auto wrong_recovery = ente::realization::EnteRealization::recover_from_authenticated_file(
        path,
        std::move(*wrong_signer)
    );
    ENTE_TEST_ASSERT(!wrong_recovery.has_value());
    ENTE_TEST_ASSERT(wrong_recovery.error() == ente::core::EnteError::InvalidSignature);

    auto recovery_signer = ente::core::Ed25519KeyPair::from_private_seed(seed);
    ENTE_TEST_ASSERT(recovery_signer.has_value());
    auto recovered = ente::realization::EnteRealization::recover_from_authenticated_file(
        path,
        std::move(*recovery_signer)
    );
    ENTE_TEST_ASSERT(recovered.has_value());
    ENTE_TEST_ASSERT(recovered->has_authenticated_journal());
    const auto transaction = recovered->action_transaction(prepared->payload.action_id);
    ENTE_TEST_ASSERT(transaction.has_value());
    ENTE_TEST_ASSERT(
        transaction->payload.phase == ente::history::ActionPhase::RecoveryRequired
    );
    ENTE_TEST_ASSERT(recovered->domain().is_action_suspended());

    // The recovery marker itself must be covered by a fresh valid signature.
    const auto authenticated_recovery =
        ente::history::RecoverableHistory::load_authenticated_from_file(
            path,
            trusted_public_key
        );
    ENTE_TEST_ASSERT(authenticated_recovery.has_value());
    ENTE_TEST_ASSERT_EQ(
        authenticated_recovery->head_digest(),
        recovered->history().head_digest()
    );

    std::filesystem::remove(path);
    std::filesystem::remove(path + ".tmp");
}

} // namespace

int main() {
    test_rfc8032_vector();
    test_authenticated_snapshot_rejects_valid_recomputed_chain();
    test_authenticated_journal_survives_restart();
    std::cout << "[PASS] test_authenticated_rec: RFC 8032 Ed25519 and offline recomputed-chain rejection verified.\n";
    return 0;
}
