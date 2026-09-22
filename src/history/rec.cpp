#include "ente/history/rec.hpp"
#include "ente/core/hash.hpp"
#include <format>
#include <numeric>

namespace ente::history {

namespace {

[[nodiscard]] std::string compute_event_hash_string(const HistoryEvent& ev) noexcept {
    std::string causal_concat;
    for (const auto& c : ev.causal_predecessors) {
        causal_concat += c.view();
        causal_concat += ",";
    }

    std::string evidence_concat;
    for (const auto& e : ev.evidence_refs) {
        evidence_concat += e.view();
        evidence_concat += ",";
    }

    return std::format("{}:{}:{}:{}:{}:{}:{}:{}",
        ev.id.view(),
        to_string(ev.kind),
        ev.identity.view(),
        ev.logical_time,
        ev.previous_event_digest.value,
        causal_concat,
        evidence_concat,
        ev.payload_digest.value
    );
}

} // namespace

core::Digest RecoverableHistory::head_digest() const noexcept {
    if (events_.empty()) {
        return core::Digest();
    }
    return events_.back().event_digest;
}

HistoryEvent RecoverableHistory::create_event(
    EventKind kind,
    const core::IdentityId& identity,
    core::LogicalTime time,
    std::vector<core::EventId> causal_predecessors,
    std::vector<core::EvidenceId> evidence_refs,
    std::string payload
) const noexcept {
    core::EventId ev_id(std::format("E{:04d}", events_.size()));
    core::Digest prev_digest = head_digest();
    core::Digest p_digest = core::HashUtil::sha256(payload);

    HistoryEvent ev{
        .id = ev_id,
        .kind = kind,
        .identity = identity,
        .logical_time = time,
        .previous_event_digest = prev_digest,
        .causal_predecessors = std::move(causal_predecessors),
        .evidence_refs = std::move(evidence_refs),
        .payload_content = std::move(payload),
        .payload_digest = p_digest,
        .event_digest = core::Digest()
    };

    std::string hash_input = compute_event_hash_string(ev);
    ev.event_digest = core::HashUtil::sha256(hash_input);

    return ev;
}

std::expected<void, core::EnteError> RecoverableHistory::append(HistoryEvent event) noexcept {
    // 1. Verify chronological progression
    if (!events_.empty()) {
        if (event.logical_time < events_.back().logical_time) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        if (event.previous_event_digest != events_.back().event_digest) {
            return std::unexpected(core::EnteError::HistoryGap);
        }
    } else {
        if (!event.previous_event_digest.is_zero()) {
            return std::unexpected(core::EnteError::GenesisNotEstablished);
        }
    }

    // 2. Verify payload digest
    core::Digest computed_payload = core::HashUtil::sha256(event.payload_content);
    if (computed_payload != event.payload_digest) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }

    // 3. Verify event digest
    std::string hash_input = compute_event_hash_string(event);
    core::Digest computed_event_digest = core::HashUtil::sha256(hash_input);
    if (computed_event_digest != event.event_digest) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }

    // 4. Verify RIT causal links exist in history
    for (const auto& causal_id : event.causal_predecessors) {
        bool found = false;
        for (const auto& past_ev : events_) {
            if (past_ev.id == causal_id) {
                found = true;
                break;
            }
        }
        if (!found) {
            return std::unexpected(core::EnteError::InvalidPredecessor);
        }
    }

    events_.push_back(std::move(event));
    return {};
}

bool RecoverableHistory::verify_integrity() const noexcept {
    if (events_.empty()) {
        return true;
    }

    core::Digest expected_prev; // Starts at zero

    for (size_t i = 0; i < events_.size(); ++i) {
        const auto& ev = events_[i];

        // 1. Check previous event linking
        if (ev.previous_event_digest != expected_prev) {
            return false;
        }

        // 2. Verify payload integrity
        core::Digest payload_hash = core::HashUtil::sha256(ev.payload_content);
        if (payload_hash != ev.payload_digest) {
            return false;
        }

        // 3. Verify event hash
        std::string hash_input = compute_event_hash_string(ev);
        core::Digest event_hash = core::HashUtil::sha256(hash_input);
        if (event_hash != ev.event_digest) {
            return false;
        }

        // 4. Verify chronological order
        if (i > 0 && ev.logical_time < events_[i - 1].logical_time) {
            return false;
        }

        expected_prev = ev.event_digest;
    }

    return true;
}

std::optional<HistoryEvent> RecoverableHistory::find_event(const core::EventId& id) const noexcept {
    for (const auto& ev : events_) {
        if (ev.id == id) {
            return ev;
        }
    }
    return std::nullopt;
}

void RecoverableHistory::tamper_event_payload_for_testing(size_t index, std::string_view corrupted_payload) noexcept {
    if (index < events_.size()) {
        events_[index].payload_content = corrupted_payload;
    }
}

} // namespace ente::history
