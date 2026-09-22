#pragma once

#include "ente/core/types.hpp"
#include <string>
#include <vector>
#include <optional>
#include <expected>

namespace ente::identity {

struct MaterialAnchorTag {};
using MaterialAnchorId = core::StrongId<MaterialAnchorTag>;

enum class SubstrateType : uint8_t {
    SimulatedMemory,
    TpmProtectedDevice,
    SecureEnclave,
    DistributedNode
};

[[nodiscard]] constexpr std::string_view to_string(SubstrateType t) noexcept {
    switch (t) {
        case SubstrateType::SimulatedMemory: return "SIMULATED_MEMORY";
        case SubstrateType::TpmProtectedDevice: return "TPM_PROTECTED_DEVICE";
        case SubstrateType::SecureEnclave: return "SECURE_ENCLAVE";
        case SubstrateType::DistributedNode: return "DISTRIBUTED_NODE";
    }
    return "UNKNOWN_SUBSTRATE";
}

struct MaterialAnchor {
    MaterialAnchorId id;
    SubstrateType type;
    std::string hardware_fingerprint; // e.g., public key or endorsement digest
};

// Material Binding: Legitimate constitutional association between an ENTE identity and a material anchor
struct MaterialBinding {
    core::IdentityId identity;
    MaterialAnchor anchor;
    core::LogicalTime bound_at{0};
    std::optional<MaterialAnchorId> previous_anchor; // RIT lineage of hardware replacement
    core::Digest binding_digest;
};

class MaterialBindingRegistry {
public:
    MaterialBindingRegistry() = default;

    // Initial binding at Genesis
    [[nodiscard]] std::expected<MaterialBinding, core::EnteError> bind_initial_anchor(
        const core::IdentityId& id,
        MaterialAnchor anchor,
        core::LogicalTime time = 0
    ) noexcept;

    // Hardware replacement under RIT without losing Identity or requiring new Genesis (Ship of Theseus)
    [[nodiscard]] std::expected<MaterialBinding, core::EnteError> migrate_to_anchor(
        MaterialAnchor new_anchor,
        core::LogicalTime time
    ) noexcept;

    [[nodiscard]] bool has_active_binding() const noexcept { return !bindings_.empty(); }
    [[nodiscard]] const MaterialBinding& active_binding() const { return bindings_.back(); }
    [[nodiscard]] const std::vector<MaterialBinding>& history() const noexcept { return bindings_; }

private:
    std::vector<MaterialBinding> bindings_;
};

} // namespace ente::identity
