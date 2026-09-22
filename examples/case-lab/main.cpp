#include "case_domain.hpp"
#include "agent_without_ente.hpp"
#include "agent_with_ente.hpp"
#include "scenarios.hpp"
#include "ente/attestation/rats.hpp"
#include "ente/constitution/verifier.hpp"

#include <iostream>
#include <iomanip>
#include <cassert>

int main() {
    std::cout << "======================================================================\n";
    std::cout << "  ENTE-CASELAB-001: Autonomous Vehicle Departure Under Anomaly\n";
    std::cout << "  KitKat Scenario Analog: Epistemic Vulnerability & RCC Safety\n";
    std::cout << "======================================================================\n\n";

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

    // Constitutional Verifier
    auto report = ente_agent.ente().verify();
    assert(report.status != ente::constitution::ConstitutiveStatus::Violated);
    assert(report.status == ente::constitution::ConstitutiveStatus::Weakened); // Actively holding under S6 anomaly

    std::cout << ">>> ALL CASE-LAB EXPERIMENTAL ASSERTIONS PASSED SUCCESSFULLY <<<\n";
    return 0;
}
