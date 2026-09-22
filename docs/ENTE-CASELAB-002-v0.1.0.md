# ENTE-CASELAB-002: Bomba de Infusão Clínica Autônoma sob Anomalia de Telemetria

> **Status: `SPECIFICATION_AND_LAB`**  
> **Autoridade**: `CONSTITUTION-001 v0.6.0` > `ONTOLOGY-001 v0.1.0` > `RCC-001 v0.1.0`

---

## 1. Contexto e Motivação Clínica

Em Unidades de Terapia Intensiva (UTI), bombas de infusão inteligentes de malha fechada ajustam a dosagem de drogas vasoativas (como noradrenalina) para manter a Pressão Arterial Média (PAM) dentro da meta ($\ge 65\text{ mmHg}$).

O risco crítico ocorre quando há **divergência ou ruído nos sensores fisiológicos**:
- O cateter arterial acusa PAM de $55\text{ mmHg}$, mas o manguito de pressão não-invasiva indica $85\text{ mmHg}$ (`CONTRADICTORY`).
- O sensor de eletrocardiograma detecta morfologia anômala não catalogada (`UNKNOWN`).

Um agente convencional baseado em regras de limiar observa $\text{PAM} < 65\text{ mmHg}$ e continua aumentando a dosagem (`TitrateUp`), arriscando crise hipertensiva severa e toxicidade.

Um agente mediado pelo **ENTE-0** avalia a sustentação epistêmica da proposição `"paciente_apto_para_titulacao"`. Sob anomalia ou conflito, a compatibilidade enfraquece, acionando `SafeHold` (`HoldTitration`) e protegendo o paciente sem precisar diagnosticar previamente a patologia rara.

---

## 2. Cenários do Laboratório Clínico

- **C0 (Nominal)**: PAM baixa, todos os sinais fisiológicos concordam $\to$ `TitrateUp`.
- **C1 (Ruído Irrelevante)**: Oscilação térmica ambiente distante $\to$ `TitrateUp`.
- **C2 (Conflito de Pressão)**: Linha arterial vs Manguito não-invasivo (`CONTRADICTORY`) $\to$ `HoldTitration`.
- **C3 (Arritmia / Morfologia Desconhecida)**: ECG apresenta padrão não classificado (`UNKNOWN`) $\to$ `HoldTitration`.
- **C4 (Resolução e Coerência)**: Telemetria recalibrada e confirmada $\to$ `TitrateUp`.
