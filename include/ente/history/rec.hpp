#pragma once

#include "ente/core/types.hpp"
#include "ente/history/event.hpp"
#include <vector>
#include <expected>
#include <optional>
#include <span>

#include <unordered_map>

namespace ente::history {

class RecoverableHistory {
public:
    RecoverableHistory() = default;

    // Append an event enforcing chronological order and cryptographic linking
    [[nodiscard]] std::expected<void, core::EnteError> append(HistoryEvent event) noexcept;

    // Validate the complete hash-chain and temporal integrity (C10, C12)
    [[nodiscard]] bool verify_integrity() const noexcept;

    // Validate the current HEAD and its direct predecessor in O(1).
    [[nodiscard]] bool verify_tail() const noexcept;

    [[nodiscard]] size_t size() const noexcept { return events_.size(); }
    [[nodiscard]] bool empty() const noexcept { return events_.empty(); }
    [[nodiscard]] const std::vector<HistoryEvent>& events() const noexcept { return events_; }
    [[nodiscard]] const HistoryEvent& head() const { return events_.back(); }
    [[nodiscard]] core::Digest head_digest() const noexcept;

    [[nodiscard]] std::optional<HistoryEvent> find_event(const core::EventId& id) const noexcept;
    [[nodiscard]] bool contains_event(const core::EventId& id) const noexcept;
    
    // Canonical event hash input serialization
    [[nodiscard]] static std::string compute_event_hash_string(const HistoryEvent& ev) noexcept;

    // Create an event with auto-calculated digests
    [[nodiscard]] HistoryEvent create_event(
        EventKind kind,
        const core::IdentityId& identity,
        core::LogicalTime time,
        std::vector<core::EventId> causal_predecessors,
        std::vector<core::EvidenceId> evidence_refs,
        std::string payload,
        std::string authority_id = "auth-root",
        std::string authority_epoch = "epoch-0"
    ) const noexcept;

    // File persistence and crash-recovery methods
    [[nodiscard]] std::expected<void, core::EnteError> save_to_file(std::string_view filepath) const noexcept;
    [[nodiscard]] static std::expected<RecoverableHistory, core::EnteError> load_from_file(std::string_view filepath) noexcept;

    // Direct mutation for tamper-testing (used strictly by tests to falsify C12)
    void tamper_event_payload_for_testing(size_t index, std::string_view corrupted_payload) noexcept;

private:
    void rebuild_index() noexcept;

    std::vector<HistoryEvent> events_;
    std::unordered_map<std::string, size_t> event_index_;
};

} // namespace ente::history
