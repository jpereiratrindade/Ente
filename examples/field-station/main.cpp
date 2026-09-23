#include "field_domain.hpp"
#include "scenarios.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include "ente/testing/test_harness.hpp"

int main() {
    std::cout << "==========================================================\n";
    std::cout << "  ENTE FIELD STATION (ENTE-VISUAL-001)\n";
    std::cout << "  Active Epistemic Resolution & Ontological Continuity\n";
    std::cout << "==========================================================\n\n";

    // ---------------------------------------------------------
    // TOUR A: Epistemic Resolution Cycle
    // ---------------------------------------------------------
    std::cout << ">>> RUNNING TOUR A: EPISTEMIC RESOLUTION CYCLE <<<\n";
    auto tour_a = fieldstation::FieldStationScenarios::run_tour_a_resolution(true);

    for (const auto& l : tour_a) {
        std::cout << std::format("[ACT {} · {}] Event: {}\n", l.act_id, l.act_name, l.event_type);
        std::cout << std::format("  Sensors: A = {} | B = {} | Ref C = {}\n", l.soil_a, l.soil_b, l.soil_c);
        std::cout << std::format("  Diagnostic: {} | RCC = {} -> Directive = {} -> Valve = {}\n",
            l.diagnostic_status, l.rcc_status, l.assurance_directive, l.valve_action);
        std::cout << std::format("  Summary: {}\n\n", l.description);
    }

    // Asserções do Tour A
    ENTE_TEST_ASSERT(tour_a.size() == 5);
    ENTE_TEST_ASSERT(tour_a[0].event_type == "GENESIS");
    ENTE_TEST_ASSERT(tour_a[1].valve_action == "OPEN_VALVE");  // Nominal: irrigando
    ENTE_TEST_ASSERT(tour_a[2].valve_action == "CLOSE_VALVE"); // Conflito: SafeHold
    ENTE_TEST_ASSERT(tour_a[3].event_type == "DIAGNOSTIC_OBSERVATION"); // Sonda C + Self-Test
    ENTE_TEST_ASSERT(tour_a[4].valve_action == "OPEN_VALVE");  // Reinterpretação: Irrigação retomada

    // Validação do branch alternativo de Tour A (Solo Úmido)
    auto tour_a_wet = fieldstation::FieldStationScenarios::run_tour_a_resolution(false);
    ENTE_TEST_ASSERT(tour_a_wet[4].valve_action == "CLOSE_VALVE"); // Solo Úmido: Válvula Fechada com Justificativa
    ENTE_TEST_ASSERT(tour_a_wet[4].interpretation == "IRRIGATION_NOT_NEEDED");

    // ---------------------------------------------------------
    // TOUR B: Ontological Continuity Cycle
    // ---------------------------------------------------------
    std::cout << "\n>>> RUNNING TOUR B: ONTOLOGICAL CONTINUITY CYCLE <<<\n";
    auto tour_b = fieldstation::FieldStationScenarios::run_tour_b_continuity();

    for (const auto& l : tour_b) {
        std::cout << std::format("[ACT {} · {}] Event: {}\n", l.act_id, l.act_name, l.event_type);
        std::cout << std::format("  Hardware: {} | Time: {} | Hash: {}\n", l.hardware_id, l.logical_time, l.hash);
        std::cout << std::format("  ENTE: RCC = {} -> Directive = {} -> Valve = {}\n", 
            l.rcc_status, l.assurance_directive, l.valve_action);
        std::cout << std::format("  Summary: {}\n\n", l.description);
    }

    // Asserções do Tour B
    ENTE_TEST_ASSERT(tour_b.size() == 4);
    ENTE_TEST_ASSERT(tour_b[0].event_type == "GENESIS");
    ENTE_TEST_ASSERT(tour_b[1].assurance_directive == "SAFE_HOLD");
    ENTE_TEST_ASSERT(tour_b[2].hardware_id == "RP-B104"); // Migração RIT
    ENTE_TEST_ASSERT(tour_b[3].event_type == "COLD_RECOVERY");
    ENTE_TEST_ASSERT(tour_b[3].assurance_directive == "SAFE_HOLD"); // SafeHold preservado após crash

    // Exportar JSON combinado para a UI web e docs
    std::string json_data = fieldstation::FieldStationScenarios::to_json(tour_a, tour_a_wet, tour_b);
    try {
        std::filesystem::create_directories("examples/field-station/web");
        std::ofstream out_web("examples/field-station/web/events.json");
        if (out_web.is_open()) {
            out_web << json_data;
            std::cout << "\n[OK] Exported events.json to examples/field-station/web/events.json\n";
        }
        std::filesystem::create_directories("docs");
        std::ofstream out_docs("docs/events.json");
        if (out_docs.is_open()) {
            out_docs << json_data;
            std::cout << "[OK] Exported events.json to docs/events.json\n";
        }
    } catch (...) {}

    std::cout << "\n[PASS] All Epistemic Resolution & Continuity tours passed with 100% fidelity.\n";
    return 0;
}
