# ENTE Conformance Profiles

**Documento:** `ENTE-CONFORMANCE-001`  
**Status:** Normative Draft  
**Perfil em desenvolvimento:** `ENTE-1 LOCAL FACTUAL CORE`

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

## 3. Perfil ENTE-LOCAL

O perfil local é monoprocesso e mono-nó. Ele não exige consenso distribuído.

| Claim | Estado atual | Evidência | Limite explícito |
|---|---|---|---|
| Arquitetura modular local | `VERIFIED` | Build e suíte na matriz GCC/Clang suportada | Não avalia integração certificada nem outros sistemas operacionais |
| Distinção autorização/execução/efeito | `VERIFIED` | `test_action_transaction` | Confirmação depende da qualidade da telemetria fornecida |
| Recuperação de ação interrompida | `VERIFIED` | Crash POSIX após `PREPARED`, `DISPATCHED`, `ACKNOWLEDGED` e `EFFECT_UNCONFIRMED` retorna como `RECOVERY_REQUIRED`; `CONFIRMED` e `FAILED` permanecem terminais | REC precisa ter sido persistido; encerramento de processo não equivale a power loss físico |
| Biografia canônica tipada | `VERIFIED` | REC V4 usa envelope e payloads length-prefixed; o mesmo material canônico alimenta hash, persistência, assinatura e recovery; tipos incompatíveis com `EventKind` são rejeitados | Não é ainda um schema externo padronizado |
| Atomicidade constitutiva | `VERIFIED` | Gênese, migração material, decisão, revisão epistêmica e transição de autoridade usam candidato isolado e testes de falha pré/pós commit | `PersistenceCommitUncertain` exige reconciliação por recovery |
| Constituição factual local | `VERIFIED` | C1–C10, C12 e C14 verificam evidências e sequências registradas; ataques C4, C6/C7, C10/C12 e C14 são falsificados pela suíte | C11/C13 são `NOT_APPLICABLE` no perfil mono-nó |
| Autoridade autenticada C14 | `VERIFIED` | Delegação Ed25519 pela chave da época anterior, chave pública por época, janela temporal estrita, rejeição de replay e recovery da linhagem | Perfil atual usa delegação simples; quorum/multisig e revogação externa não são reivindicados |
| Intenção durável antes do dispatch | `PARTIALLY_VERIFIED` | Commit point mantém REC/índice inalterados em falha anterior ao `rename`; dispatch não executa o domínio sem registro durável; falha posterior ao `rename` retorna `PersistenceCommitUncertain` | A garantia exige journal configurado; power loss físico e semântica do hardware/filesystem ainda não foram ensaiados |
| REC autenticado | `VERIFIED` no modo autenticado | Snapshot V4 canônico autenticado por Ed25519 e validado por chave pública externa; vetor RFC 8032; cadeia recalculada por atacante é rejeitada; recovery mantém assinatura | O modo V4 não autenticado permanece hash-only e não satisfaz este claim; comprometimento da chave privada permanece fora da garantia |
| Atestação de hardware | `SIMULATED` | Fixture RATS local | Sem TPM/TEE real |
| C11 — linhagem distribuída | `NOT_APPLICABLE` | Perfil mono-nó | Obrigatório apenas em `ENTE-DISTRIBUTED` |
| C13 — finalidade distribuída | `NOT_APPLICABLE` | Perfil mono-nó | Obrigatório apenas em `ENTE-DISTRIBUTED` |
| Certificação médica/automotiva | `OUT_OF_SCOPE` | Nenhuma | Depende do produto e processo completos |

## 4. Regras de desenvolvimento

1. **Claim before code:** toda funcionalidade nasce com propriedade, modelo de falha e teste de refutação.
2. **Evidence before status:** presença de classe ou função não autoriza `VERIFIED` ou `TRUSTED`.
3. **One source of truth:** REC, `DecisionTrace`, estado operacional e Observatory derivam da mesma transição factual.
4. **No silent promotion:** recovery nunca promove `DISPATCHED`, `ACKNOWLEDGED` ou `EFFECT_UNCONFIRMED` para `CONFIRMED`.
5. **Profile applicability:** `NOT_APPLICABLE` não reduz conformidade quando justificado pelo perfil.
6. **Explicit commit point:** erro anterior ao `rename` não avança REC nem índice em memória; erro de sincronização posterior ao `rename` é estado de commit incerto, nunca rollback presumido.

## 5. Próximo gate

| Gate | Estado |
|---|---|
| G0 — Constitutive Atomicity | `VERIFIED` |
| G1 — Genesis-Bound Identity | `VERIFIED` |
| G2 — Canonical Biography | `VERIFIED` |
| G3 — Evidence-Based Constitution | `VERIFIED` para invariantes aplicáveis ao perfil local |
| G4 — Authenticated Authority | `VERIFIED` para delegação Ed25519 estrita de uma autoridade predecessora |
| G5 — Supported Toolchain Green | `LOCAL PASS`; exige confirmação do workflow remoto após publicação |
| G6 — Declared-Substrate Durability | `PENDING PHYSICAL TEST` |

O gate externo restante para o marco `ENTE-1 VERIFIED LOCAL CORE` é executar e
registrar perda física de energia no hardware, kernel e filesystem declarados.
A implementação cobre fault injection determinística, `fork/_exit`, `fsync`,
`rename`, `fsync(dir)` e recovery das fases factuais; isso não é apresentado
como substituto de ensaio elétrico no substrato real.

O perfil local atual assume biografia experimental limitada. Journal append-only,
checkpoints e anti-rollback externo pertencem respectivamente ao perfil local de
longa duração e a um modelo de ameaça com host privilegiado.
