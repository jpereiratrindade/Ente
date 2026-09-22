# ENTE-BOOTSTRAP-001 — Roteiro de Construção e Verificação de ENTE-0 em C++26

## v0.1.0 — execution-plan

```context-metadata+json
{
  "document": {
    "id": "ENTE-BOOTSTRAP-001",
    "version": "0.1.0",
    "status": "execution-plan",
    "title": "Roteiro de Construção e Verificação de ENTE-0 em C++26",
    "date": "2026-09-22"
  },
  "depends_on": [
    "ENTE-CONSTITUTION-001-v0.6.0",
    "ENTE-ONTOLOGY-001-v0.1.0",
    "ENTE-RCC-001-v0.1.0",
    "ENTE-SYSTEM-001-v0.1.0",
    "ENTE-EXPERIMENT-001-v0.1.0"
  ],
  "epistemic_scope": "bootstrap-procedure",
  "implementation_status": "in-progress",
  "purpose": "Fornecer as instruções executáveis passo a passo para compilar, verificar e validar ENTE-0 contra o protocolo experimental."
}
```

---

# 1. Instruções de Compilação

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

---

# 2. Instruções de Verificação (CTest)

```bash
ctest --test-dir build --output-on-failure --verbose
```
