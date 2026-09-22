#pragma once

#include <cstdint>
#include <string_view>

namespace ente::rcc {

// Epistemic Actions chosen by RCC (RCC-001 §15, Ontology §16)
enum class EpistemicAction : uint8_t {
    Keep,               // Manter a interpretação atual
    Reobserve,          // Solicitar nova observação
    SeekEvidence,       // Buscar evidência adicional para resolver enfraquecimento
    SuspendAction,      // Suspender a ação corrente cuja precondição perdeu sustentação
    Reinterpret,        // Formular nova interpretação candidata
    SuspendJudgment     // Manter UNKNOWN quando decidir for injustificado
};

[[nodiscard]] constexpr std::string_view to_string(EpistemicAction a) noexcept {
    switch (a) {
        case EpistemicAction::Keep: return "KEEP";
        case EpistemicAction::Reobserve: return "REOBSERVE";
        case EpistemicAction::SeekEvidence: return "SEEK_EVIDENCE";
        case EpistemicAction::SuspendAction: return "SUSPEND_ACTION";
        case EpistemicAction::Reinterpret: return "REINTERPRET";
        case EpistemicAction::SuspendJudgment: return "SUSPEND_JUDGMENT";
    }
    return "UNKNOWN_ACTION";
}

} // namespace ente::rcc
