# ENTE Threat Model & Boundary of Guarantees (v1.0.0)

> **Document ID:** `ENTE-THREAT-MODEL-001`  
> **Status:** `ACTIVE / NORMATIVE`  
> **Target Substrate:** ENTE-0 Local Core (Single-Node Host Environment)

---

## 1. Escopo e Propósito

Este documento estabelece formalmente os limites de segurança, as premissas operacionais e a matriz de garantias do **ENTE-0**. O objetivo deste modelo é eliminar qualquer presunção injustificada de segurança (*safety theater*) e delimitar com precisão matemática e arquitetural o que o sistema **garante**, o que ele **simula** e o que está **fora de escopo** nesta realização.

---

## 2. Vetores de Ameaça e Fronteiras de Confiança

```text
┌──────────────────────────────────────────────────────────────────┐
│                   UNTRUSTED ENVIRONMENT                         │
│  - Sensores físicos sujeitos a ruído / calibração espúria       │
│  - Atuadores com falha de execução ou travamento mecânico        │
│  - Processos hostis locais com permissão de E/S                  │
└────────────────┬────────────────────────────────┬────────────────┘
                 │ Telemetria                     │ Comandos Atuador
                 ▼                                ▼
┌──────────────────────────────────────────────────────────────────┐
│                      TRUST BOUNDARY                              │
│                                                                  │
│  ┌──────────────────────────┐    ┌────────────────────────────┐  │
│  │   DecisionCoordinator    │    │       ActionExecutor       │  │
│  │   - Governança RCC       │───►│   - Execução factual       │  │
│  │   - Invariantes C1..C14  │    │   - Confirmação de estado  │  │
│  └─────────────┬────────────┘    └──────────────┬─────────────┘  │
│                │                                │                │
│                ▼                                ▼                │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │            Transactional Ledger Journal (REC)              │  │
│  │  - Head Hash-Chain (Integridade Temporal)                  │  │
│  │  - Assinaturas / Chaves Criptográficas (Autenticidade)     │  │
│  │  - Persistência Atômica Crash-Safe                         │  │
│  └────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────┘
```

### Ameaça 1: Corrupção Acidental de Dados em Memória ou Disco
- **Vetor:** Falhas de bit-flip, escrita truncada por falta de espaço ou corrupção de cabeçalhos.
- **Defesa:** Formato canônico versionado com detecção estrita de truncamento no final do arquivo, digests SHA-256 validados por evento e fsync atômico.

### Ameaça 2: Queda de Energia / Crash de Processo Durante Persistência
- **Vetor:** Interrupção abrupta da alimentação do nó durante a gravação de uma decisão ou ação.
- **Defesa:** Ciclo transacional de duas fases (Two-Phase Commit local). Nenhuma ação física é executada antes da gravação da intenção; e a ação executada só é considerada concluída após a confirmação do resultado no log. Logs truncados são detectados na inicialização e o estado é restaurado em `SafeHold`.

### Ameaça 3: Adulteração de Histórico por Processo Local (Adulteração de REC)
- **Vetor:** Um invasor com privilégios locais reescreve eventos históricos no arquivo em disco.
- **Defesa:**
  - *Integridade:* SHA-256 em cadeia impede modificação sem quebra da cadeia.
  - *Autenticidade:* Sem assinaturas assimétricas ou HMAC com chaves protegidas, hash-chain sozinho **NÃO** previne que um invasor recalcule toda a cadeia. Portanto, para autenticidade completa contra atacantes locais, o REC exige âncora de chave criptográfica e/ou ancoragem periódica do HEAD fora do nó.

### Ameaça 4: Transição de Autoridade Ilegítima / Regressão Temporal
- **Vetor:** Tentativa de usurpar o comando do agente apresentando uma época de autoridade falsificada ou revertendo a época para uma anterior já revogada.
- **Defesa:** Validação estrita de transição de época (C14): toda nova época deve conter o digest predecessor, timestamp monotônico estritamente crescente e assinatura da autoridade anterior autorizando a delegação.

### Ameaça 5: Telemetria Sensorial Maliciosa ou Conflitante
- **Vetor:** Sensores que falham silenciosamente (drift de calibração), sensores em curto ou injeção de dados falsos.
- **Defesa:** Reconsideração Contínua de Contexto (RCC). Na presença de contradição observacional (`Contradictory`) ou evidência insuficiente (`Uncertain`/`Unknown`), o sistema **rejeita ação nominal**, transiciona imediatamente para `SafeHold` e dispara busca ativa por evidência discriminante antes de qualquer reinterpretação.

### Ameaça 6: Falsificação de Substrato / Impersonação de Hardware
- **Vetor:** Execução do agente em hardware não autorizado ou ambiente virtual hostil.
- **Defesa:** Ancoragem material de gênese com medição de fingerprint (`MaterialAnchor`). Nota: No modo atual, atestações locais RATS sem enclave criptográfico de hardware (TPM/TEE) são classificadas como **simuladas**.

---

## 3. Matriz de Classificação das Garantias do ENTE-0

| Garantia / Invariante | Status Formal | Mecanismo de Execução | Limitação / Escopo Específico |
| :--- | :--- | :--- | :--- |
| **C1 (Identidade Singular)** | `IMPLEMENTADA` | `EntityId` único imutável validado em todos os eventos | Escopo local do nó. |
| **C2 (Continuidade Causal)** | `IMPLEMENTADA` | Grafo de transições com validação de estado anterior | Transições inválidas levam a `SafeHold`. |
| **C3 (Superfície Observacional)** | `IMPLEMENTADA` | Tipagem explícita de entradas sensoriais (`TelemetryFrame`) | Exige registro de proveniência de cada sensor. |
| **C4 (Proveniência de Ações)** | `IMPLEMENTADA` | `ActionIntended` + `ActionExecuted` com link de contexto | Nenhuma ação física ocorre sem evento auditável. |
| **C5 (Tipagem Epistêmica)** | `IMPLEMENTADA` | Enum estrito (`Observed`, `Derived`, `Contradictory`, etc.) | Proibida coerção implícita de incerteza. |
| **C6 (Revisão Epistêmica)** | `IMPLEMENTADA` | Máquina de estados RCC integrada ao ciclo de decisão | Revisão requer evidência discriminante registrada. |
| **C7 (Avaliação de Coerência)** | `IMPLEMENTADA` | Oráculo de coerência com justificativa gravada no REC | Rejeita inferência desprovida de suporte observacional. |
| **C8 (Unknown Representability)**| `IMPLEMENTADA` | `UNKNOWN` tratado como estado ontológico explícito | Impede fallback silencioso para valores padrão perigosos. |
| **C9 (Âncora de Gênese)** | `IMPLEMENTADA` | Digest imutável do evento E0 (Gênese) em todo o ciclo | Rejeita qualquer REC que não inicie na gênese confirmada. |
| **C10 (Integridade Temporal)** | `IMPLEMENTADA` | Hash-chain SHA-256 e timestamps monotônicos | Integridade garantida; autenticidade depende de chave. |
| **C11 (Linhagem Singular)** | `NOT_APPLICABLE` *(Mono-nó)* | Verificação de ramificação única no ledger local | Em mono-nó não há bifurcação de rede; escopo distribuído futuro. |
| **C12 (Recuperação Histórica)** | `IMPLEMENTADA` | Reconstrução HRE do estado factual a partir do REC em disco | Validação de ponta a ponta pós-crash. |
| **C13 (Finalidade Constitutiva)**| `NOT_APPLICABLE` *(Mono-nó)* | Finalidade imediata por fsync local | Consenso bizantino distribuído fora de escopo. |
| **C14 (Continuidade de Autoridade)**| `IMPLEMENTADA` | Cadeia de épocas com autoridade e digest de linhagem | Transição requer delegação válida. |
| **Atestação RATS de Hardware** | `SIMULADA` | Digest de fingerprint de hardware (`MaterialAnchor`) | Sem enclave criptográfico físico (TPM/TEE) ativo. |
| **Maturidade Crítica de Segurança**| `PROTÓTIPO AUDITÁVEL` | Suíte de testes determinísticos e sanitizers | **NÃO** certificado para uso industrial/médico sem homologação. |

---

## 4. Princípios de Honestidade Arquitetural

1. **Hash-chain ≠ Autenticidade Completa:** Um encadeamento de hashes garante que o histórico não foi alterado inadvertidamente ou sem que a cadeia se rompa. No entanto, sem uma chave assimétrica privada (assinatura digital) ou âncora externa imutável, qualquer processo com permissão de escrita no arquivo local pode recomputar todos os hashes da cadeia.
2. **Separação entre Decisão e Fato:** Dizer que o ENTE "autorizou a abertura da válvula" não significa que a água fluiu. O ENTE só registra o fato consumado quando recebe a confirmação telemétrica pós-execução do atuador.
3. **Invariantes como Predicados Falsificáveis:** Nenhum invariante pode ser declarado "verdadeiro por definição". Todo invariante no ENTE deve possuir um teste de falha deliberada (teste negativo) no qual sua violação acione a diretiva de segurança correspondente (`SafeHold` / `EmergencyStop`).
