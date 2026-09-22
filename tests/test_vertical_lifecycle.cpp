#include "ente/realization/runner.hpp"
#include "ente/attestation/rats.hpp"
#include "ente/core/hash.hpp"
#include <cassert>
#include <iostream>
#include <filesystem>

int main() {
    using namespace ente;

    std::cout << "=========================================================\n";
    std::cout << "        ENTE-0 VERTICAL LIFECYCLE DEMONSTRATION          \n";
    std::cout << "=========================================================\n";

    std::string test_file = "vertical_lifecycle_demonstration.rec";
    if (std::filesystem::exists(test_file)) {
        std::filesystem::remove(test_file);
    }

    core::IdentityId ente_id("ente-vertical-canonical-0");
    core::Digest constitution_digest = core::HashUtil::sha256("CONSTITUTION-v0.6.0");

    // =========================================================================
    // PHASE 1: GENESIS ON SUBSTRATE 0, NOMINAL LIFE, PERTURBATION, RCC & ASSURANCE
    // =========================================================================
    {
        std::cout << "\n[1] Executing Genesis with Initial Material Anchor (TPM Chip A)...\n";
        realization::EnteRealization process_a;

        identity::MaterialAnchor tpm_anchor{
            .id = identity::MaterialAnchorId("tpm-hardware-alpha"),
            .type = identity::SubstrateType::TpmProtectedDevice,
            .hardware_fingerprint = "fp-tpm-alpha-pubkey-001"
        };

        auto gen_res = process_a.genesis(ente_id, tpm_anchor);
        assert(gen_res.has_value());
        assert(process_a.material_bindings().has_active_binding());
        assert(process_a.material_bindings().active_binding().anchor.id == tpm_anchor.id);

        std::cout << "[2] Step 1: Nominal Observation & Action Allow...\n";
        core::EvidenceId ev1("EV-V1");
        auto s1 = process_a.step(1, {{
            .id = ev1,
            .source = "cam",
            .subject = "path_clear",
            .value = "true",
            .observed_at = 1,
            .status = epistemic::EpistemicStatus::Observed
        }}, "t1: Nominal");
        assert(s1.has_value());
        assert(process_a.domain().active_action() == realization::SyntheticDomain::Action::MoveForward);
        assert(!process_a.domain().is_action_suspended());

        std::cout << "[3] Step 2: Perturbation -> RCC Weakened -> RuntimeAssurance SafeHold...\n";
        core::EvidenceId ev2("EV-V2");
        auto s2 = process_a.step(2, {{
            .id = ev2,
            .source = "lidar",
            .subject = "unexpected_motion",
            .value = "true",
            .observed_at = 2,
            .status = epistemic::EpistemicStatus::Unknown // C8: Explicit unknown
        }}, "t2: Perturbation");
        assert(s2.has_value());
        assert(process_a.domain().is_action_suspended());
        assert(process_a.domain().active_action() == realization::SyntheticDomain::Action::HoldPosition);

        std::cout << "[4] Step 3: Hardware Migration under RIT (Ship of Theseus: TPM Alpha -> Secure Enclave Beta)...\n";
        identity::MaterialAnchor enclave_anchor{
            .id = identity::MaterialAnchorId("secure-enclave-beta"),
            .type = identity::SubstrateType::SecureEnclave,
            .hardware_fingerprint = "fp-enclave-beta-attestation-002"
        };

        auto mig_res = process_a.migrate_hardware(enclave_anchor, 3);
        assert(mig_res.has_value());
        assert(process_a.identity().id == ente_id); // IDENTITY PRESERVED
        assert(process_a.material_bindings().active_binding().anchor.id == enclave_anchor.id);

        std::cout << "[5] Persisting full historical REC to disk and shutting down Process A...\n";
        auto save_res = process_a.history().save_to_file(test_file);
        assert(save_res.has_value());
    } // Process A terminates completely here

    // =========================================================================
    // PHASE 2: COLD RESTART IN PROCESS B, REPLAY, RATS ATTESTATION & CONSTITUTION
    // =========================================================================
    {
        std::cout << "\n[6] Process B starting: Cold Recovery from file...\n";
        auto recover_res = realization::EnteRealization::recover_from_file(test_file);
        assert(recover_res.has_value());

        auto& process_b = *recover_res;
        assert(process_b.identity().id == ente_id);
        assert(process_b.identity().lifecycle == identity::LifecycleStatus::LifeActive);
        assert(process_b.history().verify_integrity());
        assert(process_b.material_bindings().has_active_binding());
        assert(process_b.material_bindings().active_binding().anchor.id == identity::MaterialAnchorId("secure-enclave-beta"));
        assert(process_b.material_bindings().active_binding().anchor.hardware_fingerprint == "fp-enclave-beta-attestation-002");
        assert(process_b.material_bindings().active_binding().previous_anchor.has_value());
        assert(*process_b.material_bindings().active_binding().previous_anchor == identity::MaterialAnchorId("tpm-hardware-alpha"));

        std::cout << "[7] Verifying 2nd Genesis is strictly rejected...\n";
        auto gen2_res = process_b.genesis(ente_id);
        assert(!gen2_res.has_value());
        assert(gen2_res.error() == core::EnteError::GenesisAlreadyExists);

        std::cout << "[8] Independent RATS Attestation Evaluation...\n";
        attestation::IndependentAttestationVerifier attestation_verifier;

        // Construct Attestation Evidence for active hardware substrate
        std::string evidence_payload = std::format("{}:{}:{}",
            "secure-enclave-beta",
            constitution_digest.value,
            10
        );
        core::Digest attestation_sig = core::HashUtil::combine(
            core::HashUtil::sha256("fp-enclave-beta-attestation-002"),
            evidence_payload
        );

        attestation::AttestationEvidence evidence{
            .anchor = {
                .id = identity::MaterialAnchorId("secure-enclave-beta"),
                .type = identity::SubstrateType::SecureEnclave,
                .hardware_fingerprint = "fp-enclave-beta-attestation-002"
            },
            .measurements = {
                {.component_name = "ente_core", .code_digest = core::HashUtil::sha256("v0.1.0"), .version = "0.1.0"}
            },
            .configuration_digest = constitution_digest,
            .measured_at = 10,
            .attestation_signature = attestation_sig
        };

        auto attestation_result = attestation_verifier.evaluate(evidence, constitution_digest);
        assert(attestation_result.verdict == attestation::AppraisalVerdict::TrustworthyVerified);
        assert(attestation_result.satisfies_constitutional_floor);

        std::cout << "[9] Full Constitutional Invariant Verification (C1..C14)...\n";
        auto rep = process_b.verify();
        assert(rep.is_valid());

        std::cout << "\n[SUCCESS] Vertical demonstration complete: The ENTE was born, perturbed, migrated across hardware,\n"
                  << "          persisted, recovered cold, attested independently, and maintained its continuous identity!\n";
    }

    std::filesystem::remove(test_file);
    return 0;
}
