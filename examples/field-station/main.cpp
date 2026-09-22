#include "field_domain.hpp"
#include "scenarios.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cassert>

int main() {
    std::cout << "==========================================================\n";
    std::cout << "  ENTE FIELD STATION (ENTE-VISUAL-001)\n";
    std::cout << "  Autonomous Field Station under Epistemic Continuity\n";
    std::cout << "==========================================================\n\n";

    auto logs = fieldstation::FieldStationScenarios::run_all();

    for (const auto& l : logs) {
        std::cout << std::format("[ACT {} · {}] Event: {}\n", l.act_id, l.act_name, l.event_type);
        std::cout << std::format("  Hardware: {} | Time: {} | Hash: {}\n", l.hardware_id, l.logical_time, l.hash);
        std::cout << std::format("  Sensors: Probe A = {} | Probe B = {}\n", l.soil_a, l.soil_b);
        std::cout << std::format("  ENTE: RCC = {} -> Directive = {} -> Valve = {}\n", 
            l.rcc_status, l.assurance_directive, l.valve_action);
        std::cout << std::format("  Summary: {}\n\n", l.description);
    }

    // Asserções Constitutivas
    assert(logs.size() == 5);
    assert(logs[0].event_type == "GENESIS");
    assert(logs[1].valve_action == "OPEN_VALVE");
    assert(logs[2].valve_action == "CLOSE_VALVE"); // Contradição -> SafeHold
    assert(logs[3].hardware_id == "RP-B104");      // Migração RIT
    assert(logs[4].event_type == "COLD_RECOVERY"); // Cold recovery preserva SafeHold

    // Salvar events.json para a interface web
    std::string json_data = fieldstation::FieldStationScenarios::to_json(logs);

    // Tenta salvar em web/events.json se o diretório existir
    try {
        std::filesystem::create_directories("examples/field-station/web");
        std::ofstream out("examples/field-station/web/events.json");
        if (out.is_open()) {
            out << json_data;
            std::cout << "[OK] Exported events.json to examples/field-station/web/events.json\n";
        }
    } catch (...) {
        // Fallback silencioso se executado em outro diretório
    }

    std::cout << "\n[PASS] All 5 Acts of ENTE Field Station verified with strict constitutional fidelity.\n";
    return 0;
}
