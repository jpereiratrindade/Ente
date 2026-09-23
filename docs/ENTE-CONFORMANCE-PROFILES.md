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
| Payload transacional canônico | `VERIFIED` | Round-trip com delimitadores e newline | Não é ainda um schema externo padronizado |
| Intenção durável antes do dispatch | `PARTIALLY_VERIFIED` | Journal opcional persiste cada fase antes do retorno; falhas injetadas e encerramento abrupto de processo nos limites de fsync/rename/fsync do diretório | A garantia exige journal configurado; power loss físico e semântica do hardware/filesystem ainda não foram ensaiados |
| REC autenticado | `OUT_OF_SCOPE` na versão atual | Nenhuma assinatura assimétrica por evento | Hash-chain fornece integridade, não autenticidade |
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

## 5. Próximo gate

O próximo gate para `ENTE-1 VERIFIED LOCAL CORE` é executar power loss físico
em hardware e filesystem declarados. A implementação agora cobre falhas
determinísticas e encerramento abrupto do processo nas operações de fsync,
rename e fsync do diretório, além da matriz de restart das fases factuais, mas
não reivindica durabilidade diante de perda real de energia.
