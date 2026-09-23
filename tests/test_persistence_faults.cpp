#include "ente/history/rec.hpp"
#include "ente/testing/test_harness.hpp"

#include <filesystem>
#include <iostream>
#include <string>

namespace {

using ente::core::EnteError;
using ente::history::PersistenceOperation;
using ente::history::PersistenceOptions;
using ente::history::RecoverableHistory;

struct FaultPlan {
    PersistenceOperation operation;
    size_t calls{0};
};

bool fail_selected_operation(PersistenceOperation operation, void* context) noexcept {
    auto& plan = *static_cast<FaultPlan*>(context);
    ++plan.calls;
    return operation != plan.operation;
}

RecoverableHistory make_history(bool include_observation) {
    RecoverableHistory history;
    const ente::core::IdentityId identity("ente-persistence-faults");
    auto genesis = history.create_event(
        ente::history::EventKind::Genesis,
        identity,
        0,
        {},
        {},
        "GENESIS"
    );
    ENTE_TEST_ASSERT(history.append(genesis).has_value());

    if (include_observation) {
        auto observation = history.create_event(
            ente::history::EventKind::Observation,
            identity,
            1,
            {genesis.id},
            {ente::core::EvidenceId("EV-PERSISTENCE-1")},
            "OBSERVED"
        );
        ENTE_TEST_ASSERT(history.append(std::move(observation)).has_value());
    }

    return history;
}

void remove_artifacts(const std::string& path) {
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::filesystem::remove(path + ".tmp", ignored);
}

void test_operation_failure(
    PersistenceOperation operation,
    bool replacement_is_visible
) {
    const std::string path = "test_persistence_faults.rec";
    remove_artifacts(path);

    const auto baseline = make_history(false);
    const auto updated = make_history(true);
    ENTE_TEST_ASSERT(baseline.save_to_file(path).has_value());

    FaultPlan plan{.operation = operation};
    const PersistenceOptions options{
        .before_operation = fail_selected_operation,
        .context = &plan
    };
    const auto failed_save = updated.save_to_file(path, options);
    ENTE_TEST_ASSERT(!failed_save.has_value());
    ENTE_TEST_ASSERT(failed_save.error() == EnteError::PersistenceFailure);
    ENTE_TEST_ASSERT(plan.calls > 0);
    ENTE_TEST_ASSERT(!std::filesystem::exists(path + ".tmp"));

    const auto recovered = RecoverableHistory::load_from_file(path);
    ENTE_TEST_ASSERT(recovered.has_value());
    ENTE_TEST_ASSERT(recovered->verify_integrity());
    ENTE_TEST_ASSERT_EQ(recovered->size(), replacement_is_visible ? updated.size() : baseline.size());

    // A later healthy save must converge on the complete new generation.
    ENTE_TEST_ASSERT(updated.save_to_file(path).has_value());
    const auto retried = RecoverableHistory::load_from_file(path);
    ENTE_TEST_ASSERT(retried.has_value());
    ENTE_TEST_ASSERT_EQ(retried->size(), updated.size());
    remove_artifacts(path);
}

void test_stale_temporary_generation_is_not_promoted() {
    const std::string path = "test_persistence_stale_tmp.rec";
    const std::string staged_path = "test_persistence_staged_generation.rec";
    remove_artifacts(path);
    remove_artifacts(staged_path);

    const auto baseline = make_history(false);
    const auto updated = make_history(true);
    ENTE_TEST_ASSERT(baseline.save_to_file(path).has_value());
    ENTE_TEST_ASSERT(updated.save_to_file(staged_path).has_value());

    std::filesystem::copy_file(
        staged_path,
        path + ".tmp",
        std::filesystem::copy_options::overwrite_existing
    );

    // A complete but unrenamed generation is not committed. Recovery selects
    // only the last target that crossed the atomic replacement boundary.
    const auto recovered = RecoverableHistory::load_from_file(path);
    ENTE_TEST_ASSERT(recovered.has_value());
    ENTE_TEST_ASSERT_EQ(recovered->size(), baseline.size());
    ENTE_TEST_ASSERT(std::filesystem::exists(path + ".tmp"));

    ENTE_TEST_ASSERT(updated.save_to_file(path).has_value());
    const auto committed = RecoverableHistory::load_from_file(path);
    ENTE_TEST_ASSERT(committed.has_value());
    ENTE_TEST_ASSERT_EQ(committed->size(), updated.size());

    remove_artifacts(path);
    remove_artifacts(staged_path);
}

} // namespace

int main() {
    test_operation_failure(PersistenceOperation::SyncTemporaryFile, false);
    test_operation_failure(PersistenceOperation::ReplaceTarget, false);
    test_operation_failure(PersistenceOperation::SyncParentDirectory, true);
    test_stale_temporary_generation_is_not_promoted();

    std::cout << "[PASS] test_persistence_faults: fsync, replace, directory-sync and stale-temp recovery boundaries verified.\n";
    return 0;
}
