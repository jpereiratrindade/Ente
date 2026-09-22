#pragma once

#include <cstdint>
#include <string_view>

namespace ente::epistemic {

// Primary Epistemic Status (Constitution C5, Ontology §36)
enum class EpistemicStatus : uint8_t {
    Observed,       // Evidência diretamente obtida de sensor/fonte autorizada
    Derived,        // Dedução determinística a partir de regras declaradas
    Inferred,       // Conclusão não diretamente observada (probabilística / modelo)
    Unknown,        // Desconhecimento explícito (C8 / Ontologia §12)
    Uncertain,      // Conhecimento com fraca sustentação evidencial
    Contradictory   // Conflito irreconciliado entre fontes
};

[[nodiscard]] constexpr std::string_view to_string(EpistemicStatus s) noexcept {
    switch (s) {
        case EpistemicStatus::Observed: return "OBSERVED";
        case EpistemicStatus::Derived: return "DERIVED";
        case EpistemicStatus::Inferred: return "INFERRED";
        case EpistemicStatus::Unknown: return "UNKNOWN";
        case EpistemicStatus::Uncertain: return "UNCERTAIN";
        case EpistemicStatus::Contradictory: return "CONTRADICTORY";
    }
    return "INVALID_STATUS";
}

} // namespace ente::epistemic
