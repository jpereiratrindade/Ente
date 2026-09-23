# ENTE Conformance Profiles

**Documento:** `ENTE-CONFORMANCE-001`  
**Status:** Normative Specification  
**Perfil consolidado:** `ENTE-1 LOCAL CORE — SEMANTICALLY VERIFIED CANDIDATE`

## 1. Regra de maturidade

A maturidade de uma realização ENTE não é uma nota global. Ela é o conjunto de
claims demonstrados dentro de um escopo, sob um modelo de ameaça e para uma
realização identificada.

```text
ENTE maturity = claims demonstrados
                + scope declarado
                + threat model declarado
                + realization identificada
```

Os estados normativos são:

- `VERIFIED`: a propriedade possui critério de aceite e teste de falsificação aprovado.
- `PARTIALLY_VERIFIED`: parte da propriedade foi demonstrada e os limites estão declarados.
- `IMPLEMENTED_CANDIDATE`: implementação completa disponível para validação e fechamento semântico.
- `IMPLEMENTED_UNVERIFIED`: existe implementação, mas a evidência ainda é insuficiente.
- `SIMULATED`: o mecanismo representa uma integração externa que não está presente.
- `NOT_APPLICABLE`: o perfil exclui justificadamente a propriedade.
- `OUT_OF_SCOPE`: a realização não reivindica a propriedade.

## 2. Princípios factuais

```text
signed          != true
authorized      != dispatched
dispatched      != executed
executed        != effect observed
effect observed != causal certainty
```

Nenhum estado parcial pode ser confundido com estado confirmado. A máquina de
ação usa os estados `AUTHORIZED`, `PREPARED`, `DISPATCHED`, `ACKNOWLEDGED`,
`EFFECT_UNCONFIRMED`, `CONFIRMED`, `FAILED` e `RECOVERY_REQUIRED`.

Também vigora a estrita separação criptográfica e ontológica de raízes:

```text
REC snapshot authentication key
≠
constitutive authority key
≠
hardware attestation key
≠
external anti-rollback root
```

## 3. Perfil ENTE-LOCAL

O perfil local é monoprocesso e mono-nó. Ele não exige consenso distribuído.

| Claim | Estado atual | Evidência | Limite explícito |
|---|---|---|---|
| Arquitetura modular local | `VERIFIED` | Build e suíte no ambiente primário suportado (GCC 14 / Clang 18 libc++ Linux x86_64) | Não avalia integração de outros sistemas operacionais |
| Distinção autorização/execução/efeito | `VERIFIED` | `test_action_transaction` | Confirmação depende da qualidade da telemetria fornecida |
| Recuperação de ação interrompida | `VERIFIED` | Crash POSIX após `PREPARED`, `DISPATCHED`, `ACKNOWLEDGED` e `EFFECT_UNCONFIRMED` retorna como `RECOVERY_REQUIRED`; `CONFIRMED` e `FAILED` permanecem terminais | REC precisa ter sido persistido; encerramento de processo não equivale a power loss físico |
| Biografia canônica tipada | `VERIFIED` | REC V4 usa envelope e payloads length-prefixed; o mesmo material canônico alimenta hash, persistência, assinatura e recovery; tipos incompatíveis com `EventKind` são rejeitados | Bounded experimental biography; journal append-only segmentado pertence ao perfil de longa duração |
| Atomicidade constitutiva | `VERIFIED` | Gênese, migração material, decisão, revisão epistêmica e transição de autoridade usam candidato isolado e testes de falha pré/pós commit | `PersistenceCommitUncertain` exige reconciliação por recovery |
| Constituição factual local | `VERIFIED` | C1–C10, C12 e C14 verificam evidências e sequências registradas; ataques C4, C6/C7, C10/C12 e C14 são falsificados pela suíte | C11/C13 são `NOT_APPLICABLE` no perfil mono-nó |
| Autoridade autenticada C14 | `VERIFIED` | Delegação Ed25519 pela chave da época anterior, chave pública por época, janela temporal estrita, rejeição de replay e recovery da linhagem | Perfil atual usa delegação simples; quorum/multisig e revogação externa não são reivindicados |
| Intenção durável antes do dispatch | `VERIFIED` | Commit point mantém REC/índice inalterados em falha anterior ao `rename`; dispatch não executa o domínio sem registro durável; falha posterior ao `rename` retorna `PersistenceCommitUncertain` | A garantia exige journal configurado; ensaio em substrato declarado Linux/Btrfs documentado em G6 |
| REC autenticado | `VERIFIED` no modo autenticado | Snapshot V4 canônico autenticado por Ed25519 e validado por chave pública externa; vetor RFC 8032; cadeia recalculada por atacante é rejeitada; recovery mantém assinatura | O modo V4 não autenticado permanece hash-only e não satisfaz este claim; comprometimento da chave privada permanece fora da garantia |
| Atestação de hardware | `SIMULATED` | Fixture RATS local | Sem TPM/TEE real |
| C11 — linhagem distribuída | `NOT_APPLICABLE` | Perfil mono-nó | Obrigatório apenas em `ENTE-DISTRIBUTED` |
| C13 — finalidade distribuída | `NOT_APPLICABLE` | Perfil mono-nó | Obrigatório apenas em `ENTE-DISTRIBUTED` |
| Certificação médica/automotiva | `OUT_OF_SCOPE` | Nenhuma | Depende do produto e processo completos |

## 4. Regras de desenvolvimento

1. **Claim before code:** toda funcionalidade nasce com propriedade, modelo de falha e teste de refutação.
2. **Evidence before status:** presença de classe ou função não autoriza `VERIFIED` ou `TRUSTED`.
3. **One source of truth:** REC, `DecisionTrace`, estado operacional e projeções visuais derivam da mesma transição factual.
4. **No silent promotion:** recovery nunca promove `DISPATCHED`, `ACKNOWLEDGED` ou `EFFECT_UNCONFIRMED` para `CONFIRMED`.
5. **Profile applicability:** `NOT_APPLICABLE` não reduz conformidade quando justificado pelo perfil.
6. **Explicit commit point:** erro anterior ao `rename` não avança REC nem índice em memória; erro de sincronização posterior ao `rename` é estado de commit incerto, nunca rollback presumido.
7. **Observatory surface boundary:** no perfil local, o Observatory é classificado como superfície demonstrativa/pedagógica; qualquer auditoria factual deve consultar diretamente o REC exportado.

## 5. Matriz de Aceite Concluída: `ENTE-1 LOCAL CORE`

O programa de **Semantic Integrity Closure** concluiu os Acceptance Gates no escopo declarado:

| Gate | Propriedade | Estado |
|---|---|---|
| **G0 — Constitutive Atomicity** | Transações de estado isoladas por candidatos em gênese, migração, decisão, revisão e autoridade | `VERIFIED` |
| **G1 — Genesis-Bound Identity** | REC impõe `Genesis(X)` único inicial e `event.identity == X` invariante em todos os eventos | `VERIFIED` |
| **G2 — Canonical Biography** | Serialização canônica length-prefixed unificada para hash, assinatura, persistência e recovery | `VERIFIED` |
| **G3 — Evidence-Based Constitution** | C1–C10, C12 e C14 comprovam proveniência factual e encadeamento causal sem parsing textual | `VERIFIED` |
| **G4 — Authenticated Authority** | Delegação Ed25519 pela chave da época anterior, janela temporal e rejeição de replays | `VERIFIED` |
| **G5 — Supported Toolchain & Packaging** | Suíte completa (22/22) na matriz GCC 14 / Clang 18 libc++ e teste real de consumidor CMake instalado | `VERIFIED` |
| **G6 — POSIX Substrate Durability** | Morte abrupta (`_exit`), detecção de torn writes e barreiras POSIX em Linux Btrfs (`docs/ENTE-SUBSTRATE-DURABILITY-G6-REPORT.md`) | `VERIFIED` *(Substrato Declarado)* |
| **Physical Power-Loss Test** | Corte elétrico físico em hardware e armazenamento real | `PENDING PHYSICAL TEST` *(Fora do escopo POSIX local)* |

Todos os gates locais foram consolidados mediante o quinteto:
```text
especificação + teste positivo + teste de falsificação/negativo + teste de recovery + escopo documentado
```
