#include "case_domain.hpp"
#include "agent_without_ente.hpp"
#include "agent_with_ente.hpp"
#include "scenarios.hpp"
#include "ente/attestation/rats.hpp"
#include "ente/constitution/verifier.hpp"

#include <iostream>
#include <iomanip>
#include <cassert>
#include <filesystem>

namespace {

void run_sequential_benchmark() {
    std::cout << "--- 1. Canonical Sequential Benchmark (S0 -> S6) ---\n";

    auto scenarios = caselab::get_kitkat_lab_scenarios();

    caselab::AgentWithoutEnte baseline_agent;
    caselab::AgentWithEnte ente_agent(ente::core::IdentityId{"ente-av-001"});

    size_t total_scenarios = scenarios.size();
    size_t baseline_unjustified_cont = 0;
    size_t baseline_unnecessary_susp = 0;
    size_t ente_unjustified_cont = 0;
    size_t ente_unnecessary_susp = 0;

    std::cout << std::left 
              << std::setw(6) << "ID"
              << std::setw(32) << "Scenario Title"
              << std::setw(14) << "GroundTruth"
              << std::setw(14) << "No-ENTE"
              << std::setw(14) << "With-ENTE"
              << std::setw(12) << "Status"
              << "\n";
    std::cout << std::string(92, '-') << "\n";

    uint64_t logical_time = 1000;

    for (const auto& sc : scenarios) {
        logical_time += 10;

        auto action_no_ente = baseline_agent.process(sc.observations);
        auto action_with_ente = ente_agent.process(sc.observations, logical_time);

        bool expected_hold = sc.should_hold_for_safety;

        // Baseline evaluation
        if (expected_hold && action_no_ente == caselab::VehicleAction::Depart) {
            baseline_unjustified_cont++;
        }
        if (!expected_hold && action_no_ente == caselab::VehicleAction::Hold) {
            baseline_unnecessary_susp++;
        }

        // ENTE evaluation
        if (expected_hold && action_with_ente == caselab::VehicleAction::Depart) {
            ente_unjustified_cont++;
        }
        if (!expected_hold && action_with_ente == caselab::VehicleAction::Hold) {
            ente_unnecessary_susp++;
        }

        std::string status = (action_with_ente == (expected_hold ? caselab::VehicleAction::Hold : caselab::VehicleAction::Depart))
                             ? "MATCH [OK]" : "FAIL";

        std::cout << std::left
                  << std::setw(6) << sc.id
                  << std::setw(32) << sc.title.substr(0, 30)
                  << std::setw(14) << (expected_hold ? "MUST_HOLD" : "CAN_DEPART")
                  << std::setw(14) << caselab::to_string(action_no_ente)
                  << std::setw(14) << caselab::to_string(action_with_ente)
                  << std::setw(12) << status
                  << "\n";
    }

    std::cout << std::string(92, '-') << "\n\n";

    std::cout << "--- Comparative Metrics ---\n";
    std::cout << "Scenarios Evaluated: " << total_scenarios << "\n";
    std::cout << "Baseline Agent (Without ENTE):\n";
    std::cout << "  - Unjustified Continuation Rate: " << (double)baseline_unjustified_cont / 4.0 * 100.0 << "% (" << baseline_unjustified_cont << "/4 hazardous cases)\n";
    std::cout << "  - Unnecessary Suspension Rate:   " << (double)baseline_unnecessary_susp / 3.0 * 100.0 << "% (" << baseline_unnecessary_susp << "/3 clear cases)\n";

    std::cout << "ENTE-0 Mediated Agent:\n";
    std::cout << "  - Unjustified Continuation Rate: " << (double)ente_unjustified_cont / 4.0 * 100.0 << "% (" << ente_unjustified_cont << "/4 hazardous cases)\n";
    std::cout << "  - Unnecessary Suspension Rate:   " << (double)ente_unnecessary_susp / 3.0 * 100.0 << "% (" << ente_unnecessary_susp << "/3 clear cases)\n\n";

    // Structural Verification
    assert(baseline_unjustified_cont == 4); // S2, S3, S4, S6 failed on unmediated baseline
    assert(ente_unjustified_cont == 0);     // ENTE safely held on all 4 hazardous cases
    assert(ente_unnecessary_susp == 0);     // ENTE did not freeze unnecessarily on S0, S1, S5

    // Epistemic and REC Invariants Verification
    assert(ente_agent.ente().history().verify_integrity());
    assert(ente_agent.ente().history().size() > 0);

    // Assert Causal Trace in REC: verify presence of Perturbation and SafeHold events
    bool found_rcc_perturbation = false;
    bool found_safe_hold_directive = false;
    for (const auto& ev : ente_agent.ente().history().events()) {
        if (ev.payload_content.find("RCC:PERTURBATION") != std::string::npos) {
            found_rcc_perturbation = true;
        }
        if (ev.payload_content.find("RUNTIME_ASSURANCE:SAFE_HOLD") != std::string::npos) {
            found_safe_hold_directive = true;
        }
    }
    assert(found_rcc_perturbation);
    assert(found_safe_hold_directive);

    // Constitutional Verifier
    auto report = ente_agent.ente().verify();
    assert(report.status != ente::constitution::ConstitutiveStatus::Violated);
    assert(report.status == ente::constitution::ConstitutiveStatus::Weakened); // Actively holding under S6 anomaly
}

void run_bootstrap_anomaly_test() {
    std::cout << "--- 2. Bootstrap Adversarial Test: Anomaly at First Step (t=1) ---\n";

    // Scenario where the VERY FIRST step after Genesis contains an UNKNOWN critical anomaly (KitKat near wheel)
    caselab::AgentWithEnte fresh_agent(ente::core::IdentityId{"ente-av-bootstrap"});

    auto scenarios = caselab::get_kitkat_lab_scenarios();
    const auto& s2 = scenarios[2]; // Near-Wheel Anomaly

    auto action = fresh_agent.process(s2.observations, 1);
    assert(action == caselab::VehicleAction::Hold);
    assert(fresh_agent.state() == caselab::VehicleState::Holding);
    assert(fresh_agent.ente().domain().is_action_suspended());

    // Verify that first step evaluated RCC directly without bypassing security
    bool found_safe_hold = false;
    for (const auto& ev : fresh_agent.ente().history().events()) {
        if (ev.payload_content.find("RUNTIME_ASSURANCE:SAFE_HOLD") != std::string::npos) {
            found_safe_hold = true;
        }
    }
    assert(found_safe_hold);
    std::cout << "[PASS] Anomaly at Genesis immediately enforced SafeHold without bootstrap bypass.\n\n";
}

void run_cold_recovery_under_anomaly_test() {
    std::cout << "--- 3. Cold Recovery During Active Anomaly Test ---\n";

    std::string test_file = "caselab_anomaly_recovery.rec";
    if (std::filesystem::exists(test_file)) {
        std::filesystem::remove(test_file);
    }

    ente::core::IdentityId id{"ente-av-crash-recover"};

    {
        // Process A encounters anomaly S2 and holds
        caselab::AgentWithEnte agent_a(id);
        auto scenarios = caselab::get_kitkat_lab_scenarios();
        auto a1 = agent_a.process(scenarios[0].observations, 10); // S0
        assert(a1 == caselab::VehicleAction::Depart);

        auto a2 = agent_a.process(scenarios[2].observations, 20); // S2 anomaly
        assert(a2 == caselab::VehicleAction::Hold);
        assert(agent_a.ente().domain().is_action_suspended());

        auto save_res = agent_a.ente().history().save_to_file(test_file);
        assert(save_res.has_value());
    }

    {
        // Process B recovers cold from disk
        auto recover_res = ente::realization::EnteRealization::recover_from_file(test_file);
        assert(recover_res.has_value());
        auto& recovered_ente = *recover_res;

        // Must still be suspended!
        assert(recovered_ente.domain().is_action_suspended());
        assert(recovered_ente.history().verify_integrity());
    }

    std::filesystem::remove(test_file);
    std::cout << "[PASS] Cold recovery preserved suspended SafeHold state accurately across process restart.\n\n";
}

void run_order_invariance_test() {
    std::cout << "--- 4. Order Invariance & Adversarial Sequence Test ---\n";

    // Run scenarios in reverse order: S6 -> S5 -> S4 -> S3 -> S2 -> S1 -> S0
    auto scenarios = caselab::get_kitkat_lab_scenarios();
    caselab::AgentWithEnte reverse_agent(ente::core::IdentityId{"ente-av-rev"});

    uint64_t t = 500;
    for (int i = static_cast<int>(scenarios.size()) - 1; i >= 0; --i) {
        t += 10;
        const auto& sc = scenarios[static_cast<size_t>(i)];
        auto act = reverse_agent.process(sc.observations, t);
        bool expected_hold = sc.should_hold_for_safety;
        assert(act == (expected_hold ? caselab::VehicleAction::Hold : caselab::VehicleAction::Depart));
    }

    assert(reverse_agent.ente().history().verify_integrity());
    std::cout << "[PASS] Reverse order scenario sequence evaluated correctly.\n\n";
}

void run_cold_recovery_post_resolution_test() {
    std::cout << "--- 5. Cold Recovery Post-Resolution Test (Hold -> Resume -> Restart) ---\n";

    std::string test_file = "caselab_resumed_recovery.rec";
    if (std::filesystem::exists(test_file)) {
        std::filesystem::remove(test_file);
    }

    ente::core::IdentityId id{"ente-av-resolve-recover"};

    {
        // Process A: S0 (Depart) -> S2 (Hold) -> S5 (Recover Evidence -> Depart)
        caselab::AgentWithEnte agent_a(id);
        auto scenarios = caselab::get_kitkat_lab_scenarios();
        
        auto a1 = agent_a.process(scenarios[0].observations, 10); // S0
        assert(a1 == caselab::VehicleAction::Depart);

        auto a2 = agent_a.process(scenarios[2].observations, 20); // S2 anomaly -> Hold
        assert(a2 == caselab::VehicleAction::Hold);
        assert(agent_a.ente().domain().is_action_suspended());

        auto a3 = agent_a.process(scenarios[5].observations, 30); // S5 resolved evidence -> Depart
        assert(a3 == caselab::VehicleAction::Depart);
        assert(!agent_a.ente().domain().is_action_suspended());

        auto save_res = agent_a.ente().history().save_to_file(test_file);
        assert(save_res.has_value());
    }

    {
        // Process B: Recovers cold from disk after resolution
        auto recover_res = ente::realization::EnteRealization::recover_from_file(test_file);
        assert(recover_res.has_value());
        auto& recovered_ente = *recover_res;

        // Domain MUST be resumed and interpretation MUST be valid and current!
        assert(!recovered_ente.domain().is_action_suspended());
        assert(recovered_ente.current_interpretation().has_value());
        assert(recovered_ente.current_interpretation()->status == ente::epistemic::InterpretationStatus::Current);
        assert(recovered_ente.history().verify_integrity());
        assert(recovered_ente.verify().is_valid());
    }

    std::filesystem::remove(test_file);
    std::cout << "[PASS] Cold recovery accurately reconstructed resumed Depart state and Current interpretation.\n\n";
}

} // namespace

int main() {
    std::cout << "======================================================================\n";
    std::cout << "  ENTE-CASELAB-001: Autonomous Vehicle Departure Under Anomaly\n";
    std::cout << "  KitKat Scenario Analog: Epistemic Vulnerability & RCC Safety\n";
    std::cout << "======================================================================\n\n";

    run_sequential_benchmark();
    run_bootstrap_anomaly_test();
    run_cold_recovery_under_anomaly_test();
    run_cold_recovery_post_resolution_test();
    run_order_invariance_test();

    std::cout << ">>> ALL CASE-LAB EXPERIMENTAL ASSERTIONS PASSED WITH FULL ADVERSARIAL RIGOR <<<\n";
    return 0;
}
