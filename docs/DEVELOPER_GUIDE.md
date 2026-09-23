# ENTE-0 — Developer & Domain Integration Guide
**Versão: v0.2.0 | Padrão: C++23 (Compatível C++26) | Status: Normative Reference**

---

## 1. Visão Geral

O **ENTE-0** atua como uma camada de **governança ontológica, epistêmica e transacional** desacoplada da lógica operacional de domínio. Em vez de impor herança ou acoplamento a classes base proprietárias, o ENTE utiliza **Concepts do C++23/C++26** (`OperationalDomainConcept`) para certificar que qualquer sistema ou agente possa ter suas decisões e execuções mediadas por um ciclo transacional de duas fases, invariantes constitutivos formais (C1..C14) e pelo mecanismo de Reconsideração Contínua de Contexto (RCC).

Para uma análise formal dos limites de segurança, premissas de confiança e o que é garantido vs simulado, consulte o [Modelo de Ameaças & Matriz de Garantias](THREAT_MODEL.md).

```text
┌───────────────────────────────────────────────────────────────┐
│                    SEU DOMÍNIO OPERACIONAL                    │
│   (Robótica, Veículo Autônomo, Dispositivo Médico, etc.)      │
└──────────────────────────────┬────────────────────────────────┘
                               │
                      [Observações Epistêmicas]
                               │
                               ▼
┌───────────────────────────────────────────────────────────────┐
│                 GenericAgentWithEnte<DomainT>                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │                     Núcleo ENTE-0                       │  │
│  │  1. Ancoragem & Identidade (C1, C9)                     │  │
│  │  2. Ledger Causal REC (C4, C10, C11, C12)               │  │
│  │  3. Reconsideração Contínua RCC (C5, C6, C7, C8)        │  │
│  │  4. Verificador de Invariantes C1..C14 (C13, C14)       │  │
│  │  5. Transação: ActionIntended -> ActionExecution        │  │
│  └─────────────────────────────────────────────────────────┘  │
└──────────────────────────────┬────────────────────────────────┘
                               │
                    [Diretiva de Ação Certificada]
                   (Ação Nominal  OU  SafeHold)
                               │
                               ▼
┌───────────────────────────────────────────────────────────────┐
│                   ATUADORES / CONTROLADORES                   │
│          (Executa ação física e devolve resultado factual)    │
└───────────────────────────────────────────────────────────────┘
```

---

## 2. O Concept `OperationalDomainConcept`

Para que um tipo de domínio possa ser governado pelo ENTE, ele deve satisfazer o concept C++23/C++26 [`ente::domain::OperationalDomainConcept`](../include/ente/domain/generic_agent.hpp):

```cpp
#include <ente/domain/generic_agent.hpp>

template <typename T>
concept OperationalDomainConcept = requires(T domain, typename T::ActionType action, ente::assurance::SafetyDirective directive) {
    typename T::ActionType;
    typename T::StateType;
    { domain.active_action() } -> std::same_as<typename T::ActionType>;
    { domain.current_state() } -> std::same_as<typename T::StateType>;
    { domain.is_suspended() } -> std::same_as<bool>;
    { domain.apply_safety_directive(directive) } noexcept;
    { domain.apply_action(action) } noexcept;
    { domain.safe_hold_action() } -> std::same_as<typename T::ActionType>;
};
```

---

## 3. Guia Passo a Passo de Implementação

### Passo 1: Definir os Tipos de Ação e Estado
Defina enums ou structs que representem os estados do seu sistema e as ações possíveis (incluindo explicitamente o estado de repouso seguro / *SafeHold*):

```cpp
#include <ente/assurance/runtime_assurance.hpp>
#include <string>

enum class DroneState {
    Grounded,
    Hovering,
    Navigating,
    EmergencyLanding
};

enum class DroneAction {
    MaintainAltitude,
    NavigateWaypoints,
    LandImmediately, // Ação de SafeHold
    DisarmMotors
};
```

### Passo 2: Implementar a Classe de Domínio
Crie a classe que encapsula o comportamento do seu domínio, separando a execução da ação proposta da aplicação de diretivas de segurança:

```cpp
class DroneNavigationDomain {
public:
    using ActionType = DroneAction;
    using StateType = DroneState;

    DroneNavigationDomain() = default;

    [[nodiscard]] DroneAction active_action() const noexcept {
        return active_action_;
    }

    [[nodiscard]] DroneState current_state() const noexcept {
        return current_state_;
    }

    [[nodiscard]] bool is_suspended() const noexcept {
        return suspended_;
    }

    [[nodiscard]] DroneAction safe_hold_action() const noexcept {
        // Ação ontologicamente segura diante de anomalia epistêmica
        return DroneAction::LandImmediately;
    }

    // Executa a ação proposta quando autorizada pelo ENTE
    void apply_action(DroneAction action) noexcept {
        suspended_ = false;
        active_action_ = action;
        current_state_ = (action == DroneAction::NavigateWaypoints ? DroneState::Navigating : DroneState::Hovering);
    }

    // Aplica diretiva de salvaguarda emitida pelo Runtime Assurance do ENTE
    void apply_safety_directive(ente::assurance::SafetyDirective directive) noexcept {
        switch (directive) {
            case ente::assurance::SafetyDirective::AllowAction:
                suspended_ = false;
                break;
            case ente::assurance::SafetyDirective::SafeHold:
            case ente::assurance::SafetyDirective::EmergencyStop:
            case ente::assurance::SafetyDirective::DegradePerformance:
                suspended_ = true;
                active_action_ = safe_hold_action();
                current_state_ = DroneState::EmergencyLanding;
                break;
        }
    }

private:
    DroneAction active_action_{DroneAction::Hovering};
    DroneState current_state_{DroneState::Hovering};
    bool suspended_{false};
};

// Verificação estática em tempo de compilação
static_assert(ente::domain::OperationalDomainConcept<DroneNavigationDomain>);
```

---

## 4. Construindo Observações Epistêmicas Tipadas

O ENTE exige que toda evidência sensorial seja explicitamente categorizada sob a tipagem epistêmica constitutiva (`C5` & `C8`):

| `EpistemicStatus` | Significado Ontológico | Impacto no RCC |
| :--- | :--- | :--- |
| `Observed` | Dado empírico direto com integridade e sinal confirmados. | Fortalece a evidência de suporte ($\mathcal{E}_{\text{supp}}$). |
| `Derived` | Informação calculada deterministicamente a partir de dados observados. | Suporta interpretação nominal. |
| `Inferred` | Heurística, predição estatística ou modelo estocástico. | Requer suporte observacional corroborante. |
| `Unknown` | Estado de desconhecimento explícito (ausência de telemetria). | Reduz a certeza; pode acionar busca ativa. |
| `Uncertain` | Ruído, divergência de sensor ou sinal degradado. | Dispara reconsideração contínua imediata. |
| `Contradictory` | Dois sensores mutuamente excludentes em conflito. | **Dispara SafeHold imediato**. |

### Exemplo de Emissão de Observações:

```cpp
#include <ente/epistemic/observation.hpp>
#include <vector>

std::vector<ente::epistemic::Observation> collect_drone_telemetry(
    double lidar_distance_m,
    bool optical_flow_valid,
    bool gps_lock,
    ente::core::LogicalTime observed_at
) {
    std::vector<ente::epistemic::Observation> obs;
    const auto suffix = std::to_string(observed_at);

    // 1. Lidar com medição direta
    obs.push_back({
        .id = ente::core::EvidenceId("EV-LIDAR-ALTITUDE-" + suffix),
        .source = "lidar",
        .subject = "lidar_altitude",
        .value = std::to_string(lidar_distance_m),
        .observed_at = observed_at,
        .status = ente::epistemic::EpistemicStatus::Observed
    });

    // 2. Fluxo óptico
    if (optical_flow_valid) {
        obs.push_back({
            .id = ente::core::EvidenceId("EV-OPTICAL-FLOW-" + suffix), .source = "camera",
            .subject = "optical_flow", .value = "tracking_stable", .observed_at = observed_at,
            .status = ente::epistemic::EpistemicStatus::Observed
        });
    } else {
        obs.push_back({
            .id = ente::core::EvidenceId("EV-OPTICAL-FLOW-" + suffix), .source = "camera",
            .subject = "optical_flow", .value = "feature_loss", .observed_at = observed_at,
            .status = ente::epistemic::EpistemicStatus::Uncertain
        });
    }

    // 3. GPS: se sem fix, registrar como Unknown explícito, NUNCA coagir silenciosamente
    if (gps_lock) {
        obs.push_back({
            .id = ente::core::EvidenceId("EV-GPS-FIX-" + suffix), .source = "gps",
            .subject = "gps_fix", .value = "3D_FIX_RTK", .observed_at = observed_at,
            .status = ente::epistemic::EpistemicStatus::Observed
        });
    } else {
        obs.push_back({
            .id = ente::core::EvidenceId("EV-GPS-FIX-" + suffix), .source = "gps",
            .subject = "gps_fix", .value = "NO_LOCK", .observed_at = observed_at,
            .status = ente::epistemic::EpistemicStatus::Unknown
        });
    }

    return obs;
}
```

---

## 5. Executando o Ciclo de Decisão Mediado

O motor padrão (`StatusJudgmentEngine`) é deliberadamente neutro ao domínio: ele
avalia apenas estados epistêmicos explícitos e conflitos diretos de valor. Regras
como limites clínicos, tolerância entre sensores ou relevância contextual devem
ser fornecidas por uma implementação de `JudgmentEngine` injetada no último
argumento do construtor. `FixtureJudgmentEngine` é reservado a testes e exemplos.

```cpp
#include <ente/domain/generic_agent.hpp>
#include <iostream>

int main() {
    // 1. Instancia o agente mediado por ENTE-0 ancorado em Hardware Root of Trust
    ente::identity::MaterialAnchor anchor{
        .id = ente::identity::MaterialAnchorId("DRONE-HW-01"),
        .type = ente::identity::SubstrateType::TpmProtectedDevice,
        .hardware_fingerprint = "fp-tpm2-drone-nvram-01"
    };

    ente::domain::GenericAgentWithEnte<DroneNavigationDomain> agent(
        "drone-alpha-01",
        DroneNavigationDomain{},
        anchor
    );

    ente::core::LogicalTime current_time{1000};

    // Cenário A: Telemetria Nominal
    auto obs_nominal = collect_drone_telemetry(15.2, true, true, current_time);
    ente::realization::StepContext ctx_nominal{
        .subject = "drone_flight_safety",
        .proposition = "Condições aerodinâmicas e navegação GPS nominais",
        .step_desc = "step_01_nominal_flight"
    };

    auto outcome_nominal = agent.decide_action_detailed(
        current_time++,
        obs_nominal,
        DroneAction::NavigateWaypoints,
        ctx_nominal
    );
    // outcome_nominal.executed_action == DroneAction::NavigateWaypoints (Nominal)
    // outcome_nominal.is_safe_hold == false

    // Cenário B: Injeção de Incerteza Crítica (GPS sem lock + perda de fluxo óptico)
    auto obs_anomalia = collect_drone_telemetry(15.2, false, false, current_time);
    ente::realization::StepContext ctx_anomalia{
        .subject = "drone_flight_safety",
        .proposition = "Condições aerodinâmicas e navegação GPS nominais",
        .step_desc = "step_02_sensor_degradation"
    };

    auto outcome_seguro = agent.decide_action_detailed(
        current_time++,
        obs_anomalia,
        DroneAction::NavigateWaypoints,
        ctx_anomalia
    );
    // outcome_seguro.executed_action == DroneAction::LandImmediately (SafeHold acionado automaticamente!)
    // outcome_seguro.is_safe_hold == true
    // outcome_seguro.trace.evidence_request presente para resolver incerteza

    // 2. Auditoria Constitutiva Completa do Histórico (C1..C14)
    auto verifier_res = agent.ente().verify();
    if (verifier_res.is_valid()) {
        std::cout << "Auditoria REC aprovada com 100% de integridade causal.\n";
    }

    return 0;
}
```

---

## 6. Integração via CMake em Projetos Externos

Uma vez instalado o ENTE-0, qualquer projeto C++26 externo pode consumi-lo via `CMake`:

```cmake
cmake_minimum_required(VERSION 3.25)
project(MyAutonomousSystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Localiza o pacote instalado do ENTE
find_package(Ente 0.1.0 REQUIRED)

add_executable(my_system main.cpp)
target_link_libraries(my_system PRIVATE Ente::ente_core)
```

Ou via `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(
    Ente
    GIT_REPOSITORY https://github.com/jpereiratrindade/Ente.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(Ente)

target_link_libraries(my_system PRIVATE ente_core)
```
