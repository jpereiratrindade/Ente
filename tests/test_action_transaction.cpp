#include "ente/domain/generic_agent.hpp"
#include "ente/history/payloads.hpp"
#include "ente/testing/test_harness.hpp"

#include <filesystem>
#include <iostream>

namespace {

struct JournalFaultPlan {
    ente::history::PersistenceOperation operation;
    bool armed{false};
    size_t matching_calls{0};
    size_t fail_on_call{1};
};

bool fail_journal_operation(
    ente::history::PersistenceOperation operation,
    void* context
) noexcept {
    auto& plan = *static_cast<JournalFaultPlan*>(context);
    if (!plan.armed || operation != plan.operation) return true;
    ++plan.matching_calls;
    return plan.matching_calls != plan.fail_on_call;
}

enum class TestAction : uint8_t { Hold, Start };
enum class TestState : uint8_t { Idle, Running, SafeHold };

constexpr std::string_view to_string(TestAction action) noexcept {
    return action == TestAction::Start ? "START" : "HOLD";
}

constexpr std::string_view to_string(TestState state) noexcept {
    switch (state) {
        case TestState::Idle: return "IDLE";
        case TestState::Running: return "RUNNING";
        case TestState::SafeHold: return "SAFE_HOLD";
    }
    return "UNKNOWN";
}

class TestDomain {
public:
    using ActionType = TestAction;
    using StateType = TestState;

    TestAction active_action() const noexcept { return action_; }
    TestState current_state() const noexcept { return state_; }
    bool is_suspended() const noexcept { return suspended_; }
    static constexpr TestAction safe_hold_action() noexcept { return TestAction::Hold; }

    void apply_action(TestAction action) noexcept {
        action_ = action;
        state_ = action == TestAction::Start ? TestState::Running : TestState::Idle;
        suspended_ = false;
    }

    void apply_safety_directive(ente::assurance::SafetyDirective directive) noexcept {
        if (directive != ente::assurance::SafetyDirective::AllowAction) {
            action_ = TestAction::Hold;
            state_ = TestState::SafeHold;
            suspended_ = true;
        }
    }

private:
    TestAction action_{TestAction::Hold};
    TestState state_{TestState::Idle};
    bool suspended_{false};
};

static_assert(ente::domain::OperationalDomainConcept<TestDomain>);

void test_factual_lifecycle() {
    ente::domain::GenericAgentWithEnte<TestDomain> agent("action-lifecycle");
    ente::realization::StepContext context{
        .subject = "machine_ready",
        .proposition = "Machine is ready to start",
        .step_desc = "nominal start"
    };
    std::vector observations{
        ente::epistemic::Observation{
            .id = ente::core::EvidenceId("EV-ACTION-READY"),
            .source = "ready_sensor",
            .subject = "machine_ready",
            .value = "true",
            .observed_at = 1,
            .status = ente::epistemic::EpistemicStatus::Observed
        }
    };

    auto outcome = agent.decide_action_detailed(1, observations, TestAction::Start, context);
    ENTE_TEST_ASSERT(!outcome.is_safe_hold);
    ENTE_TEST_ASSERT(outcome.executed_action == TestAction::Start);
    ENTE_TEST_ASSERT(!outcome.audit_error.has_value());
    ENTE_TEST_ASSERT(outcome.action_transaction.has_value());
    ENTE_TEST_ASSERT(
        outcome.action_transaction->payload.phase == ente::history::ActionPhase::EffectUnconfirmed
    );
    ENTE_TEST_ASSERT_EQ(outcome.action_transaction->payload.proposed_action, "START");
    ENTE_TEST_ASSERT_EQ(outcome.action_transaction->payload.effective_action, "START");

    bool saw_authorized = false;
    bool saw_intent = false;
    bool saw_dispatch = false;
    bool saw_ack = false;
    bool saw_effect = false;
    bool saw_authorized_phase = false;
    bool saw_prepared_phase = false;
    bool saw_dispatched_phase = false;
    bool saw_acknowledged_phase = false;
    bool saw_unconfirmed_phase = false;
    for (const auto& event : agent.ente().history().events()) {
        saw_authorized |= event.kind == ente::history::EventKind::ActionAuthorized;
        saw_intent |= event.kind == ente::history::EventKind::ActionIntended;
        saw_dispatch |= event.kind == ente::history::EventKind::ActionExecution;
        saw_ack |= event.kind == ente::history::EventKind::ActionExecutionAck;
        saw_effect |= event.kind == ente::history::EventKind::EffectObservation;
        if (event.payload_content.starts_with("ENTE_ACTION_TX_V1")) {
            auto payload = ente::history::parse_action_transaction(event.payload_content);
            ENTE_TEST_ASSERT(payload.has_value());
            saw_authorized_phase |= payload->phase == ente::history::ActionPhase::Authorized;
            saw_prepared_phase |= payload->phase == ente::history::ActionPhase::Prepared;
            saw_dispatched_phase |= payload->phase == ente::history::ActionPhase::Dispatched;
            saw_acknowledged_phase |= payload->phase == ente::history::ActionPhase::Acknowledged;
            saw_unconfirmed_phase |= payload->phase == ente::history::ActionPhase::EffectUnconfirmed;
        }
    }
    ENTE_TEST_ASSERT(saw_authorized && saw_intent && saw_dispatch && saw_ack && saw_effect);
    ENTE_TEST_ASSERT(
        saw_authorized_phase && saw_prepared_phase && saw_dispatched_phase &&
        saw_acknowledged_phase && saw_unconfirmed_phase
    );

    const auto action_id = outcome.action_transaction->payload.action_id;
    auto confirmed = agent.ente_mut().observe_action_effect(
        action_id,
        2,
        true,
        "MOTOR_CURRENT_AND_SHAFT_MOTION_OBSERVED",
        "RUNNING",
        {{
            .id = ente::core::EvidenceId("EV-ACTION-EFFECT"),
            .source = "independent_motion_sensor",
            .subject = "shaft_motion",
            .value = "running",
            .observed_at = 2,
            .status = ente::epistemic::EpistemicStatus::Observed
        }}
    );
    ENTE_TEST_ASSERT(confirmed.has_value());
    ENTE_TEST_ASSERT(confirmed->payload.phase == ente::history::ActionPhase::Confirmed);

    auto duplicate_confirmation = agent.ente_mut().observe_action_effect(
        action_id,
        3,
        true,
        "DUPLICATE_CONFIRMATION",
        "RUNNING",
        {{
            .id = ente::core::EvidenceId("EV-ACTION-EFFECT-2"),
            .source = "independent_motion_sensor",
            .subject = "shaft_motion",
            .value = "running",
            .observed_at = 3,
            .status = ente::epistemic::EpistemicStatus::Observed
        }}
    );
    ENTE_TEST_ASSERT(!duplicate_confirmation.has_value());
    ENTE_TEST_ASSERT(duplicate_confirmation.error() == ente::core::EnteError::InvalidActionTransition);
}

void test_recovery_marks_dispatched_action_unknown() {
    const std::string path = "action_transaction_recovery.rec";
    std::filesystem::remove(path);

    ente::realization::EnteRealization process;
    ENTE_TEST_ASSERT(process.genesis(ente::core::IdentityId("ente-action-recovery")).has_value());
    ENTE_TEST_ASSERT(process.enable_durable_journal(path).has_value());
    ENTE_TEST_ASSERT(process.has_durable_journal());
    auto decision = process.step(1, {{
        .id = ente::core::EvidenceId("EV-RECOVERY-READY"),
        .source = "sensor",
        .subject = "path_clear",
        .value = "true",
        .observed_at = 1,
        .status = ente::epistemic::EpistemicStatus::Observed
    }});
    ENTE_TEST_ASSERT(decision.has_value());

    auto prepared = process.prepare_action(
        1,
        "START",
        "START",
        ente::assurance::SafetyDirective::AllowAction,
        "IDLE"
    );
    ENTE_TEST_ASSERT(prepared.has_value());
    auto dispatched = process.dispatch_action(prepared->payload.action_id, 1);
    ENTE_TEST_ASSERT(dispatched.has_value());

    // No explicit save: dispatch must already be durable before a physical
    // executor could be called.
    auto recovered = ente::realization::EnteRealization::recover_from_file(path);
    ENTE_TEST_ASSERT(recovered.has_value());
    ENTE_TEST_ASSERT(recovered->has_durable_journal());
    auto recovered_tx = recovered->action_transaction(prepared->payload.action_id);
    ENTE_TEST_ASSERT(recovered_tx.has_value());
    ENTE_TEST_ASSERT(recovered_tx->payload.phase == ente::history::ActionPhase::RecoveryRequired);
    ENTE_TEST_ASSERT(recovered->domain().is_action_suspended());
    ENTE_TEST_ASSERT(recovered->history().verify_integrity());

    std::filesystem::remove(path);
}

void test_canonical_payload_round_trip() {
    ente::history::ActionTransactionPayload payload{
        .action_id = ente::core::ActionTransactionId("action|with:delimiters"),
        .phase = ente::history::ActionPhase::Prepared,
        .proposed_action = "OPEN|VALVE:\nA",
        .effective_action = "HOLD:SAFE|MODE",
        .safety_directive = ente::assurance::SafetyDirective::SafeHold,
        .pre_state = "line1\nline2|state",
        .post_state = {},
        .detail = "arbitrary:value|kept"
    };
    const auto encoded = ente::history::serialize_action_transaction(payload);
    auto decoded = ente::history::parse_action_transaction(encoded);
    ENTE_TEST_ASSERT(decoded.has_value());
    ENTE_TEST_ASSERT_EQ(decoded->action_id, payload.action_id);
    ENTE_TEST_ASSERT_EQ(decoded->proposed_action, payload.proposed_action);
    ENTE_TEST_ASSERT_EQ(decoded->effective_action, payload.effective_action);
    ENTE_TEST_ASSERT_EQ(decoded->pre_state, payload.pre_state);
    ENTE_TEST_ASSERT_EQ(decoded->detail, payload.detail);
}

void test_mutable_history_forces_full_audit_before_action() {
    ente::realization::EnteRealization realization;
    ENTE_TEST_ASSERT(realization.genesis(ente::core::IdentityId("ente-dirty-history")).has_value());
    ENTE_TEST_ASSERT(realization.step(1, {{
        .id = ente::core::EvidenceId("EV-DIRTY-1"),
        .source = "sensor",
        .subject = "path_clear",
        .value = "true",
        .observed_at = 1,
        .status = ente::epistemic::EpistemicStatus::Observed
    }}).has_value());

    // Corrupt a non-HEAD event. Tail-only verification cannot discover this,
    // so privileged mutable access must force a complete audit.
    realization.history_mut().tamper_event_payload_for_testing(1, "CORRUPTED_NON_HEAD");
    auto next = realization.step(2, {{
        .id = ente::core::EvidenceId("EV-DIRTY-2"),
        .source = "sensor",
        .subject = "path_clear",
        .value = "true",
        .observed_at = 2,
        .status = ente::epistemic::EpistemicStatus::Observed
    }});
    ENTE_TEST_ASSERT(!next.has_value());
    ENTE_TEST_ASSERT(next.error() == ente::core::EnteError::ConstitutiveViolation);
    ENTE_TEST_ASSERT(realization.domain().is_action_suspended());
}

void test_precommit_failure_does_not_advance_live_state() {
    const std::string path = "action_transaction_precommit_failure.rec";
    std::filesystem::remove(path);
    std::filesystem::remove(path + ".tmp");

    JournalFaultPlan plan{
        .operation = ente::history::PersistenceOperation::ReplaceTarget
    };
    const ente::history::PersistenceOptions options{
        .before_operation = fail_journal_operation,
        .context = &plan
    };

    ente::realization::EnteRealization process;
    ENTE_TEST_ASSERT(process.genesis(ente::core::IdentityId("ente-precommit-failure")).has_value());
    ENTE_TEST_ASSERT(process.enable_durable_journal(path, options).has_value());
    const auto history_size_before = process.history().size();
    const auto head_before = process.history().head_digest();

    plan.armed = true;
    const auto failed = process.prepare_action(
        1,
        "START",
        "START",
        ente::assurance::SafetyDirective::AllowAction,
        "IDLE"
    );
    ENTE_TEST_ASSERT(!failed.has_value());
    ENTE_TEST_ASSERT(failed.error() == ente::core::EnteError::PersistenceFailure);
    ENTE_TEST_ASSERT_EQ(process.history().size(), history_size_before);
    ENTE_TEST_ASSERT_EQ(process.history().head_digest(), head_before);

    const auto persisted = ente::history::RecoverableHistory::load_from_file(path);
    ENTE_TEST_ASSERT(persisted.has_value());
    ENTE_TEST_ASSERT_EQ(persisted->size(), history_size_before);
    ENTE_TEST_ASSERT_EQ(persisted->head_digest(), head_before);

    plan.armed = false;
    ENTE_TEST_ASSERT(process.prepare_action(
        1,
        "START",
        "START",
        ente::assurance::SafetyDirective::AllowAction,
        "IDLE"
    ).has_value());

    std::filesystem::remove(path);
    std::filesystem::remove(path + ".tmp");
}

void test_effect_observation_rolls_back_as_one_unit() {
    const std::string path = "action_effect_precommit_failure.rec";
    std::filesystem::remove(path);
    std::filesystem::remove(path + ".tmp");

    JournalFaultPlan plan{
        .operation = ente::history::PersistenceOperation::ReplaceTarget
    };
    const ente::history::PersistenceOptions options{
        .before_operation = fail_journal_operation,
        .context = &plan
    };

    ente::realization::EnteRealization process;
    ENTE_TEST_ASSERT(process.genesis(ente::core::IdentityId("ente-effect-rollback")).has_value());
    ENTE_TEST_ASSERT(process.enable_durable_journal(path, options).has_value());
    const auto prepared = process.prepare_action(
        1,
        "START",
        "START",
        ente::assurance::SafetyDirective::AllowAction,
        "IDLE"
    );
    ENTE_TEST_ASSERT(prepared.has_value());
    ENTE_TEST_ASSERT(process.dispatch_action(prepared->payload.action_id, 1).has_value());
    ENTE_TEST_ASSERT(process.acknowledge_action(
        prepared->payload.action_id,
        1,
        "START",
        true,
        "RUNNING"
    ).has_value());

    const auto history_size_before = process.history().size();
    const auto head_before = process.history().head_digest();
    plan.armed = true;
    const auto failed = process.observe_action_effect(
        prepared->payload.action_id,
        2,
        true,
        "SHAFT_MOTION_OBSERVED",
        "RUNNING",
        {{
            .id = ente::core::EvidenceId("EV-ROLLBACK-EFFECT"),
            .source = "independent_motion_sensor",
            .subject = "shaft_motion",
            .value = "running",
            .observed_at = 2,
            .status = ente::epistemic::EpistemicStatus::Observed
        }}
    );
    ENTE_TEST_ASSERT(!failed.has_value());
    ENTE_TEST_ASSERT(failed.error() == ente::core::EnteError::PersistenceFailure);
    ENTE_TEST_ASSERT_EQ(process.history().size(), history_size_before);
    ENTE_TEST_ASSERT_EQ(process.history().head_digest(), head_before);
    const auto transaction = process.action_transaction(prepared->payload.action_id);
    ENTE_TEST_ASSERT(transaction.has_value());
    ENTE_TEST_ASSERT(transaction->payload.phase == ente::history::ActionPhase::Acknowledged);

    const auto persisted = ente::history::RecoverableHistory::load_from_file(path);
    ENTE_TEST_ASSERT(persisted.has_value());
    ENTE_TEST_ASSERT_EQ(persisted->size(), history_size_before);
    ENTE_TEST_ASSERT_EQ(persisted->head_digest(), head_before);

    plan.armed = false;
    ENTE_TEST_ASSERT(process.observe_action_effect(
        prepared->payload.action_id,
        2,
        true,
        "SHAFT_MOTION_OBSERVED",
        "RUNNING",
        {{
            .id = ente::core::EvidenceId("EV-ROLLBACK-EFFECT"),
            .source = "independent_motion_sensor",
            .subject = "shaft_motion",
            .value = "running",
            .observed_at = 2,
            .status = ente::epistemic::EpistemicStatus::Observed
        }}
    ).has_value());

    std::filesystem::remove(path);
    std::filesystem::remove(path + ".tmp");
}

void test_postrename_failure_is_explicitly_uncertain() {
    const std::string path = "action_transaction_commit_uncertain.rec";
    std::filesystem::remove(path);
    std::filesystem::remove(path + ".tmp");

    JournalFaultPlan plan{
        .operation = ente::history::PersistenceOperation::SyncParentDirectory
    };
    const ente::history::PersistenceOptions options{
        .before_operation = fail_journal_operation,
        .context = &plan
    };

    ente::realization::EnteRealization process;
    ENTE_TEST_ASSERT(process.genesis(ente::core::IdentityId("ente-commit-uncertain")).has_value());
    ENTE_TEST_ASSERT(process.enable_durable_journal(path, options).has_value());
    const auto history_size_before = process.history().size();

    plan.armed = true;
    const auto uncertain = process.prepare_action(
        1,
        "START",
        "START",
        ente::assurance::SafetyDirective::AllowAction,
        "IDLE"
    );
    ENTE_TEST_ASSERT(!uncertain.has_value());
    ENTE_TEST_ASSERT(uncertain.error() == ente::core::EnteError::PersistenceCommitUncertain);
    ENTE_TEST_ASSERT_EQ(process.history().size(), history_size_before + 1);

    const auto payload = ente::history::parse_action_transaction(
        process.history().head().payload_content
    );
    ENTE_TEST_ASSERT(payload.has_value());
    ENTE_TEST_ASSERT(payload->phase == ente::history::ActionPhase::Authorized);
    const auto transaction = process.action_transaction(payload->action_id);
    ENTE_TEST_ASSERT(transaction.has_value());
    ENTE_TEST_ASSERT(transaction->payload.phase == ente::history::ActionPhase::Authorized);

    const auto persisted = ente::history::RecoverableHistory::load_from_file(path);
    ENTE_TEST_ASSERT(persisted.has_value());
    ENTE_TEST_ASSERT_EQ(persisted->size(), process.history().size());
    ENTE_TEST_ASSERT_EQ(persisted->head_digest(), process.history().head_digest());

    const auto recovered = ente::realization::EnteRealization::recover_from_file(path);
    ENTE_TEST_ASSERT(recovered.has_value());
    const auto recovered_transaction = recovered->action_transaction(payload->action_id);
    ENTE_TEST_ASSERT(recovered_transaction.has_value());
    ENTE_TEST_ASSERT(
        recovered_transaction->payload.phase == ente::history::ActionPhase::RecoveryRequired
    );
    ENTE_TEST_ASSERT(recovered->domain().is_action_suspended());

    std::filesystem::remove(path);
    std::filesystem::remove(path + ".tmp");
}

void test_dispatch_must_be_durable_before_domain_side_effect() {
    const std::string path = "action_dispatch_precommit_failure.rec";
    std::filesystem::remove(path);
    std::filesystem::remove(path + ".tmp");

    JournalFaultPlan plan{
        .operation = ente::history::PersistenceOperation::ReplaceTarget,
        .fail_on_call = 3
    };
    const ente::history::PersistenceOptions options{
        .before_operation = fail_journal_operation,
        .context = &plan
    };
    ente::domain::GenericAgentWithEnte<TestDomain> agent(
        "dispatch-durability-boundary",
        TestDomain{},
        std::nullopt,
        path,
        options
    );

    plan.armed = true;
    const auto outcome = agent.decide_action_detailed(
        1,
        {{
            .id = ente::core::EvidenceId("EV-DISPATCH-READY"),
            .source = "ready_sensor",
            .subject = "machine_ready",
            .value = "true",
            .observed_at = 1,
            .status = ente::epistemic::EpistemicStatus::Observed
        }},
        TestAction::Start,
        {
            .subject = "machine_ready",
            .proposition = "Machine is ready to start",
            .step_desc = "dispatch durability boundary"
        }
    );

    ENTE_TEST_ASSERT(outcome.is_safe_hold);
    ENTE_TEST_ASSERT(outcome.executed_action == TestAction::Hold);
    ENTE_TEST_ASSERT(outcome.audit_error.has_value());
    ENTE_TEST_ASSERT(outcome.audit_error == ente::core::EnteError::PersistenceFailure);
    ENTE_TEST_ASSERT(outcome.action_transaction.has_value());
    ENTE_TEST_ASSERT(
        outcome.action_transaction->payload.phase == ente::history::ActionPhase::Prepared
    );
    ENTE_TEST_ASSERT(agent.domain().current_state() == TestState::SafeHold);
    ENTE_TEST_ASSERT(agent.domain().active_action() == TestAction::Hold);

    const auto persisted = ente::history::RecoverableHistory::load_from_file(path);
    ENTE_TEST_ASSERT(persisted.has_value());
    bool saw_prepared = false;
    bool saw_dispatched = false;
    ente::core::ActionTransactionId action_id;
    for (const auto& event : persisted->events()) {
        if (!event.payload_content.starts_with("ENTE_ACTION_TX_V1")) continue;
        const auto payload = ente::history::parse_action_transaction(event.payload_content);
        ENTE_TEST_ASSERT(payload.has_value());
        action_id = payload->action_id;
        saw_prepared |= payload->phase == ente::history::ActionPhase::Prepared;
        saw_dispatched |= payload->phase == ente::history::ActionPhase::Dispatched;
    }
    ENTE_TEST_ASSERT(saw_prepared);
    ENTE_TEST_ASSERT(!saw_dispatched);
    const auto transaction = agent.ente().action_transaction(action_id);
    ENTE_TEST_ASSERT(transaction.has_value());
    ENTE_TEST_ASSERT(transaction->payload.phase == ente::history::ActionPhase::Prepared);

    std::filesystem::remove(path);
    std::filesystem::remove(path + ".tmp");
}

} // namespace

int main() {
    test_factual_lifecycle();
    test_recovery_marks_dispatched_action_unknown();
    test_canonical_payload_round_trip();
    test_mutable_history_forces_full_audit_before_action();
    test_precommit_failure_does_not_advance_live_state();
    test_effect_observation_rolls_back_as_one_unit();
    test_postrename_failure_is_explicitly_uncertain();
    test_dispatch_must_be_durable_before_domain_side_effect();
    std::cout << "[PASS] test_action_transaction: factual phases, durable commit point, side-effect boundary and recovery verified.\n";
    return 0;
}
