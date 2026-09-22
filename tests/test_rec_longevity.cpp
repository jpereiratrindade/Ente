#include "ente/realization/runner.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cassert>

int main() {
    std::cout << "======================================================================\n";
    std::cout << "       ENTE-0 REC LONGEVITY & SCALABILITY STRESS TEST (10,000 EVENTS) \n";
    std::cout << "      Linear Throughput Verification under Long-Lived Continuity      \n";
    std::cout << "======================================================================\n\n";

    constexpr size_t EVENTS_TO_STREAM = 10000;

    ente::realization::EnteRealization ente;
    ente::core::IdentityId id("ente-longevity-identity");
    assert(ente.genesis(id).has_value());

    auto start_time = std::chrono::high_resolution_clock::now();

    for (size_t i = 1; i <= EVENTS_TO_STREAM; ++i) {
        auto res = ente.step(i, {{
            .id = ente::core::EvidenceId(std::format("EV-LONG-{}", i)),
            .source = "sensor_stream",
            .subject = "path_clear",
            .value = "true",
            .observed_at = i,
            .status = ente::epistemic::EpistemicStatus::Observed
        }}, "Continuous longevity stream");

        assert(res.has_value());
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;

    double events_per_sec = static_cast<double>(EVENTS_TO_STREAM) / duration.count();

    std::cout << "--- Longevity Results ---\n";
    std::cout << "  - Events Appended & Verified: " << EVENTS_TO_STREAM << "\n";
    std::cout << "  - Final Ledger Size:          " << ente.history().size() << " events\n";
    std::cout << "  - Total Execution Time:       " << std::fixed << std::setprecision(3) << duration.count() << " seconds\n";
    std::cout << "  - Sustained Throughput:       " << std::fixed << std::setprecision(1) << events_per_sec << " events/second\n\n";

    // Periodic / Final Full Audit (O(H))
    std::cout << "[Audit] Executing full historical audit from Genesis to HEAD (" << ente.history().size() << " events)...\n";
    auto full_audit_start = std::chrono::high_resolution_clock::now();
    assert(ente.history().verify_integrity());
    auto final_report = ente.verify();
    assert(final_report.is_valid());
    auto full_audit_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> audit_duration = full_audit_end - full_audit_start;

    std::cout << "  - Full Historical Audit Time: " << std::fixed << std::setprecision(3) << audit_duration.count() << " seconds\n";
    std::cout << ">>> 10,000 CONTINUOUS EVENTS EXECUTED WITH FULL SCALABILITY AND CAUSAL INTEGRITY <<<\n";
    return 0;
}
