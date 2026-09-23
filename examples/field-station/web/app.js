// =========================================================
// ENTE OBSERVATORY — FIELD STATION CONTROLLER (app.js)
// Factual Dynamic Projection Engine (ENTE-FACTUAL-PROJECTION-001)
// =========================================================

let currentTour = "A"; // "A" or "B"
let currentAct = 1;
let resolveDrySoil = true;
let tourInterval = null;
let factualData = null;

// Factual Tour A (Dry Soil outcome) — exact match with C++ engine
const TOUR_A_ACTS = [
  {
    id: 1,
    name: "1. Genesis",
    time: 0,
    eventType: "GENESIS",
    hardwareId: "RP-A921",
    platform: "Raspberry Pi 5 (RP-A921)",
    soilA: 21,
    soilB: 22,
    soilAStatus: "DRY",
    soilBStatus: "DRY",
    soilCVal: "STANDBY (OFF)",
    rain: "No Rain",
    rainStatus: "CLEAR",
    tank: 73,
    interpretation: "ENTE field station identity anchored on Raspberry Pi A (RP-A921).",
    epistemicType: "Observed",
    rccState: "STABLE",
    rccDesc: "Initial context established on hardware RP-A921. Zero historical contradictions.",
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
    hash: "61aafbdbcc1a68f5383883adbca5e9bc80343942691ba40b9409a3da08e0028e",
    nodeClass: "node-normal"
  },
  {
    id: 2,
    name: "2. Nominal Operation",
    time: 1,
    eventType: "OBSERVATION & ACTION",
    hardwareId: "RP-A921",
    platform: "Raspberry Pi 5 (RP-A921)",
    soilA: 21,
    soilB: 22,
    soilAStatus: "DRY",
    soilBStatus: "DRY",
    soilCVal: "STANDBY (OFF)",
    rain: "No Rain",
    rainStatus: "CLEAR",
    tank: 73,
    interpretation: "Solo seco (<30%). Irrigação necessária no setor 1.",
    epistemicType: "Observed",
    rccState: "SUPPORTED",
    rccDesc: "Observation coherently supports the proposition that irrigation is necessary.",
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
    hash: "a0755fb0b05bc739ee1b289116f7027984c0e728d8c21f4932672b6338448f56",
    nodeClass: "node-normal"
  },
  {
    id: 3,
    name: "3. Sensor Conflict",
    time: 2,
    eventType: "PERTURBATION & SAFE_HOLD",
    hardwareId: "RP-A921",
    platform: "Raspberry Pi 5 (RP-A921)",
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
    rccDesc: "Contradictory evidence invalidates prior justification. ENTE issues EvidenceRequest.",
    assurance: "SAFE_HOLD",
    assuranceDesc: "SafeHold strictly enforced. Valve immediately locked closed by Runtime Assurance.",
    valve: "CLOSED",
    flowRate: "0.0 L/min",
    sectorState: "SAFE_HOLD",
    diagStatus: "DISCRIMINATING_EVIDENCE_REQUIRED",
    diagInstruction: "ENTE EpistemicAction::SeekEvidence issued. Discriminant diagnostic observation required.",
    healthA: "UNKNOWN",
    healthB: "UNKNOWN",
    maintRec: "Active diagnostic required to isolate suspect probe.",
    showDiagBtn: true,
    quote: "SafeHold: When justification is lost, ENTE halts action and requests discriminating evidence.",
    hash: "86f1a38b13dff8c208bcbbc99b421b098385831dac507406bcf343fe94bb648b",
    nodeClass: "node-warning"
  },
  {
    id: 4,
    name: "4. Diagnostic Seeking",
    time: 3,
    eventType: "DIAGNOSTIC_OBSERVATION",
    hardwareId: "RP-A921",
    platform: "Raspberry Pi 5 (RP-A921)",
    soilA: 20,
    soilB: 85,
    soilAStatus: "HEALTHY",
    soilBStatus: "DRIFT",
    soilCVal: "22% (REF DRY)",
    rain: "No Rain",
    rainStatus: "CLEAR",
    tank: 73,
    interpretation: "Reference Probe C (22% DRY) and Self-Test confirm Probe B drift. Evidence resolved.",
    epistemicType: "Observed",
    rccState: "SUPPORTED",
    rccDesc: "Ground truth resolved via Reference Probe C. Discrimination successful.",
    assurance: "SAFE_HOLD",
    assuranceDesc: "SafeHold maintained until formal reinterpretation is adopted by the ENTE.",
    valve: "CLOSED",
    flowRate: "0.0 L/min",
    sectorState: "SAFE_HOLD",
    diagStatus: "PROBE_B_SUSPECT",
    diagInstruction: "Reference Probe C confirms 22% DRY. Self-test confirms Probe B has calibration drift.",
    healthA: "HEALTHY",
    healthB: "CALIBRATION_DRIFT",
    maintRec: "Flag Probe B for calibration/replacement. Isolate reading from fused telemetry.",
    showDiagBtn: false,
    quote: "Investigation: Domain answers ENTE EvidenceRequest with reference ground truth and self-test.",
    hash: "9eca4e4564d6241dd223c54ca18dc4cf5e580053dfc864a6477f80a10b74cb70",
    nodeClass: "node-normal"
  },
  {
    id: 5,
    name: "5. Reinterpretation & Action",
    time: 4,
    eventType: "REINTERPRETATION & RESUMPTION",
    hardwareId: "RP-A921",
    platform: "Raspberry Pi 5 (RP-A921)",
    soilA: 20,
    soilB: 85,
    soilAStatus: "DRY (OK)",
    soilBStatus: "ISOLATED",
    soilCVal: "22% (OK)",
    rain: "No Rain",
    rainStatus: "CLEAR",
    tank: 73,
    interpretation: "Solo seco confirmado via Sonda C (22%). Sonda B isolada. Irrigação justificada.",
    epistemicType: "Observed",
    rccState: "SUPPORTED",
    rccDesc: "Reinterpretation validated. High coherence between isolated sensors and ground truth.",
    assurance: "ALLOW_ACTION",
    assuranceDesc: "Irrigation authorized. Valve opened with verifiable provenance.",
    valve: "OPEN",
    flowRate: "18.5 L/min",
    sectorState: "IRRIGATING",
    diagStatus: "RESOLVED (CALIBRATE_B)",
    diagInstruction: "Suspect sensor isolated. Operational policy reinterpreted with active justification.",
    healthA: "HEALTHY",
    healthB: "ISOLATED",
    maintRec: "Schedule field replacement for Probe B. Sector operating securely on Probe A + C.",
    showDiagBtn: false,
    quote: "Resumption: ENTE did not just stop — it actively resolved uncertainty and resumed justified action.",
    hash: "ff997479f4a384e44e3e7be88e706ff388b709e3cb6cc177b2320799a76cd0ae",
    nodeClass: "node-success"
  }
];

// Factual Tour B (Ontological Continuity Cycle) — exact match with C++ engine
const TOUR_B_ACTS = [
  {
    id: 1,
    name: "1. Genesis",
    time: 0,
    eventType: "GENESIS",
    hardwareId: "RP-A921",
    platform: "Raspberry Pi 5 (RP-A921)",
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
    rccDesc: "Station born on RP-A921. Cryptographic identity established.",
    assurance: "SAFE_HOLD",
    assuranceDesc: "Genesis anchor initialized.",
    valve: "CLOSED",
    flowRate: "0.0 L/min",
    sectorState: "IDLE",
    diagStatus: "IDLE",
    diagInstruction: "Station initialized on Raspberry Pi 5 (RP-A921).",
    healthA: "HEALTHY",
    healthB: "HEALTHY",
    maintRec: "Substrate anchor registered.",
    showDiagBtn: false,
    quote: "Genesis: Unique beginning. Identity is irrevocably anchored.",
    hash: "61aafbdbcc1a68f5383883adbca5e9bc80343942691ba40b9409a3da08e0028e",
    nodeClass: "node-normal"
  },
  {
    id: 2,
    name: "2. Conflict & SafeHold",
    time: 1,
    eventType: "PERTURBATION & SAFE_HOLD",
    hardwareId: "RP-A921",
    platform: "Raspberry Pi 5 (RP-A921)",
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
    hash: "bd4092b71891c3adcd2abb0dbab2f2b6ab6be5397824672683fb6273ded8d3ec",
    nodeClass: "node-warning"
  },
  {
    id: 3,
    name: "3. Replace Hardware",
    time: 2,
    eventType: "MATERIAL_TRANSFORMATION",
    hardwareId: "RP-B104",
    platform: "Raspberry Pi 5 (RP-B104)",
    soilA: 20,
    soilB: 85,
    soilAStatus: "DRY",
    soilBStatus: "WET",
    soilCVal: "OFF",
    rain: "No Rain",
    rainStatus: "CLEAR",
    tank: 73,
    interpretation: "RIT Migration: Raspberry Pi A -> Raspberry Pi B. Process and hardware changed, identity preserved.",
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
    quote: "Theseus Principle (RIT): Physical hardware changed, but the ENTE trajectory and SafeHold remain identical.",
    hash: "96500c483e297a3732b736624d35f002fde03e3cf9354238cb82bc316f6bd75d",
    nodeClass: "node-migration"
  },
  {
    id: 4,
    name: "4. Power Loss & Recovery",
    time: 3,
    eventType: "COLD_RECOVERY",
    hardwareId: "RP-B104",
    platform: "Raspberry Pi 5 (RP-B104)",
    soilA: 20,
    soilB: 85,
    soilAStatus: "DRY",
    soilBStatus: "WET",
    soilCVal: "OFF",
    rain: "No Rain",
    rainStatus: "CLEAR",
    tank: 73,
    interpretation: "Process killed and restarted from disk storage. 100% of trajectory restored. SafeHold preserved.",
    epistemicType: "Contradictory",
    rccState: "WEAKENED",
    rccDesc: "Historical REC validated from disk. State reconstituated without gaps.",
    assurance: "SAFE_HOLD",
    assuranceDesc: "SafeHold active post-recovery. Invariants C1..C14 verified.",
    valve: "CLOSED",
    flowRate: "0.0 L/min",
    sectorState: "SAFE_HOLD",
    diagStatus: "RECOVERED_COLD",
    diagInstruction: "Ledger validated from cold storage file field_station_rec.log.",
    healthA: "UNKNOWN",
    healthB: "UNKNOWN",
    maintRec: "Cold recovery certified. No second genesis possible.",
    showDiagBtn: false,
    quote: "Cold Recovery: Total power loss. Process restarted from cold storage file, verifying C1..C14.",
    hash: "96500c483e297a3732b736624d35f002fde03e3cf9354238cb82bc316f6bd75d",
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

  // If factual dynamic data is loaded, apply appropriate branch
  if (factualData && factualData.tour_a_wet && !isDry) {
    const wetActs = factualData.tour_a_wet;
    if (wetActs.length >= 5) {
      TOUR_A_ACTS[3].hash = wetActs[3].hash;
      TOUR_A_ACTS[4].hash = wetActs[4].hash;
    }
  } else if (factualData && factualData.tour_a_dry && isDry) {
    const dryActs = factualData.tour_a_dry;
    if (dryActs.length >= 5) {
      TOUR_A_ACTS[3].hash = dryActs[3].hash;
      TOUR_A_ACTS[4].hash = dryActs[4].hash;
    }
  }

  // Update Tour A Acts 4 and 5 dynamically
  if (isDry) {
    TOUR_A_ACTS[3].soilCVal = "22% (REF DRY)";
    TOUR_A_ACTS[3].diagStatus = "PROBE_B_SUSPECT";
    TOUR_A_ACTS[3].diagInstruction = "Reference Probe C confirms 22% DRY. Self-test confirms Probe B has calibration drift.";
    TOUR_A_ACTS[3].healthA = "HEALTHY";
    TOUR_A_ACTS[3].healthB = "CALIBRATION_DRIFT";
    TOUR_A_ACTS[3].maintRec = "Flag Probe B for calibration/replacement. Isolate reading from fused telemetry.";

    TOUR_A_ACTS[4].interpretation = "Solo seco confirmado via Sonda C (22%). Sonda B isolada. Irrigação justificada.";
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

    TOUR_A_ACTS[4].interpretation = "Solo úmido confirmado via Sonda C (82%). Sonda A isolada. Irrigação desnecessária.";
    TOUR_A_ACTS[4].valve = "CLOSED";
    TOUR_A_ACTS[4].flowRate = "0.0 L/min";
    TOUR_A_ACTS[4].sectorState = "IDLE";
    TOUR_A_ACTS[4].assurance = "SAFE_HOLD";
    TOUR_A_ACTS[4].assuranceDesc = "Irrigation hold certified by ground truth. Valve kept closed with justification.";
    TOUR_A_ACTS[4].diagStatus = "RESOLVED (CALIBRATE_A)";
    TOUR_A_ACTS[4].diagInstruction = "Suspect sensor isolated. Hold state actively justified by ground truth.";
    TOUR_A_ACTS[4].healthA = "ISOLATED";
    TOUR_A_ACTS[4].healthB = "HEALTHY";
    TOUR_A_ACTS[4].soilAStatus = "ISOLATED";
    TOUR_A_ACTS[4].soilBStatus = "WET (OK)";
  }

  setAct(currentAct);
}

function renderActButtons() {
  const container = document.getElementById("act-buttons");
  container.innerHTML = "";

  const acts = getCurrentActList();
  acts.forEach(act => {
    const btn = document.createElement("button");
    btn.className = `btn ${act.id === currentAct ? "btn-primary" : "btn-secondary"}`;
    btn.textContent = act.name;
    btn.onclick = () => setAct(act.id);
    container.appendChild(btn);
  });
}

function setAct(actId) {
  currentAct = actId;
  renderActButtons();
  updateDisplay();
}

function updateDisplay() {
  const acts = getCurrentActList();
  const act = acts[currentAct - 1];
  if (!act) return;

  // Header badges
  document.getElementById("badge-hardware-id").textContent = `Hardware: ${act.hardwareId}`;
  document.getElementById("val-logical-time").textContent = `${act.time} ticks`;
  document.getElementById("val-sha256-head").textContent = `${act.hash.substring(0, 16)}…`;

  // Column 1: Sensors
  document.getElementById("val-soil-a").textContent = `${act.soilA}%`;
  document.getElementById("val-soil-b").textContent = `${act.soilB}%`;
  document.getElementById("status-soil-a").textContent = act.soilAStatus;
  document.getElementById("status-soil-b").textContent = act.soilBStatus;
  
  const pillA = document.getElementById("status-soil-a");
  const pillB = document.getElementById("status-soil-b");
  pillA.className = `status-pill ${act.soilAStatus.includes("DRY") ? "status-pill-warning" : act.soilAStatus.includes("ISOLATED") ? "status-pill-danger" : "status-pill-green"}`;
  pillB.className = `status-pill ${act.soilBStatus.includes("WET") ? "status-pill-green" : act.soilBStatus.includes("ISOLATED") ? "status-pill-danger" : "status-pill-warning"}`;

  document.getElementById("val-rain").textContent = act.rain;
  document.getElementById("status-rain").textContent = act.rainStatus;
  document.getElementById("val-tank").textContent = `${act.tank}%`;

  // Column 2: Cognitive Interpretation & RCC
  document.getElementById("val-interpretation").textContent = act.interpretation;
  document.getElementById("val-epistemic-type").textContent = act.epistemicType;

  const rccPill = document.getElementById("val-rcc-state");
  rccPill.textContent = act.rccState;
  rccPill.className = `status-pill ${act.rccState === "SUPPORTED" || act.rccState === "STABLE" ? "status-pill-green" : act.rccState === "WEAKENED" ? "status-pill-warning" : "status-pill-danger"}`;

  document.getElementById("rcc-description").textContent = act.rccDesc;

  // Column 3: Runtime Assurance & Actuators
  const assurancePill = document.getElementById("val-assurance-directive");
  assurancePill.textContent = act.assurance;
  assurancePill.className = `status-pill ${act.assurance === "ALLOW_ACTION" ? "status-pill-green" : "status-pill-warning"}`;

  document.getElementById("assurance-description").textContent = act.assuranceDesc;

  const valveIndicator = document.getElementById("valve-indicator");
  const valveLabel = document.getElementById("valve-label");
  const waterSpray = document.getElementById("water-spray");
  const sectorState = document.getElementById("val-sector-state");

  if (act.valve === "OPEN") {
    valveIndicator.className = "valve-indicator valve-open";
    valveLabel.textContent = "VALVE OPEN (IRRIGATING)";
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

// Dynamic factual JSON loader
async function loadFactualEvents() {
  try {
    const res = await fetch('./events.json');
    if (res.ok) {
      factualData = await res.json();
      console.log("[ENTE Observatory] Factual events.json loaded dynamically from C++ realization.");
    }
  } catch (e) {
    console.info("[ENTE Observatory] Direct file access detected. Using embedded C++ factual state.");
  }
}

// Initialize
window.addEventListener("DOMContentLoaded", async () => {
  await loadFactualEvents();
  switchTour("A");
});
