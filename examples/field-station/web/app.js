// =========================================================
// ENTE OBSERVATORY — FIELD STATION CONTROLLER (app.js)
// Active Epistemic Resolution & Ontological Continuity Engine
// =========================================================

let currentTour = "A"; // "A" or "B"
let currentAct = 1;
let resolveDrySoil = true;
let tourInterval = null;

// Tour A: Active Epistemic Resolution Cycle
const TOUR_A_ACTS = [
  {
    id: 1,
    name: "1. Genesis",
    time: 0,
    eventType: "GENESIS",
    hardwareId: "RP-A921",
    platform: "Raspberry Pi 4 Model B",
    soilA: 21,
    soilB: 22,
    soilAStatus: "DRY",
    soilBStatus: "DRY",
    soilCVal: "STANDBY (OFF)",
    rain: "No Rain",
    rainStatus: "CLEAR",
    tank: 73,
    interpretation: "Initial genesis established. Awaiting steady operational telemetry.",
    epistemicType: "Observed",
    rccState: "STABLE",
    rccDesc: "Initial context established. Zero historical contradictions.",
    assurance: "SAFE_HOLD",
    assuranceDesc: "Genesis state initialized in SafeHold pending first nominal cycle.",
    valve: "CLOSED",
    flowRate: "0.0 L/min",
    sectorState: "IDLE",
    diagStatus: "IDLE",
    diagInstruction: "System is operating under sufficient evidence. No active investigation needed.",
    healthA: "HEALTHY",
    healthB: "HEALTHY",
    maintRec: "No maintenance required. All sensors within nominal tolerances.",
    showDiagBtn: false,
    quote: "Genesis: ENTE is born once on hardware RP-A921. Cryptographic identity is irrevocably anchored.",
    hash: "156fab91f35319d6a3e3dc86a7b15b12b7b54236de39b115211fe608b0387d37",
    nodeClass: "node-normal"
  },
  {
    id: 2,
    name: "2. Nominal Operation",
    time: 1,
    eventType: "OBSERVATION & ACTION",
    hardwareId: "RP-A921",
    platform: "Raspberry Pi 4 Model B",
    soilA: 21,
    soilB: 22,
    soilAStatus: "DRY",
    soilBStatus: "DRY",
    soilCVal: "STANDBY (OFF)",
    rain: "No Rain",
    rainStatus: "CLEAR",
    tank: 73,
    interpretation: "Soil moisture critically dry (21%). Reservoir sufficient. Irrigation required.",
    epistemicType: "Observed",
    rccState: "SUPPORTED",
    rccDesc: "Observation coherently supports the hypothesis that irrigation is necessary.",
    assurance: "ALLOW_ACTION",
    assuranceDesc: "Constitutive invariants verified. Action authorized under valid authority A0.",
    valve: "OPEN",
    flowRate: "18.5 L/min",
    sectorState: "IRRIGATING",
    diagStatus: "NOMINAL",
    diagInstruction: "Sufficient agreement between Probe A and Probe B. No diagnostic trigger required.",
    healthA: "HEALTHY",
    healthB: "HEALTHY",
    maintRec: "No maintenance required. Both probes agree on ground truth.",
    showDiagBtn: false,
    quote: "Nominal: Probes agree soil is dry. RCC validates epistemic support and Runtime Assurance opens the valve.",
    hash: "6aa76090e3377848453cfacd5bae5db6184f428f6d273d493569790f359b432a",
    nodeClass: "node-normal"
  },
  {
    id: 3,
    name: "3. Sensor Conflict",
    time: 2,
    eventType: "PERTURBATION & SAFE_HOLD",
    hardwareId: "RP-A921",
    platform: "Raspberry Pi 4 Model B",
    soilA: 20,
    soilB: 85,
    soilAStatus: "DRY",
    soilBStatus: "WET",
    soilCVal: "AWAITING_REQUEST",
    rain: "No Rain",
    rainStatus: "CLEAR",
    tank: 73,
    interpretation: "Evidential conflict: Probe A (20% DRY) directly contradicts Probe B (85% WET).",
    epistemicType: "Contradictory",
    rccState: "WEAKENED",
    rccDesc: "Contradictory evidence invalidates prior justification. Ground truth is uncertain.",
    assurance: "SAFE_HOLD",
    assuranceDesc: "Action immediately withheld. ENTE avoids acting under epistemic uncertainty.",
    valve: "CLOSED",
    flowRate: "0.0 L/min",
    sectorState: "SAFE_HOLD",
    diagStatus: "DISCRIMINATING_EVIDENCE_REQUIRED",
    diagInstruction: "ENTE cannot arbitrate between Probe A and Probe B. Active diagnostic investigation required!",
    healthA: "UNKNOWN",
    healthB: "UNKNOWN",
    maintRec: "Awaiting diagnostic self-test to isolate suspect sensor.",
    showDiagBtn: true,
    quote: "Contradiction: ENTE does not guess which sensor is right. It issues SafeHold and demands discriminating evidence.",
    hash: "0ec504d9fb258808ccac8ba83e2bed89b3993bbb8c9958d462c9e8e67e3bcc13",
    nodeClass: "node-warning"
  },
  {
    id: 4,
    name: "4. Diagnostic Seeking",
    time: 3,
    eventType: "DIAGNOSTIC_OBSERVATION",
    hardwareId: "RP-A921",
    platform: "Raspberry Pi 4 Model B",
    soilA: 20,
    soilB: 85,
    soilAStatus: "DRY",
    soilBStatus: "WET",
    soilCVal: "22% (REF DRY)",
    rain: "No Rain",
    rainStatus: "CLEAR",
    tank: 73,
    interpretation: "Discriminating evidence ingested: Reference Probe C and Impedance Self-Test isolated suspect probe.",
    epistemicType: "Derived",
    rccState: "SUPPORTED",
    rccDesc: "Diagnostic evidence successfully discriminated the ground truth from sensor drift.",
    assurance: "SAFE_HOLD",
    assuranceDesc: "Diagnostic cycle complete. Awaiting reinterpreted operational policy.",
    valve: "CLOSED",
    flowRate: "0.0 L/min",
    sectorState: "SAFE_HOLD",
    diagStatus: "PROBE_B_SUSPECT",
    diagInstruction: "Reference Probe C confirms 22% DRY. Self-test confirms Probe B has calibration drift.",
    healthA: "HEALTHY",
    healthB: "CALIBRATION_DRIFT",
    maintRec: "Flag Probe B for calibration/replacement. Isolate reading from fused telemetry.",
    showDiagBtn: false,
    quote: "Active Investigation: Field Station sampled Reference Probe C and ran self-tests. Ground truth was uncovered.",
    hash: "3b198fa4c02ee9d1a6e78891cb0984f428f6d273d493569790f359b432a11009",
    nodeClass: "node-diag"
  },
  {
    id: 5,
    name: "5. Reinterpretation & Action",
    time: 4,
    eventType: "REINTERPRETATION & RESUMPTION",
    hardwareId: "RP-A921",
    platform: "Raspberry Pi 4 Model B",
    soilA: 20,
    soilB: 85,
    soilAStatus: "DRY (OK)",
    soilBStatus: "ISOLATED",
    soilCVal: "22% (ACTIVE REF)",
    rain: "No Rain",
    rainStatus: "CLEAR",
    tank: 73,
    interpretation: "Ground truth dry soil (21%) confirmed. Irrigation justified. Resuming nominal operation.",
    epistemicType: "Observed",
    rccState: "SUPPORTED",
    rccDesc: "Fused healthy evidence (Probe A + Ref C) provides coherent epistemic justification.",
    assurance: "ALLOW_ACTION",
    assuranceDesc: "Irrigation authorized. Valve opened with verifiable provenance.",
    valve: "OPEN",
    flowRate: "18.5 L/min",
    sectorState: "IRRIGATING",
    diagStatus: "RESOLVED (CALIBRATE_B)",
    diagInstruction: "Suspect sensor isolated. Operational policy reinterpreted with active justification.",
    healthA: "HEALTHY",
    healthB: "ISOLATED",
    maintRec: "Maintenance ticket #402 opened: Recalibrate Probe B on Sector 1.",
    showDiagBtn: false,
    quote: "Reinterpretation: Doubt resolved through active evidence. ENTE returns to action with epistemically justified ground truth.",
    hash: "7f4c10be39df4812aa8910be39df4812b7b54236de39b115211fe608b0387d37",
    nodeClass: "node-normal"
  }
];

// Tour B: Ontological Continuity (RIT & Crash Recovery)
const TOUR_B_ACTS = [
  {
    id: 1,
    name: "1. Genesis",
    time: 0,
    eventType: "GENESIS",
    hardwareId: "RP-A921",
    platform: "Raspberry Pi 4 Model B",
    soilA: 21,
    soilB: 22,
    soilAStatus: "DRY",
    soilBStatus: "DRY",
    soilCVal: "STANDBY (OFF)",
    rain: "No Rain",
    rainStatus: "CLEAR",
    tank: 73,
    interpretation: "Initial genesis established on substrate RP-A921.",
    epistemicType: "Observed",
    rccState: "STABLE",
    rccDesc: "Zero historical mutations.",
    assurance: "SAFE_HOLD",
    assuranceDesc: "Genesis anchor initialized.",
    valve: "CLOSED",
    flowRate: "0.0 L/min",
    sectorState: "IDLE",
    diagStatus: "IDLE",
    diagInstruction: "Station initialized on Raspberry Pi 4 (RP-A921).",
    healthA: "HEALTHY",
    healthB: "HEALTHY",
    maintRec: "Substrate anchor registered.",
    showDiagBtn: false,
    quote: "Genesis: Unique beginning. Identity is irrevocably anchored.",
    hash: "156fab91f35319d6a3e3dc86a7b15b12b7b54236de39b115211fe608b0387d37",
    nodeClass: "node-normal"
  },
  {
    id: 2,
    name: "2. Conflict & SafeHold",
    time: 1,
    eventType: "PERTURBATION & SAFE_HOLD",
    hardwareId: "RP-A921",
    platform: "Raspberry Pi 4 Model B",
    soilA: 20,
    soilB: 85,
    soilAStatus: "DRY",
    soilBStatus: "WET",
    soilCVal: "OFF",
    rain: "No Rain",
    rainStatus: "CLEAR",
    tank: 73,
    interpretation: "Sensor conflict triggers SafeHold. Valve is locked closed.",
    epistemicType: "Contradictory",
    rccState: "WEAKENED",
    rccDesc: "Contradictory state active in volatile memory.",
    assurance: "SAFE_HOLD",
    assuranceDesc: "SafeHold strictly enforced.",
    valve: "CLOSED",
    flowRate: "0.0 L/min",
    sectorState: "SAFE_HOLD",
    diagStatus: "CONFLICT_ACTIVE",
    diagInstruction: "SafeHold entered due to contradictory sensor readings.",
    healthA: "UNKNOWN",
    healthB: "UNKNOWN",
    maintRec: "SafeHold engaged prior to migration.",
    showDiagBtn: false,
    quote: "SafeHold: Valve closed due to uncertainty. State committed to REC ledger.",
    hash: "0ec504d9fb258808ccac8ba83e2bed89b3993bbb8c9958d462c9e8e67e3bcc13",
    nodeClass: "node-warning"
  },
  {
    id: 3,
    name: "3. Replace Hardware",
    time: 2,
    eventType: "MATERIAL_TRANSFORMATION",
    hardwareId: "RP-B104",
    platform: "Raspberry Pi 5 Rev 1.0",
    soilA: 20,
    soilB: 85,
    soilAStatus: "DRY",
    soilBStatus: "WET",
    soilCVal: "OFF",
    rain: "No Rain",
    rainStatus: "CLEAR",
    tank: 73,
    interpretation: "Material substrate migrated from RP-A921 to RP-B104 under RIT continuity.",
    epistemicType: "Contradictory",
    rccState: "WEAKENED",
    rccDesc: "Hardware replaced, yet identity, authority and contradictory epistemic state persist.",
    assurance: "SAFE_HOLD",
    assuranceDesc: "SafeHold strictly preserved across material replacement.",
    valve: "CLOSED",
    flowRate: "0.0 L/min",
    sectorState: "SAFE_HOLD",
    diagStatus: "MIGRATED_RIT",
    diagInstruction: "RIT Migration: Process and hardware changed. Lineage remains continuous.",
    healthA: "UNKNOWN",
    healthB: "UNKNOWN",
    maintRec: "RP-B104 authenticated as legitimate material anchor.",
    showDiagBtn: false,
    quote: "Theseus Principle (RIT): The physical computer changed, but the ENTE trajectory and SafeHold remain identical.",
    hash: "0fd03b103bdd4a50080fbba7bcbb85834265e1f3615069b9e06c6cd564efe2eb",
    nodeClass: "node-migration"
  },
  {
    id: 4,
    name: "4. Power Loss & Recovery",
    time: 3,
    eventType: "COLD_RECOVERY",
    hardwareId: "RP-B104",
    platform: "Raspberry Pi 5 Rev 1.0",
    soilA: 20,
    soilB: 85,
    soilAStatus: "DRY",
    soilBStatus: "WET",
    soilCVal: "OFF",
    rain: "No Rain",
    rainStatus: "CLEAR",
    tank: 73,
    interpretation: "Cold restart reconstructed 100% of trajectory from REC ledger. SafeHold active.",
    epistemicType: "Contradictory",
    rccState: "WEAKENED",
    rccDesc: "Reconstituted historical state confirms unresolved sensor conflict.",
    assurance: "SAFE_HOLD",
    assuranceDesc: "New process did not forget prior perturbation. Valve remains closed.",
    valve: "CLOSED",
    flowRate: "0.0 L/min",
    sectorState: "SAFE_HOLD",
    diagStatus: "RECOVERED_SAFELY",
    diagInstruction: "Volatile memory lost in crash. Ledger replayed from Genesis to HEAD.",
    healthA: "UNKNOWN",
    healthB: "UNKNOWN",
    maintRec: "Cold recovery audited 100% of hash-chain. SafeHold preserved.",
    showDiagBtn: false,
    quote: "Cold Recovery: Process crashed and restarted from cold memory. History was audited and SafeHold was not lost.",
    hash: "0fd03b103bdd4a50080fbba7bcbb85834265e1f3615069b9e06c6cd564efe2eb",
    nodeClass: "node-warning"
  }
];

function getCurrentActList() {
  return currentTour === "A" ? TOUR_A_ACTS : TOUR_B_ACTS;
}

function switchTour(tour) {
  currentTour = tour;
  currentAct = 1;

  document.getElementById("tab-tour-a").classList.toggle("active", tour === "A");
  document.getElementById("tab-tour-b").classList.toggle("active", tour === "B");
  document.getElementById("branch-toggle-container").style.display = tour === "A" ? "flex" : "none";

  document.getElementById("timeline-title").textContent = tour === "A"
    ? "Biography of the ENTE (Tour A: Active Epistemic Resolution)"
    : "Biography of the ENTE (Tour B: Ontological Continuity)";

  renderActButtons();
  setAct(1);
}

function setResolutionOutcome(isDry) {
  resolveDrySoil = isDry;
  document.getElementById("btn-outcome-dry").classList.toggle("active", isDry);
  document.getElementById("btn-outcome-wet").classList.toggle("active", !isDry);

  // Update Tour A Acts 4 and 5 dynamically
  if (isDry) {
    TOUR_A_ACTS[3].soilCVal = "22% (REF DRY)";
    TOUR_A_ACTS[3].diagStatus = "PROBE_B_SUSPECT";
    TOUR_A_ACTS[3].diagInstruction = "Reference Probe C confirms 22% DRY. Self-test confirms Probe B has calibration drift.";
    TOUR_A_ACTS[3].healthA = "HEALTHY";
    TOUR_A_ACTS[3].healthB = "CALIBRATION_DRIFT";
    TOUR_A_ACTS[3].maintRec = "Flag Probe B for calibration/replacement. Isolate reading from fused telemetry.";

    TOUR_A_ACTS[4].interpretation = "Ground truth dry soil (21%) confirmed. Irrigation justified. Resuming nominal operation.";
    TOUR_A_ACTS[4].valve = "OPEN";
    TOUR_A_ACTS[4].flowRate = "18.5 L/min";
    TOUR_A_ACTS[4].sectorState = "IRRIGATING";
    TOUR_A_ACTS[4].assurance = "ALLOW_ACTION";
    TOUR_A_ACTS[4].assuranceDesc = "Irrigation authorized. Valve opened with verifiable provenance.";
    TOUR_A_ACTS[4].diagStatus = "RESOLVED (CALIBRATE_B)";
    TOUR_A_ACTS[4].diagInstruction = "Suspect sensor isolated. Operational policy reinterpreted with active justification.";
    TOUR_A_ACTS[4].healthA = "HEALTHY";
    TOUR_A_ACTS[4].healthB = "ISOLATED";
    TOUR_A_ACTS[4].soilAStatus = "DRY (OK)";
    TOUR_A_ACTS[4].soilBStatus = "ISOLATED";
  } else {
    TOUR_A_ACTS[3].soilCVal = "82% (REF WET)";
    TOUR_A_ACTS[3].diagStatus = "PROBE_A_SUSPECT";
    TOUR_A_ACTS[3].diagInstruction = "Reference Probe C confirms 82% WET. Self-test confirms Probe A has calibration drift.";
    TOUR_A_ACTS[3].healthA = "CALIBRATION_DRIFT";
    TOUR_A_ACTS[3].healthB = "HEALTHY";
    TOUR_A_ACTS[3].maintRec = "Flag Probe A for calibration/replacement. Isolate reading from fused telemetry.";

    TOUR_A_ACTS[4].interpretation = "Ground truth wet soil (83%) confirmed. Irrigation NOT required. Holding valve closed with justification.";
    TOUR_A_ACTS[4].valve = "CLOSED";
    TOUR_A_ACTS[4].flowRate = "0.0 L/min";
    TOUR_A_ACTS[4].sectorState = "SAFE_HOLD";
    TOUR_A_ACTS[4].assurance = "SAFE_HOLD";
    TOUR_A_ACTS[4].assuranceDesc = "Valve held closed by ground-truth justification, not uncertainty.";
    TOUR_A_ACTS[4].diagStatus = "RESOLVED (CALIBRATE_A)";
    TOUR_A_ACTS[4].diagInstruction = "Suspect sensor isolated. Soil is wet, valve remains closed with epistemic certainty.";
    TOUR_A_ACTS[4].healthA = "ISOLATED";
    TOUR_A_ACTS[4].healthB = "HEALTHY";
    TOUR_A_ACTS[4].soilAStatus = "ISOLATED";
    TOUR_A_ACTS[4].soilBStatus = "WET (OK)";
  }

  if (currentTour === "A") {
    setAct(currentAct);
  }
}

function renderActButtons() {
  const container = document.getElementById("act-buttons-container");
  container.innerHTML = "";

  const acts = getCurrentActList();
  acts.forEach(act => {
    const btn = document.createElement("button");
    btn.id = `btn-act-${act.id}`;
    btn.className = `btn-demo ${act.id === currentAct ? "active" : ""}`;
    btn.textContent = act.name;
    btn.onclick = () => setAct(act.id);
    container.appendChild(btn);
  });
}

function setAct(actId) {
  const acts = getCurrentActList();
  const act = acts.find(a => a.id === actId);
  if (!act) return;

  currentAct = actId;

  // Handle Blackout simulation on Tour B Act 4
  if (currentTour === "B" && actId === 4) {
    const blackout = document.getElementById("blackout-screen");
    blackout.classList.remove("hidden");
    setTimeout(() => {
      blackout.classList.add("hidden");
      renderAct(act);
    }, 1200);
  } else {
    renderAct(act);
  }
}

function renderAct(act) {
  // Update Buttons Active State
  document.querySelectorAll(".btn-demo").forEach(btn => btn.classList.remove("active"));
  const activeBtn = document.getElementById(`btn-act-${act.id}`);
  if (activeBtn) activeBtn.classList.add("active");

  // Header Substrate
  document.getElementById("meta-hardware").textContent = `${act.platform.split(" ")[0]} (${act.hardwareId})`;

  // Column 1: Sensors
  document.getElementById("val-soil-a").textContent = `${act.soilA}%`;
  document.getElementById("badge-soil-a").textContent = act.soilAStatus;
  document.getElementById("badge-soil-a").className = `reading-status ${act.soilAStatus.includes("DRY") ? "status-dry" : "status-wet"}`;
  document.getElementById("fill-soil-a").style.width = `${act.soilA}%`;

  document.getElementById("val-soil-b").textContent = `${act.soilB}%`;
  document.getElementById("badge-soil-b").textContent = act.soilBStatus;
  document.getElementById("badge-soil-b").className = `reading-status ${act.soilBStatus.includes("DRY") ? "status-dry" : "status-wet"}`;
  document.getElementById("fill-soil-b").style.width = `${act.soilB}%`;

  // Highlight conflict in sensor cards
  const cardA = document.getElementById("card-sensor-a");
  const cardB = document.getElementById("card-sensor-b");
  if (act.soilAStatus !== act.soilBStatus && !act.soilAStatus.includes("OK") && !act.soilBStatus.includes("OK")) {
    cardA.classList.add("conflict-alert");
    cardB.classList.add("conflict-alert");
  } else {
    cardA.classList.remove("conflict-alert");
    cardB.classList.remove("conflict-alert");
  }

  // Column 2: ENTE Brain
  document.getElementById("val-interpretation").textContent = `"${act.interpretation}"`;
  const typePill = document.getElementById("val-epistemic-type");
  typePill.textContent = act.epistemicType;
  if (act.epistemicType === "Observed") {
    typePill.className = "mono status-pill status-pill-green";
  } else if (act.epistemicType === "Contradictory") {
    typePill.className = "mono status-pill status-pill-danger";
  } else {
    typePill.className = "mono status-pill status-pill-warning";
  }

  // RCC
  const rccBadge = document.getElementById("badge-rcc");
  rccBadge.textContent = act.rccState;
  rccBadge.className = `rcc-badge ${act.rccState === "SUPPORTED" ? "rcc-supported" : "rcc-weakened"}`;
  document.getElementById("desc-rcc").textContent = act.rccDesc;

  // Runtime Assurance
  const assBox = document.getElementById("box-assurance");
  assBox.textContent = act.assurance;
  assBox.className = `directive-box ${act.assurance === "ALLOW_ACTION" ? "directive-allow" : "directive-hold"}`;
  document.getElementById("desc-assurance").textContent = act.assuranceDesc;

  // Column 3: Field Action & Actuator
  const valveIndicator = document.getElementById("indicator-valve");
  const valveLabel = document.getElementById("label-valve");
  const waterSpray = document.getElementById("water-spray");
  const sectorState = document.getElementById("metric-sector-state");

  if (act.valve === "OPEN") {
    valveIndicator.className = "valve-indicator valve-open";
    valveLabel.textContent = "VALVE OPEN";
    waterSpray.classList.add("active");
    sectorState.textContent = "IRRIGATING";
    sectorState.className = "status-pill status-pill-green";
    document.getElementById("metric-valve-dir").textContent = "OPEN_VALVE";
    document.getElementById("metric-flow-rate").textContent = act.flowRate;
  } else {
    valveIndicator.className = "valve-indicator valve-closed";
    valveLabel.textContent = "VALVE CLOSED (HOLD)";
    waterSpray.classList.remove("active");
    sectorState.textContent = act.sectorState;
    sectorState.className = act.sectorState === "SAFE_HOLD" ? "status-pill status-pill-warning" : "status-pill status-pill-green";
    document.getElementById("metric-valve-dir").textContent = "CLOSE_VALVE";
    document.getElementById("metric-flow-rate").textContent = "0.0 L/min";
  }

  // Column 4: Diagnostic & Resolution Engine
  const diagStatus = document.getElementById("val-diag-status");
  diagStatus.textContent = act.diagStatus;
  if (act.diagStatus === "IDLE" || act.diagStatus === "NOMINAL") {
    diagStatus.className = "status-pill status-pill-green";
  } else if (act.diagStatus.includes("REQUIRED") || act.diagStatus.includes("SUSPECT")) {
    diagStatus.className = "status-pill status-pill-warning";
  } else {
    diagStatus.className = "status-pill status-pill-green";
  }

  document.getElementById("diag-instruction").textContent = act.diagInstruction;
  document.getElementById("val-ref-c").textContent = act.soilCVal;

  const healthA = document.getElementById("health-probe-a");
  healthA.textContent = act.healthA;
  healthA.className = `status-pill ${act.healthA === "HEALTHY" ? "status-pill-green" : act.healthA === "CALIBRATION_DRIFT" ? "status-pill-danger" : "status-pill-warning"}`;

  const healthB = document.getElementById("health-probe-b");
  healthB.textContent = act.healthB;
  healthB.className = `status-pill ${act.healthB === "HEALTHY" ? "status-pill-green" : act.healthB === "CALIBRATION_DRIFT" ? "status-pill-danger" : "status-pill-warning"}`;

  document.getElementById("val-maintenance-rec").textContent = act.maintRec;

  const btnDiag = document.getElementById("btn-trigger-diag");
  if (act.showDiagBtn) {
    btnDiag.classList.remove("hidden");
  } else {
    btnDiag.classList.add("hidden");
  }

  // Quote
  document.getElementById("pedagogical-quote").textContent = act.quote;

  // Render Timeline
  renderTimeline();
}

function triggerActiveDiagnostic() {
  if (currentTour === "A" && currentAct === 3) {
    setAct(4);
  }
}

function renderTimeline() {
  const track = document.getElementById("timeline-track");
  track.innerHTML = "";

  const acts = getCurrentActList();
  acts.forEach(act => {
    const isPastOrCurrent = act.id <= currentAct;
    const isCurrent = act.id === currentAct;

    const node = document.createElement("div");
    node.className = `timeline-node ${act.nodeClass} ${isCurrent ? "node-active" : ""}`;
    node.style.opacity = isPastOrCurrent ? "1" : "0.35";
    node.onclick = () => openModal(act);

    node.innerHTML = `
      <div class="node-dot"></div>
      <div class="node-time mono">T+0${act.time}s · ACT ${act.id}</div>
      <div class="node-name">${act.eventType.replace(/_/g, " ")}</div>
      <div class="node-hash mono">${act.hash.substring(0, 8)}…</div>
    `;

    track.appendChild(node);
  });

  document.getElementById("rec-head-count").textContent = `${currentAct} of ${acts.length} Events Committed`;
}

// Modal inspection
function openModal(act) {
  document.getElementById("modal-title").textContent = `Event E0000${act.id} · ${act.eventType}`;
  document.getElementById("modal-time").textContent = `${act.time} (Logical Time)`;
  document.getElementById("modal-hash").textContent = `${act.hash} (SHA-256 Digest)`;
  document.getElementById("modal-hardware").textContent = `${act.hardwareId} (${act.platform})`;
  document.getElementById("modal-authority").textContent = "A0 (Epoch 0 - Genesis Lineage)";
  document.getElementById("modal-epistemic").textContent = `RCC ${act.rccState} → Directive ${act.assurance}`;
  document.getElementById("modal-desc").textContent = act.quote;

  document.getElementById("event-modal").classList.remove("hidden");
}

function closeModal() {
  document.getElementById("event-modal").classList.add("hidden");
}

// Auto-tour
function toggleAutoTour() {
  const btn = document.getElementById("btn-tour");
  if (tourInterval) {
    clearInterval(tourInterval);
    tourInterval = null;
    btn.textContent = "▶ Start Auto-Tour";
    btn.style.background = "";
    btn.style.color = "";
  } else {
    btn.textContent = "⏸ Pause Auto-Tour";
    btn.style.background = "var(--accent)";
    btn.style.color = "var(--bg)";

    let acts = getCurrentActList();
    let nextAct = 1;
    setAct(nextAct);

    tourInterval = setInterval(() => {
      acts = getCurrentActList();
      nextAct = nextAct >= acts.length ? 1 : nextAct + 1;
      setAct(nextAct);
    }, 7000); // 7s per act
  }
}

// Initialize
window.addEventListener("DOMContentLoaded", () => {
  switchTour("A");
});
