# ENTE-0 — Primeira Realização em C++23 / C++26

> **Status: `ENTE-0 IMPLEMENTED_CANDIDATE` · `ENTE-1 LOCAL FACTUAL CORE` em desenvolvimento**
> *"Sempre pronto. Sempre incompleto."*

Este repositório contém **ENTE-0**, a primeira realização mínima candidata capaz de demonstrar em testes determinísticos controlados os compromissos ontológicos, epistêmicos e transacionais da categoria constitutiva **ENTE** em C++23 (compatível com C++26).

Para a análise formal dos limites de segurança, premissas de confiança e o que é garantido vs simulado, consulte o documento normativo:
📖 **[Modelo de Ameaças & Matriz de Garantias (THREAT_MODEL.md)](file:///home/jpereiratrindade/dev/cpp/Ente/docs/THREAT_MODEL.md)**

---

## 1. Hierarquia Normativa

```text
1. CONSTITUTION (v0.6.0)  -> Autoridade ontológica máxima; define o que ENTE é
2. THREAT MODEL (v1.0.0)  -> Delimitação de garantias, confiança e ameaças
3. ONTOLOGY (v0.1.0)      -> Vocabulário operacional, entidades e transições válidas
4. RCC (v0.1.0)           -> Dinâmica contínua de reconsideração de interpretações
5. SYSTEM (v0.1.0)        -> Desenho arquitetural em C++23/C++26
6. EXPERIMENT (v0.1.0)    -> Protocolo de demonstração e falsificação empírica
7. BOOTSTRAP (v0.1.0)     -> Roteiro executável de compilação e verificação
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
- **C11 (LINEAGE_SINGULARITY)**: Linhagem singular não bifurcada *(Mono-nó: localmente único)*.
- **C12 (HISTORY_RECOVERABILITY)**: Reconstrução histórica da trajetória a partir da Gênese (HRE).
- **C13 (CONSTITUTIVE_FINALITY_SAFETY)**: Garantia de ramo único e integridade de finalidade local.
- **C14 (CONSTITUTIVE_AUTHORITY_CONTINUITY)**: Linhagem contínua e ininterrupta de épocas de autoridade legítima.

---

## 3. Compilação e Testes

### Pré-requisitos
- Compilador C++23 (`GCC 13/14+` ou `Clang 17/18+`)
- `CMake 3.25+`
- `Ninja`

### Build
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Executar a Suíte de Verificação Completa (18 Testes)
```bash
ctest --test-dir build --output-on-failure --verbose
```

### Instalação da Biblioteca e Exportação CMake
```bash
cmake --install build --prefix /usr/local
```

### Executar a Demonstração da Vertical Unificada
```bash
./build/test_vertical_lifecycle
```

### Executar o Experimento de Falsificação e Baselines (EXP-001)
```bash
./build/test_experiment_001
```

### Executar a Bateria de Chaos & Injeção de Falhas (CHAOS-001..006)
```bash
./build/test_chaos_invariants
```

### Executar a Simulação Estocástica Controlada de Monte Carlo
```bash
./build/test_monte_carlo_benchmark
```

### Executar o Teste de Longevidade & Escalabilidade do REC (10k+ Eventos)
```bash
./build/test_rec_longevity
```

### Executar o Laboratório de Domínio 001: Veículo Autônomo / KitKat (ENTE-CASELAB-001)
```bash
./build/case_lab
```

### Executar o Laboratório de Domínio 002: Bomba de Infusão Clínica (ENTE-CASELAB-002)
```bash
./build/clinical_lab
```

### Executar a Aplicação ENTE Field Station & Observatory (ENTE-CASELAB-003)
```bash
./build/ente_field_station
```
*(Abra `examples/field-station/web/index.html` em seu navegador para explorar a interface visual e interativa)*

---

## 4. Documentação e Casos de Uso

- **Manual de Desenvolvimento & Integração de Domínios**: [`docs/DEVELOPER_GUIDE.md`](file:///home/jpereiratrindade/dev/cpp/Ente/docs/DEVELOPER_GUIDE.md) *(Novo!)*
- **Perfis de Conformidade e Claims Verificáveis**: [`docs/ENTE-CONFORMANCE-PROFILES.md`](file:///home/jpereiratrindade/dev/cpp/Ente/docs/ENTE-CONFORMANCE-PROFILES.md)
- **Website Interativo & Manifesto**: [`docs/index.html`](file:///home/jpereiratrindade/dev/cpp/Ente/docs/index.html) *(Deploy no GitHub Pages)*
- **Aplicação Visual ENTE Observatory (Field Station)**: [`examples/field-station/web/index.html`](file:///home/jpereiratrindade/dev/cpp/Ente/examples/field-station/web/index.html)
- **Relatório Formal de Demonstração & Falsificação**: [`docs/ENTE-FORMAL-REPORT-v0.1.0.md`](file:///home/jpereiratrindade/dev/cpp/Ente/docs/ENTE-FORMAL-REPORT-v0.1.0.md)
- **Normas Constitutivas (C1..C14)**: [`docs/ENTE-CONSTITUTION-001-v0.6.0.md`](file:///home/jpereiratrindade/dev/cpp/Ente/docs/ENTE-CONSTITUTION-001-v0.6.0.md)
- **Ontologia Operacional**: [`docs/ENTE-ONTOLOGY-001-v0.1.0.md`](file:///home/jpereiratrindade/dev/cpp/Ente/docs/ENTE-ONTOLOGY-001-v0.1.0.md)
- **Reconsideração Contínua de Contexto (RCC)**: [`docs/ENTE-RCC-001-v0.1.0.md`](file:///home/jpereiratrindade/dev/cpp/Ente/docs/ENTE-RCC-001-v0.1.0.md)
- **Arquitetura de Sistema em C++26**: [`docs/ENTE-SYSTEM-001-v0.1.0.md`](file:///home/jpereiratrindade/dev/cpp/Ente/docs/ENTE-SYSTEM-001-v0.1.0.md)
- **Protocolo Experimental (EXP-001)**: [`docs/ENTE-EXPERIMENT-001-v0.1.0.md`](file:///home/jpereiratrindade/dev/cpp/Ente/docs/ENTE-EXPERIMENT-001-v0.1.0.md)
- **Consumo de Domínio 001 (Laboratório KitKat)**: [`docs/ENTE-CASELAB-001-v0.1.0.md`](file:///home/jpereiratrindade/dev/cpp/Ente/docs/ENTE-CASELAB-001-v0.1.0.md)
- **Consumo de Domínio 002 (Laboratório Clínico UTI)**: [`docs/ENTE-CASELAB-002-v0.1.0.md`](file:///home/jpereiratrindade/dev/cpp/Ente/docs/ENTE-CASELAB-002-v0.1.0.md)
- **Consumo de Domínio 003 (Estação de Campo & Irrigação)**: [`docs/ENTE-CASELAB-003-v0.1.0.md`](file:///home/jpereiratrindade/dev/cpp/Ente/docs/ENTE-CASELAB-003-v0.1.0.md) *(Código em [`examples/field-station/`](file:///home/jpereiratrindade/dev/cpp/Ente/examples/field-station/))*

---

## 5. Licença

Este projeto é software livre licenciado sob os termos da **GNU General Public License v3.0 (GPLv3)**. Veja o arquivo [`LICENSE`](file:///home/jpereiratrindade/dev/cpp/Ente/LICENSE) para os termos completos.
