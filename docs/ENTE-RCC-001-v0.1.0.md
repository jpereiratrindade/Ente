# ENTE-RCC-001 — Reavaliação Contextual Contínua

## v0.1.0 — design-candidate

```context-metadata+json
{
  "document": {
    "id": "ENTE-RCC-001",
    "version": "0.1.0",
    "status": "design-candidate",
    "title": "Reavaliação Contextual Contínua",
    "date": "2026-09-22"
  },
  "depends_on": [
    "ENTE-CONSTITUTION-001-v0.6.0",
    "ENTE-ONTOLOGY-001-v0.1.0"
  ],
  "epistemic_scope": "operational-mechanism",
  "implementation_status": "implemented-in-ente-0",
  "purpose": "Especificar o contrato operacional da reavaliação contínua de interpretações sob nova evidência."
}
```

---

# 0. Propósito

A Reavaliação Contextual Contínua — RCC — é o mecanismo pelo qual uma realização ENTE evita tratar sua interpretação corrente como verdade definitiva.

RCC responde continuamente à pergunta:
> **A interpretação que sustenta minha ação atual ainda explica adequadamente o que estou observando?**

---

# 1. Princípio fundamental

```text
INTERPRETATION_CURRENT = best_supported_explanation_now ≠ final_truth
```

---

# 5. Saídas da RCC

Compatibilidade:
```text
SUPPORTED
WEAKENED
CONTRADICTORY
INSUFFICIENT_EVIDENCE
UNKNOWN
```

Ações Epistemológicas:
```text
KEEP
REOBSERVE
SEEK_EVIDENCE
WAIT
COMPARE
ASK
SUSPEND_ACTION
SUSPEND_JUDGMENT
REINTERPRET
```

---

# 16. Suspensão de Ação

Se `required_interpretation` passa de `SUPPORTED` para `WEAKENED`, `UNKNOWN` ou `CONTRADICTORY`, a ação que dela depende deve ser suspensa (`SUSPEND_ACTION`).

---

# 35. Contrato Mínimo RCC (R1..R10)

- R1: Interpretação corrente é explícita.
- R2: Nova evidência pode desafiá-la.
- R3: `UNKNOWN` é representável.
- R4: Contradição é representável.
- R5: Sustentação de ação pode ser enfraquecida.
- R6: Ações epistemológicas existem.
- R7: Reinterpretação preserva predecessor causal (RIT).
- R8: Revisões materiais entram na história recuperável.
- R9: Autoridade constitutiva permanece separada.
- R10: Reavaliação não exige treinamento de modelo online.

---

# 42. Métricas Principais

1. `UNJUSTIFIED_CONTINUATION_RATE`: taxa de ações mantidas quando a interpretação de suporte perdeu sustentação.
2. `UNNECESSARY_SUSPENSION_RATE`: taxa de ações suspensas diante de observações compatíveis/nominais.

---

> **RCC não exige conhecer antecipadamente o que surgiu.  
> Exige reconhecer quando aquilo que eu acreditava deixou de explicar adequadamente o que está acontecendo.**

**Sempre pronto. Sempre incompleto.**
