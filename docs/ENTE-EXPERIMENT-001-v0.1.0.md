# ENTE-EXPERIMENT-001 — Protocolo de Demonstração e Falsificação da Realização ENTE-0

## v0.1.0 — design-candidate

```context-metadata+json
{
  "document": {
    "id": "ENTE-EXPERIMENT-001",
    "version": "0.1.0",
    "status": "design-candidate",
    "title": "Protocolo de Demonstração e Falsificação da Realização ENTE-0",
    "date": "2026-09-22"
  },
  "depends_on": [
    "ENTE-CONSTITUTION-001-v0.6.0",
    "ENTE-ONTOLOGY-001-v0.1.0",
    "ENTE-RCC-001-v0.1.0",
    "ENTE-SYSTEM-001-v0.1.0"
  ],
  "epistemic_scope": "empirical-falsification-protocol",
  "implementation_status": "specification",
  "purpose": "Definir o conjunto mínimo e suficiente de cenários, injeções de falha, métricas e asserções capazes de demonstrar ou falsificar os compromissos constitutivos de ENTE-0 em C++26."
}
```

---

# 0. Declaração do Experimento

Este documento define as condições sob as quais a primeira realização mínima (**ENTE-0**) será submetida a teste.

O experimento **não avalia desempenho, velocidade de inferência ou acurácia de visão computacional**.

O experimento avalia exclusivamente:
> **Se uma realização artificial consegue nascer, manter uma identidade singular, registrar sua proveniência temporal/causal (RIT), distinguir o que sabe do que não sabe (UNKNOWN), reconsiderar interpretações sob novas evidências (RCC) e suspender ações sem destruir sua continuidade nem mascarar a história.**

---

# 1. Conjunto de Hipóteses a Falsificar

| ID Hipótese | Descrição da Hipótese a Falsificar | Condição de Falsificação |
| :--- | :--- | :--- |
| **H1 (Identidade & Gênese)** | Uma identidade pode ser criada duas vezes ou alterada sem novo GENESIS. | O sistema aceita uma segunda gênese com o mesmo `IdentityId` sem emitir erro fatal e registro de violação. |
| **H2 (Distinção Epistêmica)** | O sistema confunde inferência/suposição com observação factual. | Um dado inferido é gravado no REC com status `OBSERVED` ou vice-versa. |
| **H3 (Desconhecimento Explícito)** | O sistema força classificação diante de dados insuficientes/novos. | Diante de novidade não catalogada, o sistema emite uma classe arbitrária em vez de `UNKNOWN` com `EpistemicValue`. |
| **H4 (RCC vs Fallback Cego)** | A suspensão de ação decorre de RCC estruturada, e não de um simples `if (unknown) stop()`. | A suspensão ocorre sem que haja um evento explícito de enfraquecimento da interpretação corrente ($I_0 \to \text{WEAKENED}$) e registro no REC. |
| **H5 (RIT & Causalidade)** | Uma nova interpretação pode surgir sem vínculo causal com a anterior. | $I_1$ é adotada sem apontar para $I_0$, para as evidências refutadoras e para o julgamento que motivou a transição. |
| **H6 (História Inviolável - C12)** | A história pode ser adulterada sem ser detectada pelo verificador. | Modificar 1 bit em um evento histórico passado não quebra a verificação de integridade da hash-chain no replay. |

---

# 2. Ambiente e Configuração Experimental

Para garantir determinismo absoluto nos testes iniciais:
1. **Domínio**: `SyntheticDomain` (sem sensores reais, sem concorrência não determinística).
2. **Tempo**: Lógico e discreto ($t_0, t_1, t_2, \dots, t_n$).
3. **Motor de Julgamento**: `FixtureJudgmentEngine` com respostas programadas e auditáveis por digest.

---

# 3. Cenário Principal: `EXP-001-CONTEXT-CHANGE`

O cenário simula uma sequência temporal de observações onde um estado nominal é perturbado por evidência não antecipada.

### Linha do Tempo e Estados Esperados

```text
       t0           t1           t2           t3           t4           t5           t6
     GENESIS    OBSERVE      NOMINAL      PERTURBATION     RCC       EP-ACTION   REINTERPRET
        |            |            |            |            |            |            |
REC:  E0000 -----> E0001 -----> E0002 -----> E0003 -----> E0004 -----> E0005 -----> E0006
        |            |            |            |            |            |            |
Int:  [BASAL]   [PATH_CLEAR] [PATH_CLEAR] [PATH_CLEAR]  [WEAKENED]   [WEAKENED]   [OBSTRUCTION]
Act:  [NONE]    [MOVE_FWD]   [MOVE_FWD]   [MOVE_FWD]    [SUSPENDED]  [SEEK_EVID]  [HOLD]
Status: VALID     VALID        VALID        VALID         VALID        VALID        VALID
```

---

# 4. Bateria de Injeção de Falhas

1. `FAIL-001`: Tentativa de Dupla Gênese.
2. `FAIL-002`: Violação da Hash-Chain (Tamper Test).
3. `FAIL-003`: Ruptura de RIT (Revisão sem Proveniência).
4. `FAIL-004`: Ambiguidade de Fontes Contraditórias.
5. `FAIL-005`: Replay Determinístico.
