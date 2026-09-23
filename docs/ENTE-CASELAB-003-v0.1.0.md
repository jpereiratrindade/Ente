# ENTE-CASELAB-003: Field Station — Resolução Epistêmica Ativa e Continuidade Ontológica
**Versão: v0.1.0 | Status: IMPLEMENTED_CANDIDATE | Data: 2026-09-22 | Licença: GPLv3**

---

## 1. Contexto e Problema de Domínio

O **Case Lab 003 (Field Station & Observatory)** modela uma estação agrícola autônoma de irrigação de precisão governada pela arquitetura constitutiva **ENTE-0** através da interface pública `GenericAgentWithEnte<FieldStationDomain>` ([`field_domain.hpp`](../examples/field-station/field_domain.hpp)).

A estação responde à pergunta operacional:
> **“É seguro e justificado irrigar este setor agora?”**

---

## 2. O Ciclo Cognitivo Completo do RCC

O **SafeHold** (*Válvula Fechada*) é a resposta de segurança imediata diante de incerteza ontológica, mas **não encerra o ciclo epistêmico**. O ENTE sustenta a busca ativa de evidências e a posterior retomada da ação fundamentada:

```text
       ┌──────────┐
       │   AGIR   │ ──▶ 1. Operação Nominal (Sondas A & B = Solo Seco 21% -> Válvula Aberta)
       └────┬─────┘
            │
            ▼
       ┌──────────┐
       │ DUVIDAR  │ ──▶ 2. Injeção de Anomalia (Sonda A = 20% Seco vs Sonda B = 85% Úmido)
       └────┬─────┘
            │
            ▼
       ┌──────────┐
       │  PARAR   │ ──▶ 3. SafeHold Imediato (RCC Enfraquecido -> Válvula Fechada)
       └────┬─────┘         "DISCRIMINATING_EVIDENCE_REQUIRED"
            │
            ▼
       ┌──────────┐
       │INVESTIGAR│ ──▶ 4. Busca Ativa no Domínio (Sonda de Referência C + Auto-teste de Impedância)
       └────┬─────┘
            │
            ▼
       ┌──────────┐
       │REFORMULAR│ ──▶ 5. Reinterpretação Fundamentada (Isolamento de Sonda com Drift)
       └────┬─────┘
            │
            ▼
       ┌──────────┐
       │ RE-AGIR  │ ──▶ 6. Retomada da Ação Justificada (ou Manutenção de Hold com Certeza)
       └──────────┘
```

---

## 3. Dois Tours Demonstrativos Especializados

### Tour A: Resolução Epistêmica Ativa (*Epistemic Resolution Tour*)
1. **Gênese**: Nascimento da identidade `ente-field-001` no hardware `RP-A921`.
2. **Nominal**: Sondas concordam em solo seco $\rightarrow$ RCC Suportado $\rightarrow$ Válvula Aberta.
3. **Conflito de Sensores**: Contradição A vs B $\rightarrow$ SafeHold imediato $\rightarrow$ ENTE emite demanda de evidência discriminante.
4. **Investigação Diagnóstica**: Sonda de Referência C e auto-testes determinam a verdade de solo e isolam a sonda defeituosa.
5. **Reinterpretação & Resolução**:
   - *Desfecho 1 (Solo Seco Confirmado)*: Sonda C = 22% Seco + Sonda B com drift $\rightarrow$ Válvula **ABERTA** (irrigação retomada).
   - *Desfecho 2 (Solo Úmido Confirmado)*: Sonda C = 82% Úmido + Sonda A com drift $\rightarrow$ Válvula **FECHADA** (hold fundamentado na certeza de que o solo está úmido).

### Tour B: Continuidade Ontológica sob Contingência Material (*Ontological Continuity Tour*)
1. **Gênese & SafeHold**: Conflito registrado e comitado no REC ledger.
2. **Migração Material (RIT / Navio de Teseu)**: Migração de `RP-A921` (Pi 4) para `RP-B104` (Pi 5) preservando a linhagem e o estado de SafeHold.
3. **Power Loss & Cold Recovery**: Blecaute do processo $\rightarrow$ novo processo reconstrói 100% da trajetória do REC e mantém a integridade do SafeHold.

---

## 4. Executáveis e Visualização

- **Binário C++26**: [`build/ente_field_station`](../build/ente_field_station)
- **Interface Web Interativa (Observatory)**: [`examples/field-station/web/index.html`](../examples/field-station/web/index.html)
- **Suíte de Testes Automatizada**: Integrado via CTest (`test_field_station`).
