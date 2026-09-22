# ENTE-SYSTEM-001 — Construção Inicial do Sistema ENTE em C++26

## v0.1.0 — design-candidate

```context-metadata+json
{
  "document": {
    "id": "ENTE-SYSTEM-001",
    "version": "0.1.0",
    "status": "design-candidate",
    "title": "Construção Inicial do Sistema ENTE em C++26",
    "date": "2026-09-22"
  },
  "depends_on": [
    "ENTE-CONSTITUTION-001-v0.6.0",
    "ENTE-ONTOLOGY-001-v0.1.0",
    "ENTE-RCC-001-v0.1.0"
  ],
  "language": "C++26",
  "build_system": "CMake",
  "test_system": "CTest",
  "epistemic_scope": "system-design",
  "implementation_status": "implemented-in-ente-0",
  "purpose": "Definir a primeira arquitetura implementável de uma realização ENTE em C++26."
}
```

---

# 0. Declaração de construção

Este documento define o primeiro desenho de implementação de uma realização ENTE em C++26 (**ENTE-0**).

O objetivo é construir um sistema pequeno o bastante para ser compreendido integralmente e rigoroso o bastante para testar se os conceitos constitutivos produzem comportamento observável real.

---

# 1. Princípio arquitetural

> **ENTE funciona como um contrato constitutivo aplicado a uma realização concreta.**

O ENTE não se apropria do domínio nem impõe uma classe-base fechada.

---

# 4. Escopo da primeira implementação (ENTE-0)

Implementados no núcleo local:
- `GENESIS` & `IDENTITY`
- `MATERIAL_ANCHOR` & `MATERIAL_BINDING` (Navio de Teseu)
- `EVENT_SCOPED_PRNG` (Replay determinístico endereçado por evento)
- `OBSERVATION` & `INTERPRETATION`
- `UNKNOWN` & `CONTRADICTORY` explícitos
- `RCC` & `EPISTEMIC_ACTIONS`
- `RUNTIME_ASSURANCE` (Desacoplado da RCC)
- `RIT` & `REA`
- `REC` (Ledger local com hash-chain e grafo causal)
- `AUTHORITY_LINEAGE` (C14)
- `COLD_RECOVERY` & Persistência em disco
- `CONSTITUTION_VERIFIER` (Avaliação dinâmica de C1..C14)
- `ATTESTATION_RATS` (Modelo de atestação independente)

Adiados para marcos futuros:
- Consenso BFT distribuído em rede
- Épocas de autoridade entre múltiplos nós independentes
- Sensores físicos e hardware embarcado real
- Emendas constitucionais automáticas

---

# 5. Estrutura de namespaces

```cpp
namespace ente;
namespace ente::core;
namespace ente::identity;
namespace ente::history;
namespace ente::epistemic;
namespace ente::judgment;
namespace ente::rcc;
namespace ente::assurance;
namespace ente::attestation;
namespace ente::constitution;
namespace ente::realization;
```

---

> **O primeiro ENTE não precisa saber muito.  
> Precisa conseguir nascer, distinguir o que sabe do que não sabe, reconsiderar o que acredita e demonstrar historicamente como mudou sem deixar de ser ele mesmo.**

**Sempre pronto. Sempre incompleto.**
