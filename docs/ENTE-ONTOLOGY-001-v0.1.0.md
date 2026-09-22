# ENTE-ONTOLOGY-001 — Ontologia Operacional da Categoria ENTE

## v0.1.0 — design-candidate

```context-metadata+json
{
  "document": {
    "id": "ENTE-ONTOLOGY-001",
    "version": "0.1.0",
    "status": "design-candidate",
    "title": "Ontologia Operacional da Categoria ENTE",
    "date": "2026-09-22"
  },
  "depends_on": [
    "ENTE-CONSTITUTION-001-v0.6.0"
  ],
  "epistemic_scope": "operational-ontology",
  "implementation_status": "implemented-in-ente-0",
  "purpose": "Formalizar entidades, relações, estados, eventos, autoridades e invariantes necessários para materializar uma realização ENTE."
}
```

---

# 0. Propósito

Este documento transforma os compromissos da Constituição ENTE em uma ontologia operacional.

Seu objetivo não é implementar ENTE.

Seu objetivo é definir, de forma suficientemente precisa, **o que deve existir**, **como os elementos se relacionam**, **quais transições são válidas** e **quais estados são proibidos** para que uma implementação futura possa ser testada.

---

# 1. Princípio de modelagem

Cada entidade ou relação definida aqui deve corresponder a algo:
```text
OBSERVABLE
VERIFIABLE
TRACEABLE
```
ou explicitamente:
```text
UNKNOWN
UNRESOLVED
NOT_APPLICABLE
```

A ausência de evidência não pode ser convertida em validade.

---

# 2. Vocabulário de alto nível

Sete famílias principais:
1. `IDENTITY`
2. `TEMPORAL_LINEAGE`
3. `EPISTEMIC_STATE`
4. `CONSTITUTIVE_STATE`
5. `AUTHORITY`
6. `HISTORY`
7. `TRANSFORMATION`

---

# 3. Entidade: ENTE_REALIZATION

Campos mínimos:
```text
realization_id
genesis_id
constitution_ref
current_state_ref
lineage_ref
authority_epoch_ref
history_ref
constitutive_status
lifecycle_status
```

---

# 4. Entidade: IDENTITY

```text
IDENTITY ≠ CURRENT_STATE ≠ MATERIAL_COMPOSITION ≠ PROCESS_ID ≠ HOSTNAME
```

A identidade é sustentada por:
```text
GENESIS + CONSTITUTIVE_LINEAGE + RECOVERABLE_HISTORY + LEGITIMATE_AUTHORITY_LINEAGE
```

---

# 5. Entidade: GENESIS

Para cada `IDENTITY`:
```text
exactly_one GENESIS
```
Invariantes:
- `GENESIS_IMMUTABLE_FACT`
- `GENESIS_UNIQUE_PER_IDENTITY`
- `GENESIS_PRECEDES_ALL_LIFE_EVENTS`

---

# 6. Entidade: DEATH

```text
DEATH ≠ DEATH_CERTIFICATION
```
Após `DEATH_FINALIZED`:
```text
SAME_IDENTITY_SUCCESSION = FORBIDDEN
```

---

# 8. Entidade: CONSTITUTIVE_INVARIANT

Invariantes C1 a C14:
- C1: IDENTITY
- C2: CONTINUITY
- C3: OBSERVABILITY
- C4: PROVENANCE
- C5: EPISTEMIC_DISTINCTION
- C6: REVISION_CAPABILITY
- C7: COHERENCE_EVALUATION
- C8: UNKNOWN_REPRESENTABILITY
- C9: GENESIS_ANCHOR
- C10: TEMPORAL_INTEGRITY
- C11: LINEAGE_SINGULARITY
- C12: HISTORY_RECOVERABILITY
- C13: CONSTITUTIVE_FINALITY_SAFETY
- C14: CONSTITUTIVE_AUTHORITY_CONTINUITY

---

# 9 a 13. Modelo Epistemológico

- `OBSERVATION`: Evidência diretamente obtida (`OBSERVED`).
- `DERIVATION`: Resultado deduzido deterministicamente (`DERIVED`).
- `INFERENCE`: Conclusão inferida por modelo (`INFERRED`).
- `UNKNOWN`: Estado epistêmico explícito de ausência de conhecimento fundamentado.
- `INTERPRETATION`: Explicação operacional corrente e provisória.

---

# 20 a 25. Linhagem e Autoridade

- `LINEAGE`: Sequência legítima de estados desde GENESIS.
- `FORK`: Mais de uma continuação independente candidata.
- `SUCCESSION`: Decisão constitutiva sobre qual continuação preserva a identidade.
- `AUTHORITY_EPOCH`: Configuração autorizada para produzir decisões válidas.
- `AUTHORITY_TRANSITION`: Passagem legítima entre épocas sem dupla soberania.

---

# 29. Entidade: REC (Registro Evolutivo Constitutivo)

Ledger append-only com encadeamento criptográfico, grafo causal e referências a evidências.

---

# 44. Transições Proibidas

- `NO_GENESIS -> VERIFIED_IDENTITY`
- `DEATH_FINALIZED -> SAME_IDENTITY_LIFE_ACTIVE`
- `FORK_UNRESOLVED -> MULTIPLE_FINALIZED_SUCCESSORS`
- `AUTHORITY_INVALID -> CONSTITUTIVE_FINALITY`
- `UNKNOWN -> CERTAIN` sem evidência intermediária
- `RECONSTRUCTED_RECORD -> ORIGINAL_RECORD`
- `NEW_AUTHORITY -> ACTIVE` sem transição válida ou recovery pré-constituído

---

> **A Constituição define o que precisa permanecer verdadeiro.  
> A Ontologia define o que precisa existir para que isso possa ser demonstrado.**

**Sempre pronto. Sempre incompleto.**
