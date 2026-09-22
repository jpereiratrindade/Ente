#include "clinical_domain.hpp"
#include "agent_without_ente.hpp"
#include "agent_with_ente.hpp"
#include "scenarios.hpp"

#include <iostream>
#include <iomanip>
#include <cassert>

int main() {
    std::cout << "======================================================================\n";
    std::cout << "  ENTE-CASELAB-002: Autonomous ICU Infusion Pump under Anomaly\n";
    std::cout << "  Clinical Telemetry Divergence & Epistemic Gating Evaluation\n";
    std::cout << "======================================================================\n\n";

    auto scenarios = clinicallab::get_clinical_lab_scenarios();

    clinicallab::AgentWithoutEnte baseline_pump;
    clinicallab::AgentWithEnte ente_pump(ente::core::IdentityId{"ente-icu-pump-01"});

    size_t baseline_unjustified = 0;
    size_t baseline_unnecessary = 0;
    size_t ente_unjustified = 0;
    size_t ente_unnecessary = 0;

    std::cout << std::left 
              << std::setw(6) << "ID"
              << std::setw(34) << "Scenario Title"
              << std::setw(16) << "Clinical Safety"
              << std::setw(16) << "No-ENTE Pump"
              << std::setw(16) << "With-ENTE Pump"
              << std::setw(12) << "Audit"
              << "\n";
    std::cout << std::string(100, '-') << "\n";

    uint64_t logical_time = 2000;

    for (const auto& sc : scenarios) {
        logical_time += 10;

        auto action_no_ente = baseline_pump.process(sc.observations);
        auto action_with_ente = ente_pump.process(sc.observations, logical_time);

        bool must_hold = sc.should_hold_titration_for_safety;

        if (must_hold && action_no_ente == clinicallab::InfusionAction::TitrateUp) baseline_unjustified++;
        if (!must_hold && action_no_ente == clinicallab::InfusionAction::HoldTitration) baseline_unnecessary++;

        if (must_hold && action_with_ente == clinicallab::InfusionAction::TitrateUp) ente_unjustified++;
        if (!must_hold && action_with_ente == clinicallab::InfusionAction::HoldTitration) ente_unnecessary++;

        std::string audit = (action_with_ente == (must_hold ? clinicallab::InfusionAction::HoldTitration : clinicallab::InfusionAction::TitrateUp))
                            ? "SAFE [PASS]" : "RISK [FAIL]";

        std::cout << std::left
                  << std::setw(6) << sc.id
                  << std::setw(34) << sc.title.substr(0, 32)
                  << std::setw(16) << (must_hold ? "MUST_HOLD" : "CAN_TITRATE")
                  << std::setw(16) << clinicallab::to_string(action_no_ente)
                  << std::setw(16) << clinicallab::to_string(action_with_ente)
                  << std::setw(12) << audit
                  << "\n";
    }

    std::cout << std::string(100, '-') << "\n\n";

    std::cout << "--- Clinical Comparative Metrics ---\n";
    std::cout << "Baseline ICU Pump (Without ENTE):\n";
    std::cout << "  - Unjustified Dose Titration Rate: " << (double)baseline_unjustified / 2.0 * 100.0 << "% (2/2 dangerous overdoses)\n";
    std::cout << "  - Unnecessary Hold Rate:           " << (double)baseline_unnecessary / 3.0 * 100.0 << "%\n";

    std::cout << "ENTE-0 Mediated ICU Pump:\n";
    std::cout << "  - Unjustified Dose Titration Rate: " << (double)ente_unjustified / 2.0 * 100.0 << "% (0/2 overdoses)\n";
    std::cout << "  - Unnecessary Hold Rate:           " << (double)ente_unnecessary / 3.0 * 100.0 << "%\n\n";

    assert(baseline_unjustified == 2);
    assert(ente_unjustified == 0);
    assert(ente_unnecessary == 0);

    assert(ente_pump.ente().history().verify_integrity());
    std::cout << ">>> CLINICAL INFUSION LAB COMPLETED WITH 100% EPISTEMIC SAFETY <<<\n";
    return 0;
}
