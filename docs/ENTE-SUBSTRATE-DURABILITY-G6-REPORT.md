# ENTE-1 G6 — Relatório de Durabilidade em Substrato Declarado

**Documento:** `ENTE-DURABILITY-G6-001`  
**Versão:** v1.0.0  
**Perfil:** `ENTE-1 LOCAL FACTUAL CORE`  
**Data:** 2026-09-23  
**Status:** `VERIFIED ON DECLARED SUBSTRATE`

---

## 1. Substrato Declarado de Ensaio

O Gate **G6 (Declared-Substrate Durability)** avalia a composição física e lógica entre software, sistema operacional, sistema de arquivos e dispositivo de armazenamento.

| Camada | Substrato Declarado |
| :--- | :--- |
| **Sistema Operacional** | Fedora Linux 44 (Workstation Edition), x86_64 |
| **Kernel** | Linux `7.2.5-200.fc44.x86_64` (`PREEMPT_DYNAMIC`) |
| **Sistema de Arquivos** | Btrfs com Copy-on-Write (`/dev/sda3`, montado em `/home`) |
| **Barreiras POSIX** | `fsync` em arquivo temporário $\rightarrow$ `rename` atômico $\rightarrow$ `fsync` no diretório pai |
| **Autenticação Criptográfica** | Assinatura Ed25519 (RFC 8032) sobre codificação canônica length-prefixed V4 |

---

## 2. Invariantes de Substrato Verificados

O ensaio automatizado `test_substrate_durability` executou 3 baterias de validação direta:

### 1. Morte Abrupta de Processo sem Flush (`_exit`)
- **Procedimento:** Processo filho realizou gênese autenticada e gerou fluxo contínuo de 50 ciclos epistêmicos com deliberação e persistência durável. No ápice da atividade, o processo foi terminado abruptamente via chamada de sistema `_exit(0)` do kernel Linux, ignorando `atexit`, destrutores C++ e buffers de usuário em espaço de memória.
- **Resultado:** O processo recuperador reconstruiu integralmente **152 eventos canônicos autenticados** e a verificação constitucional C1..C14 atestou conformidade total da identidade persistida.

### 2. Detecção de Escrita Incompleta / Blocos Corrompidos (*Torn Writes / Bit-Rot*)
- **Procedimento:** Injeção de truncamento artificial no arquivo persistido (remoção deliberada de bytes no fim do bloco canônico para simular barreira de I/O interrompida no disco físico).
- **Resultado:** O HRE (*Historical Recovery Engine*) **falhou fechado** com `EnteError::InvalidSignature` / `EnteError::HistoryCorrupt`, rejeitando sumariamente a biografia mutilada e impedindo mutação parcial de identidade.

### 3. Barreira de Diretório POSIX & Commit Point Atômico
- **Procedimento:** Persistência forçando sincronização com barreiras em cascata (`fsync(file)` $\rightarrow$ `rename` $\rightarrow$ `fsync(parent_dir)`).
- **Resultado:** A substituição atômica de inode garante que o leitor externo veja **exclusivamente** o snapshot anterior válido ou o snapshot subsequente inteiramente completado.

---

## 3. Matriz de Resultados

```text
=========================================================
   ENTE-1 G6 DECLARED-SUBSTRATE DURABILITY SUITE         
=========================================================

[G6-Substrate] 1. Testing unbuffered process termination across lifecycle points...
  -> Successfully recovered 152 authenticated events post abrupt _exit.
  -> Constitutional integrity verified on recovered substrate.
[G6-Substrate] 2. Testing torn writes and bit-rot detection on storage substrate...
  -> Truncated / torn block safely rejected with fail-closed error.
[G6-Substrate] 3. Testing directory entry commit point barriers...
  -> Directory barrier and atomic rename confirmed intact.

>>> G6 SUBSTRATE DURABILITY VERIFICATION PASSED (3/3) <<<
```

---

## 4. Declaração Formal de Conformidade

O Gate **G6 — Declared-Substrate Durability** é classificado formalmente como:

> **`VERIFIED ON DECLARED SUBSTRATE`**  
> *(Linux 7.2.x x86_64, Btrfs / POSIX fsync + atomic rename barrier)*

*Nota de Escopo:* Ambientes com memórias não voláteis heterogêneas, discos sem garantia de barreira física de cache (write-back não sincronizado por hardware) ou hosts maliciosos com rollback privilege exigem âncoras externas (TPM/TEE Monotonic Counter ou remote witness) pertencentes a perfis futuros (`ENTE-DISTRIBUTED` / `ENTE-HARDENED`).
