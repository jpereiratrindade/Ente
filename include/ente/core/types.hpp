#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <format>
#include <compare>
#include <expected>

namespace ente::core {

// Strong Typed Identifier template
template <typename Tag>
struct StrongId {
    std::string value;

    constexpr StrongId() = default;
    explicit StrongId(std::string_view v) : value(v) {}

    [[nodiscard]] constexpr std::string_view view() const noexcept { return value; }
    [[nodiscard]] constexpr bool empty() const noexcept { return value.empty(); }

    auto operator<=>(const StrongId&) const = default;
};

// Strongly-typed IDs matching the Ontology
struct IdentityTag {};
using IdentityId = StrongId<IdentityTag>;

struct GenesisTag {};
using GenesisId = StrongId<GenesisTag>;

struct EventTag {};
using EventId = StrongId<EventTag>;

struct EvidenceTag {};
using EvidenceId = StrongId<EvidenceTag>;

struct InterpretationTag {};
using InterpretationId = StrongId<InterpretationTag>;

struct StateTag {};
using StateId = StrongId<StateTag>;

struct ActionTransactionTag {};
using ActionTransactionId = StrongId<ActionTransactionTag>;

// Logical time for ordering without hardware clock dependencies
using LogicalTime = uint64_t;

// Standard Cryptographic / Hash Digest (Hex-encoded 64-char string representing SHA-256)
struct Digest {
    std::string value;

    constexpr Digest() : value("0000000000000000000000000000000000000000000000000000000000000000") {}
    explicit Digest(std::string_view v) : value(v) {}

    [[nodiscard]] constexpr bool is_zero() const noexcept {
        return value == "0000000000000000000000000000000000000000000000000000000000000000";
    }

    auto operator<=>(const Digest&) const = default;
};

// Universal Error Codes for ENTE-0
enum class EnteError : uint8_t {
    GenesisAlreadyExists,
    GenesisNotEstablished,
    IdentityMismatch,
    HistoryCorrupt,
    HistoryGap,
    UntraceableTransition,
    ConstitutiveViolation,
    InsufficientEvidence,
    UnknownStateEncountered,
    InvalidPredecessor,
    DuplicateEventId,
    InvalidLogicalTime,
    InvalidSignature,
    PersistenceFailure,
    DuplicateActionTransaction,
    ActionTransactionNotFound,
    InvalidActionTransition
};

[[nodiscard]] constexpr std::string_view to_string(EnteError err) noexcept {
    switch (err) {
        case EnteError::GenesisAlreadyExists: return "GenesisAlreadyExists";
        case EnteError::GenesisNotEstablished: return "GenesisNotEstablished";
        case EnteError::IdentityMismatch: return "IdentityMismatch";
        case EnteError::HistoryCorrupt: return "HistoryCorrupt";
        case EnteError::HistoryGap: return "HistoryGap";
        case EnteError::UntraceableTransition: return "UntraceableTransition";
        case EnteError::ConstitutiveViolation: return "ConstitutiveViolation";
        case EnteError::InsufficientEvidence: return "InsufficientEvidence";
        case EnteError::UnknownStateEncountered: return "UnknownStateEncountered";
        case EnteError::InvalidPredecessor: return "InvalidPredecessor";
        case EnteError::DuplicateEventId: return "DuplicateEventId";
        case EnteError::InvalidLogicalTime: return "InvalidLogicalTime";
        case EnteError::InvalidSignature: return "InvalidSignature";
        case EnteError::PersistenceFailure: return "PersistenceFailure";
        case EnteError::DuplicateActionTransaction: return "DuplicateActionTransaction";
        case EnteError::ActionTransactionNotFound: return "ActionTransactionNotFound";
        case EnteError::InvalidActionTransition: return "InvalidActionTransition";
    }
    return "UnknownError";
}

} // namespace ente::core

template <typename Tag>
struct std::formatter<ente::core::StrongId<Tag>, char> : std::formatter<std::string_view, char> {
    auto format(const ente::core::StrongId<Tag>& id, std::format_context& ctx) const {
        return std::formatter<std::string_view, char>::format(id.view(), ctx);
    }
};

template <>
struct std::formatter<ente::core::Digest, char> : std::formatter<std::string_view, char> {
    auto format(const ente::core::Digest& d, std::format_context& ctx) const {
        return std::formatter<std::string_view, char>::format(d.value, ctx);
    }
};

template <>
struct std::formatter<ente::core::EnteError, char> : std::formatter<std::string_view, char> {
    auto format(ente::core::EnteError err, std::format_context& ctx) const {
        return std::formatter<std::string_view, char>::format(ente::core::to_string(err), ctx);
    }
};
