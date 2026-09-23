#include "ente/realization/runner.hpp"
#include "ente/attestation/rats.hpp"
#include "ente/core/hash.hpp"
#include "ente/testing/test_harness.hpp"
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
        ENTE_TEST_ASSERT(gen_res.has_value());
        ENTE_TEST_ASSERT(process_a.material_bindings().has_active_binding());
        ENTE_TEST_ASSERT(process_a.material_bindings().active_binding().anchor.id == tpm_anchor.id);

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
        ENTE_TEST_ASSERT(s1.has_value());
        ENTE_TEST_ASSERT(process_a.domain().active_action() == realization::SyntheticDomain::Action::MoveForward);
        ENTE_TEST_ASSERT(!process_a.domain().is_action_suspended());

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
        ENTE_TEST_ASSERT(s2.has_value());
        ENTE_TEST_ASSERT(process_a.domain().is_action_suspended());
        ENTE_TEST_ASSERT(process_a.domain().active_action() == realization::SyntheticDomain::Action::HoldPosition);

        std::cout << "[4] Step 3: Hardware Migration under RIT (Ship of Theseus: TPM Alpha -> Secure Enclave Beta)...\n";
        identity::MaterialAnchor enclave_anchor{
            .id = identity::MaterialAnchorId("secure-enclave-beta"),
            .type = identity::SubstrateType::SecureEnclave,
            .hardware_fingerprint = "fp-enclave-beta-attestation-002"
        };

        auto mig_res = process_a.migrate_hardware(enclave_anchor, 3);
        ENTE_TEST_ASSERT(mig_res.has_value());
        ENTE_TEST_ASSERT_EQ(process_a.identity().id, ente_id); // IDENTITY PRESERVED
        ENTE_TEST_ASSERT(process_a.material_bindings().active_binding().anchor.id == enclave_anchor.id);

        std::cout << "[5] Persisting full historical REC to disk and shutting down Process A...\n";
        auto save_res = process_a.history().save_to_file(test_file);
        ENTE_TEST_ASSERT(save_res.has_value());
    } // Process A terminates completely here

    // =========================================================================
    // PHASE 2: COLD RESTART IN PROCESS B, REPLAY, RATS ATTESTATION & CONSTITUTION
    // =========================================================================
    {
        std::cout << "\n[6] Process B starting: Cold Recovery from file...\n";
        auto recover_res = realization::EnteRealization::recover_from_file(test_file);
        ENTE_TEST_ASSERT(recover_res.has_value());

        auto& process_b = *recover_res;
        ENTE_TEST_ASSERT_EQ(process_b.identity().id, ente_id);
        ENTE_TEST_ASSERT(process_b.identity().lifecycle == identity::LifecycleStatus::LifeActive);
        ENTE_TEST_ASSERT(process_b.history().verify_integrity());
        ENTE_TEST_ASSERT(process_b.material_bindings().has_active_binding());
        ENTE_TEST_ASSERT(process_b.material_bindings().active_binding().anchor.id == identity::MaterialAnchorId("secure-enclave-beta"));
        ENTE_TEST_ASSERT_EQ(process_b.material_bindings().active_binding().anchor.hardware_fingerprint, "fp-enclave-beta-attestation-002");
        ENTE_TEST_ASSERT(process_b.material_bindings().active_binding().previous_anchor.has_value());
        ENTE_TEST_ASSERT(*process_b.material_bindings().active_binding().previous_anchor == identity::MaterialAnchorId("tpm-hardware-alpha"));

        std::cout << "[7] Verifying 2nd Genesis is strictly rejected...\n";
        auto gen2_res = process_b.genesis(ente_id);
        ENTE_TEST_ASSERT(!gen2_res.has_value());
        ENTE_TEST_ASSERT(gen2_res.error() == core::EnteError::GenesisAlreadyExists);

        std::cout << "[8] Independent RATS Attestation Evaluation...\n";
        attestation::IndependentAttestationVerifier attestation_verifier;

        // Construct Attestation Evidence for active hardware substrate
        std::string nonce = "nonce-test-vertical-0";
        std::string evidence_payload = std::format("{}:{}:{}:{}",
            "secure-enclave-beta",
            constitution_digest.value,
            nonce,
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
            .nonce = nonce,
            .attestation_signature = attestation_sig
        };

        auto attestation_result = attestation_verifier.evaluate(evidence, constitution_digest);
        ENTE_TEST_ASSERT(attestation_result.verdict == attestation::AppraisalVerdict::TrustworthyVerified);
        ENTE_TEST_ASSERT(attestation_result.satisfies_constitutional_floor);

        // Adversarial Replay / Missing Signature test:
        attestation::AttestationEvidence unsigned_evidence = evidence;
        unsigned_evidence.attestation_signature = core::Digest();
        auto unauth_res = attestation_verifier.evaluate(unsigned_evidence, constitution_digest);
        ENTE_TEST_ASSERT(unauth_res.verdict == attestation::AppraisalVerdict::StructurallyAcceptable);
        ENTE_TEST_ASSERT(!unauth_res.satisfies_constitutional_floor); // Must NOT satisfy floor without signature!

        std::cout << "[9] Full Constitutional Invariant Verification (C1..C14)...\n";
        auto rep = process_b.verify();
        ENTE_TEST_ASSERT(rep.is_valid());

        std::cout << "\n[SUCCESS] Vertical demonstration complete: The ENTE was born, perturbed, migrated across hardware,\n"
                  << "          persisted, recovered cold, attested independently, and maintained its continuous identity!\n";
    }

    std::filesystem::remove(test_file);
    return 0;
}

