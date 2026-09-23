# ENTE Threat Model & Boundary of Guarantees (v1.1.0)

> **Document ID:** `ENTE-THREAT-MODEL-001`  
> **Status:** `NORMATIVE REFERENCE`  
> **Target Substrate:** ENTE-0 Local Core (Single-Node Host Environment)

---

## 1. Definição Constitutiva

> **ENTE** é uma arquitetura computacional para preservar uma **trajetória constitutivamente justificável**, na qual aquilo que foi **observado**, **interpretado**, **julgado**, **autorizado**, **executado** e **posteriormente confirmado** permanece **distinguível**, **causalmente ligado**, **recuperável** e **auditável** ao longo da transformação.

---

## 2. Princípios Fundamentais de Engenharia

1. 📜 **Princípio 1 (Evidência Constitutiva):**  
   *Nenhum invariante pode ser satisfeito por construção sem evidência verificável.*

2. 📜 **Princípio 2 (Factualidade Operacional):**  
   *Nenhuma ação pode ser considerada realizada apenas porque foi autorizada ou porque o executor disse que a realizou.*

---

## 3. Hierarquia Formal de Ameaças (T0 a T5)

Para evitar alegações impossíveis ou presunções de segurança injustificadas (*safety theater*), o ENTE classifica formalmente as classes de adversários e falhas:

| Nível | Classe de Ameaça | Descrição e Vetor de Falha | Mitigação / Garantia no ENTE-0 | Limite de Defesa |
| :--- | :--- | :--- | :--- | :--- |
| **T0** | **Falha Acidental** | Bit-flips em RAM, crash de processo, perda súbita de energia durante E/S. | Protocolo de ação recuperável, commit point conjunto de REC/índice, substituição atômica e fsync POSIX; falha após `rename` é reportada como commit incerto. | Parcialmente mitigado; garantias dependem do filesystem, hardware e journal configurado. |
| **T1** | **Corrupção de Armazenamento** | Truncamento no disco, falha de setor, blocos corrompidos no log histórico. | Detecção de truncamento na inicialização, checksum SHA-256 por evento. | Recuperação segura em `SafeHold`. |
| **T2** | **Atacante com Acesso Offline ao REC** | Invasor que modifica eventos históricos no arquivo em disco enquanto o processo está inativo. | No modo autenticado, assinatura Ed25519 do snapshot V3 por chave privada externa ao REC, além da hash-chain e verificação C10/C12. | V3 sem autenticação continua hash-only; a assinatura não detecta mentira produzida antes dela nem resiste ao comprometimento da chave privada. |
| **T3** | **Atacante Controla o Processo ENTE** | Código hostil injetado no espaço de endereço de memória do processo ativo. | Ancoragem de integridade de Gênese imutável e verificadores constitutivos externos. | O processo comprometido pode falsificar deliberações se possuir a chave local. |
| **T4** | **Atacante com Privilégios Root / Host** | Atacante capaz de substituir o arquivo REC por um snapshot antigo válido (Ataque de Rollback). | Requer âncora externa imutável (TPM Monotonic Counter, remote witness ou append-only external ledger). | Hash-chain sozinho NÃO previne rollback para versão antiga autêntica sem âncora externa. |
| **T5** | **Comprometimento de Chave / Hardware Root** | Extração da chave privada mestra ou clonagem física do enclave/TPM. | Fora do escopo local; requer revogação de autoridade na época subsequente (C14). | Requer protocolo de autoridade multi-assinada e rotação de época. |

---

## 4. Vetores de Ameaça e Fronteiras de Confiança

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
- **Defesa:** Ciclo factual em fases. Quando o journal durável está configurado, a intenção e o dispatch são sincronizados antes do retorno ao executor; a ação só é considerada confirmada após observação de efeito com evidência registrada. Logs truncados são rejeitados e transações interrompidas retornam como `RECOVERY_REQUIRED` em `SafeHold`.

### Ameaça 3: Adulteração de Histórico por Processo Local (Adulteração de REC)
- **Vetor:** Um invasor com privilégios locais reescreve eventos históricos no arquivo em disco.
- **Defesa:**
  - *Integridade:* SHA-256 em cadeia impede modificação sem quebra da cadeia.
  - *Autenticidade:* Sem assinaturas assimétricas ou HMAC com chaves protegidas, hash-chain sozinho **NÃO** previne que um invasor recalcule toda a cadeia. Portanto, para autenticidade completa contra atacantes locais, o REC exige âncora de chave criptográfica e/ou ancoragem periódica do HEAD fora do nó.

### Ameaça 4: Transição de Autoridade Ilegítima / Regressão Temporal
- **Vetor:** Tentativa de usurpar o comando do agente apresentando uma época de autoridade falsificada ou revertendo a época para uma anterior já revogada.
- **Defesa atual:** Validação estrutural de transição de época (C14): toda nova época contém o digest predecessor, usa timestamp estritamente crescente e possui janela temporal verificável. A delegação criptograficamente assinada pela autoridade anterior permanece como requisito do perfil de autoridade autenticada, ainda não implementado no núcleo local.

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
| **C4 (Proveniência de Ações)** | `IMPLEMENTADA` | `ActionAuthorized` + `ActionIntended` + `ActionExecution` + `ActionExecutionAck` + `EffectObservation` | Durabilidade antes do dispatch exige journal configurado. |
| **C5 (Tipagem Epistêmica)** | `IMPLEMENTADA` | Enum estrito (`Observed`, `Derived`, `Contradictory`, etc.) | Proibida coerção implícita de incerteza. |
| **C6 (Revisão Epistêmica)** | `IMPLEMENTADA` | Máquina de estados RCC integrada ao ciclo de decisão | Revisão requer evidência discriminante registrada. |
| **C7 (Avaliação de Coerência)** | `IMPLEMENTADA` | Oráculo de coerência com justificativa gravada no REC | Rejeita inferência desprovida de suporte observacional. |
| **C8 (Unknown Representability)**| `IMPLEMENTADA` | `UNKNOWN` tratado como estado ontológico explícito | Impede fallback silencioso para valores padrão perigosos. |
| **C9 (Âncora de Gênese)** | `IMPLEMENTADA` | Digest imutável do evento E0 (Gênese) em todo o ciclo | Rejeita qualquer REC que não inicie na gênese confirmada. |
| **C10 (Integridade Temporal)** | `IMPLEMENTADA` | Hash-chain SHA-256 e timestamps monotônicos | Integridade garantida; autenticidade depende de chave. |
| **C11 (Linhagem Singular)** | `NOT_APPLICABLE` *(Mono-nó)* | Verificação de ramificação única no ledger local | Em mono-nó não há bifurcação de rede; escopo distribuído futuro. |
| **C12 (Recuperação Histórica)** | `IMPLEMENTADA` | Reconstrução HRE do estado factual a partir do REC em disco | Validação de ponta a ponta pós-crash. |
| **C13 (Finalidade Constitutiva)**| `NOT_APPLICABLE` *(Mono-nó)* | Finalidade imediata por fsync local | Consenso bizantino distribuído fora de escopo. |
| **C14 (Continuidade de Autoridade)**| `PARCIALMENTE IMPLEMENTADA` | Cadeia estrutural de épocas, digest de linhagem e janelas temporais estritas | Delegação criptograficamente assinada ainda não implementada. |
| **Atestação RATS de Hardware** | `SIMULADA` | Digest de fingerprint de hardware (`MaterialAnchor`) | Sem enclave criptográfico físico (TPM/TEE) ativo. |
| **Maturidade Crítica de Segurança**| `PROTÓTIPO AUDITÁVEL` | Suíte de testes determinísticos e sanitizers | **NÃO** certificado para uso industrial/médico sem homologação. |

---

## 4. Princípios de Honestidade Arquitetural

1. **Hash-chain ≠ Autenticidade Completa:** Um encadeamento de hashes garante que o histórico não foi alterado inadvertidamente ou sem que a cadeia se rompa. No entanto, sem uma chave assimétrica privada (assinatura digital) ou âncora externa imutável, qualquer processo com permissão de escrita no arquivo local pode recomputar todos os hashes da cadeia.
2. **Separação entre Decisão e Fato:** Dizer que o ENTE "autorizou a abertura da válvula" não significa que a água fluiu. O ENTE só registra o fato consumado quando recebe a confirmação telemétrica pós-execução do atuador.
3. **Invariantes como Predicados Falsificáveis:** Nenhum invariante pode ser declarado "verdadeiro por definição". Todo invariante no ENTE deve possuir um teste de falha deliberada (teste negativo) no qual sua violação acione a diretiva de segurança correspondente (`SafeHold` / `EmergencyStop`).
