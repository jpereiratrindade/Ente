# ENTE-0 — Primeira Realização em C++26

> **Status: `IMPLEMENTED_CANDIDATE`**  
> *"Sempre pronto. Sempre incompleto."*

Este repositório contém **ENTE-0**, a primeira realização mínima candidata capaz de demonstrar em testes determinísticos controlados os compromissos ontológicos e epistêmicos da categoria constitutiva **ENTE** em C++26.

---

## 1. Hierarquia Normativa

```text
1. CONSTITUTION (v0.6.0)  -> Autoridade ontológica máxima; define o que ENTE é
2. ONTOLOGY (v0.1.0)      -> Vocabulário operacional, entidades e transições válidas
3. RCC (v0.1.0)           -> Dinâmica contínua de reconsideração de interpretações
4. SYSTEM (v0.1.0)        -> Desenho arquitetural em C++26
5. EXPERIMENT (v0.1.0)    -> Protocolo de demonstração e falsificação empírica
6. BOOTSTRAP (v0.1.0)     -> Roteiro executável de compilação e verificação
```

---

## 2. Invariantes Constitutivos Ativos (C1..C14)

- **C1 (IDENTITY)**: Identidade singular, distinguível e persistente.
- **C2 (CONTINUITY)**: Transições preservam relação contínua e verificável com o estado anterior.
- **C3 (OBSERVABILITY)**: Superfície observacional explícita.
- **C4 (PROVENANCE)**: Registro de origem e justificativa em toda transformação material.
- **C5 (EPISTEMIC_DISTINCTION)**: Tipagem explícita entre `Observed`, `Derived`, `Inferred`, `Unknown`, `Uncertain`, `Contradictory`.
- **C6 (REVISION_CAPABILITY)**: RCC permanentemente disponível para rever interpretações.
- **C7 (COHERENCE_EVALUATION)**: Avaliação dinâmica de coerência entre evidência e interpretação.
- **C8 (UNKNOWN_REPRESENTABILITY)**: `UNKNOWN` é um estado epistêmico de primeira classe (nunca coagido silenciosamente).
- **C9 (GENESIS_ANCHOR)**: Gênese única, irrevogável e ancorada por digest criptográfico.
- **C10 (TEMPORAL_INTEGRITY)**: Encadeamento temporal e grafo causal RIT no ledger (REC).
- **C11 (LINEAGE_SINGULARITY)**: Linhagem singular não bifurcada.
- **C12 (HISTORY_RECOVERABILITY)**: Reconstrução histórica da trajetória a partir da Gênese (HRE).
- **C13 (CONSTITUTIVE_FINALITY_SAFETY)**: Garantia de ramo único e integridade de finalidade local.
- **C14 (CONSTITUTIVE_AUTHORITY_CONTINUITY)**: Linhagem contínua e ininterrupta de épocas de autoridade legítima.

---

## 3. Compilação e Testes

### Pré-requisitos
- Compilador C++26 (`GCC 14+` ou `Clang 18+`)
- `CMake 3.25+`
- `Ninja`

### Build
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Executar a Suíte de Verificação
```bash
ctest --test-dir build --output-on-failure --verbose
```

### Executar o Experimento de Falsificação Diretamente
```bash
./build/test_experiment_001
```
