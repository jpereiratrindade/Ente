#include "ente/history/payloads.hpp"
#include "ente/history/rec.hpp"
#include "ente/realization/runner.hpp"
#include "ente/testing/test_harness.hpp"
#include "ente/testing/history_fixtures.hpp"

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

using ente::history::ActionPhase;
using ente::history::PersistenceOperation;
using ente::history::PersistenceOptions;
using ente::history::RecoverableHistory;

constexpr int crash_exit_code = 73;
constexpr int child_setup_failure_exit_code = 74;

void remove_artifacts(const std::string& path) {
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::filesystem::remove(path + ".tmp", ignored);
}

RecoverableHistory make_history(bool include_observation) {
    RecoverableHistory history;
    const ente::core::IdentityId identity("ente-process-crash");
    auto genesis = history.create_event(
        ente::history::EventKind::Genesis,
        identity,
        0,
        {},
        {},
        ente::testing::canonical_genesis_payload(identity)
    );
    ENTE_TEST_ASSERT(history.append(genesis).has_value());

    if (include_observation) {
        auto observation = history.create_event(
            ente::history::EventKind::Observation,
            identity,
            1,
            {genesis.id},
            {ente::core::EvidenceId("EV-CRASH-1")},
            ente::testing::canonical_observation_payload(
                ente::core::EvidenceId("EV-CRASH-1"), 1)
        );
        ENTE_TEST_ASSERT(history.append(std::move(observation)).has_value());
    }
    return history;
}

std::optional<ente::history::ActionTransactionPayload> last_action_payload(
    const RecoverableHistory& history
) {
    std::optional<ente::history::ActionTransactionPayload> result;
    for (const auto& event : history.events()) {
        if (!event.payload_content.starts_with("ENTE_ACTION_TX_V1")) continue;
        auto parsed = ente::history::parse_action_transaction(event.payload_content);
        ENTE_TEST_ASSERT(parsed.has_value());
        result = std::move(*parsed);
    }
    return result;
}

#if defined(__unix__) || defined(__APPLE__)

struct CrashPlan {
    PersistenceOperation operation;
};

bool crash_before_selected_operation(PersistenceOperation operation, void* context) noexcept {
    const auto& plan = *static_cast<const CrashPlan*>(context);
    if (operation == plan.operation) {
        ::_exit(crash_exit_code);
    }
    return true;
}

void require_child_success(bool condition) noexcept {
    if (!condition) ::_exit(child_setup_failure_exit_code);
}

void wait_for_expected_crash(pid_t child) {
    int status = 0;
    ENTE_TEST_ASSERT(::waitpid(child, &status, 0) == child);
    ENTE_TEST_ASSERT(WIFEXITED(status));
    ENTE_TEST_ASSERT_EQ(WEXITSTATUS(status), crash_exit_code);
}

void test_crash_at_persistence_boundary(
    PersistenceOperation operation,
    bool replacement_is_visible,
    bool temporary_file_remains
) {
    const std::string path = "test_process_crash_persistence.rec";
    remove_artifacts(path);

    const auto baseline = make_history(false);
    const auto updated = make_history(true);
    ENTE_TEST_ASSERT(baseline.save_to_file(path).has_value());

    const pid_t child = ::fork();
    ENTE_TEST_ASSERT(child >= 0);
    if (child == 0) {
        CrashPlan plan{.operation = operation};
        const PersistenceOptions options{
            .before_operation = crash_before_selected_operation,
            .context = &plan
        };
        const auto result = updated.save_to_file(path, options);
        (void)result;
        ::_exit(child_setup_failure_exit_code);
    }
    wait_for_expected_crash(child);

    ENTE_TEST_ASSERT_EQ(std::filesystem::exists(path + ".tmp"), temporary_file_remains);
    const auto recovered = RecoverableHistory::load_from_file(path);
    ENTE_TEST_ASSERT(recovered.has_value());
    ENTE_TEST_ASSERT(recovered->verify_integrity());
    ENTE_TEST_ASSERT_EQ(recovered->size(), replacement_is_visible ? updated.size() : baseline.size());

    ENTE_TEST_ASSERT(updated.save_to_file(path).has_value());
    const auto retried = RecoverableHistory::load_from_file(path);
    ENTE_TEST_ASSERT(retried.has_value());
    ENTE_TEST_ASSERT_EQ(retried->size(), updated.size());
    remove_artifacts(path);
}

void build_action_phase_and_crash(const std::string& path, ActionPhase target_phase) noexcept {
    ente::realization::EnteRealization process;
    require_child_success(process.genesis(ente::core::IdentityId("ente-action-process-crash")).has_value());
    require_child_success(process.enable_durable_journal(path).has_value());
    require_child_success(process.step(1, {{
        .id = ente::core::EvidenceId("EV-ACTION-READY"),
        .source = "ready_sensor",
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
    require_child_success(prepared.has_value());
    if (target_phase == ActionPhase::Prepared) ::_exit(crash_exit_code);

    const auto dispatched = process.dispatch_action(prepared->payload.action_id, 1);
    require_child_success(dispatched.has_value());
    if (target_phase == ActionPhase::Dispatched) ::_exit(crash_exit_code);

    if (target_phase == ActionPhase::Failed) {
        require_child_success(process.acknowledge_action(
            prepared->payload.action_id,
            1,
            "START",
            false,
            "IDLE",
            "test-executor"
        ).has_value());
        ::_exit(crash_exit_code);
    }

    const auto acknowledged = process.acknowledge_action(
        prepared->payload.action_id,
        1,
        "START",
        true,
        "RUNNING",
        "test-executor"
    );
    require_child_success(acknowledged.has_value());
    if (target_phase == ActionPhase::Acknowledged) ::_exit(crash_exit_code);

    if (target_phase == ActionPhase::EffectUnconfirmed) {
        require_child_success(process.observe_action_effect(
            prepared->payload.action_id,
            2,
            false,
            "NO_INDEPENDENT_EFFECT_OBSERVATION",
            "RUNNING"
        ).has_value());
        ::_exit(crash_exit_code);
    }

    require_child_success(target_phase == ActionPhase::Confirmed);
    require_child_success(process.observe_action_effect(
        prepared->payload.action_id,
        2,
        true,
        "SHAFT_MOTION_OBSERVED",
        "RUNNING",
        {{
            .id = ente::core::EvidenceId("EV-ACTION-EFFECT"),
            .source = "independent_motion_sensor",
            .subject = "shaft_motion",
            .value = "running",
            .observed_at = 2,
            .status = ente::epistemic::EpistemicStatus::Observed
        }}
    ).has_value());
    ::_exit(crash_exit_code);
}

void test_crash_after_action_phase(ActionPhase phase, bool recovery_required) {
    const std::string path = "test_action_phase_process_crash.rec";
    remove_artifacts(path);

    const pid_t child = ::fork();
    ENTE_TEST_ASSERT(child >= 0);
    if (child == 0) build_action_phase_and_crash(path, phase);
    wait_for_expected_crash(child);

    const auto on_disk = RecoverableHistory::load_from_file(path);
    ENTE_TEST_ASSERT(on_disk.has_value());
    const auto before_recovery = last_action_payload(*on_disk);
    ENTE_TEST_ASSERT(before_recovery.has_value());
    ENTE_TEST_ASSERT(before_recovery->phase == phase);

    const auto recovered = ente::realization::EnteRealization::recover_from_file(path);
    ENTE_TEST_ASSERT(recovered.has_value());
    const auto transaction = recovered->action_transaction(before_recovery->action_id);
    ENTE_TEST_ASSERT(transaction.has_value());
    ENTE_TEST_ASSERT(transaction->payload.phase == (
        recovery_required ? ActionPhase::RecoveryRequired : phase
    ));
    ENTE_TEST_ASSERT_EQ(recovered->domain().is_action_suspended(), recovery_required);

    const auto persisted_recovery = RecoverableHistory::load_from_file(path);
    ENTE_TEST_ASSERT(persisted_recovery.has_value());
    const auto final_payload = last_action_payload(*persisted_recovery);
    ENTE_TEST_ASSERT(final_payload.has_value());
    ENTE_TEST_ASSERT(final_payload->phase == (
        recovery_required ? ActionPhase::RecoveryRequired : phase
    ));
    remove_artifacts(path);
}

#endif

} // namespace

int main() {
#if defined(__unix__) || defined(__APPLE__)
    test_crash_at_persistence_boundary(PersistenceOperation::SyncTemporaryFile, false, true);
    test_crash_at_persistence_boundary(PersistenceOperation::ReplaceTarget, false, true);
    test_crash_at_persistence_boundary(PersistenceOperation::SyncParentDirectory, true, false);

    test_crash_after_action_phase(ActionPhase::Prepared, true);
    test_crash_after_action_phase(ActionPhase::Dispatched, true);
    test_crash_after_action_phase(ActionPhase::Acknowledged, true);
    test_crash_after_action_phase(ActionPhase::EffectUnconfirmed, true);
    test_crash_after_action_phase(ActionPhase::Confirmed, false);
    test_crash_after_action_phase(ActionPhase::Failed, false);

    std::cout << "[PASS] test_process_crash_recovery: abrupt POSIX process exit preserves atomic REC and factual action recovery semantics.\n";
#else
    std::cout << "[SKIP] test_process_crash_recovery requires POSIX fork/waitpid semantics.\n";
#endif
    return 0;
}
