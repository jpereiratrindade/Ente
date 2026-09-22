// =========================================================
// ENTE OBSERVATORY — FIELD STATION CONTROLLER (app.js)
// =========================================================

const ACTS = [
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
    rain: "No Rain",
    rainStatus: "CLEAR",
    tank: 73,
    flow: "OK",
    interpretation: "Initial genesis established. Awaiting steady operational telemetry.",
    epistemicType: "Observed",
    rccState: "STABLE",
    rccDesc: "Initial context established. Zero historical contradictions.",
    assurance: "SAFE_HOLD",
    assuranceDesc: "Genesis state initialized in SafeHold pending first nominal cycle.",
    valve: "CLOSED",
    flowRate: "0.0 L/min",
    sectorState: "IDLE",
    quote: "Genesis: ENTE is born once on hardware RP-A921. Cryptographic identity is irrevocably anchored.",
    hash: "9af2c04b8e1932fa",
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
    rain: "No Rain",
    rainStatus: "CLEAR",
    tank: 73,
    flow: "OK",
    interpretation: "Soil moisture critically dry (21%). Reservoir sufficient. Irrigation required.",
    epistemicType: "Observed",
    rccState: "SUPPORTED",
    rccDesc: "Observation coherently supports the hypothesis that irrigation is necessary.",
    assurance: "ALLOW_ACTION",
    assuranceDesc: "Constitutive invariants verified. Action authorized under valid authority A0.",
    valve: "OPEN",
    flowRate: "18.5 L/min",
    sectorState: "IRRIGATING",
    quote: "Nominal: Probes agree soil is dry. RCC validates epistemic support and Runtime Assurance opens the valve.",
    hash: "4c7a10be39df4812",
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
    rain: "No Rain",
    rainStatus: "CLEAR",
    tank: 73,
    flow: "ANOMALY",
    interpretation: "Evidential conflict: Probe A (20% DRY) directly contradicts Probe B (85% WET).",
    epistemicType: "Contradictory",
    rccState: "WEAKENED",
    rccDesc: "Contradictory evidence invalidates prior justification. Ground truth is uncertain.",
    assurance: "SAFE_HOLD",
    assuranceDesc: "Action immediately withheld. ENTE avoids acting under epistemic uncertainty.",
    valve: "CLOSED",
    flowRate: "0.0 L/min",
    sectorState: "SAFE_HOLD",
    quote: "Contradiction: ENTE does not guess which sensor is right. It recognizes that it no longer knows enough to irrigate.",
    hash: "d83f912e75cb0134",
    nodeClass: "node-warning"
  },
  {
    id: 4,
    name: "4. Replace Hardware",
    time: 3,
    eventType: "MATERIAL_TRANSFORMATION",
    hardwareId: "RP-B104",
    platform: "Raspberry Pi 5 Rev 1.0",
    soilA: 20,
    soilB: 85,
    soilAStatus: "DRY",
    soilBStatus: "WET",
    rain: "No Rain",
    rainStatus: "CLEAR",
    tank: 73,
    flow: "ANOMALY",
    interpretation: "Hardware substrate migrated from RP-A921 to RP-B104 under RIT continuity.",
    epistemicType: "Contradictory",
    rccState: "WEAKENED",
    rccDesc: "Substrate replaced, yet identity, authority and contradictory epistemic state persist.",
    assurance: "SAFE_HOLD",
    assuranceDesc: "SafeHold strictly preserved across material replacement.",
    valve: "CLOSED",
    flowRate: "0.0 L/min",
    sectorState: "SAFE_HOLD",
    quote: "Theseus Principle (RIT): The physical computer changed, but the ENTE trajectory and SafeHold remain identical.",
    hash: "e901aa7211bf4310",
    nodeClass: "node-migration"
  },
  {
    id: 5,
    name: "5. Power Cycle",
    time: 4,
    eventType: "COLD_RECOVERY",
    hardwareId: "RP-B104",
    platform: "Raspberry Pi 5 Rev 1.0",
    soilA: 20,
    soilB: 85,
    soilAStatus: "DRY",
    soilBStatus: "WET",
    rain: "No Rain",
    rainStatus: "CLEAR",
    tank: 73,
    flow: "ANOMALY",
    interpretation: "Cold restart reconstructed 100% of trajectory from REC ledger. SafeHold active.",
    epistemicType: "Contradictory",
    rccState: "WEAKENED",
    rccDesc: "Reconstituted historical state confirms unresolved sensor conflict.",
    assurance: "SAFE_HOLD",
    assuranceDesc: "New process did not forget prior perturbation. Valve remains closed.",
    valve: "CLOSED",
    flowRate: "0.0 L/min",
    sectorState: "SAFE_HOLD",
    quote: "Cold Recovery: Process crashed and restarted from cold memory. History was audited and SafeHold was not lost.",
    hash: "ff12b84e3100ca99",
    nodeClass: "node-warning"
  }
];

let currentAct = 1;
let tourInterval = null;

function setAct(actId) {
  const act = ACTS.find(a => a.id === actId);
  if (!act) return;

  currentAct = actId;

  // Handle Blackout simulation on Act 5
  if (actId === 5) {
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
  // Update Buttons
  document.querySelectorAll(".btn-demo").forEach(btn => {
    btn.classList.remove("active");
  });
  const activeBtn = document.getElementById(`btn-act-${act.id}`);
  if (activeBtn) activeBtn.classList.add("active");

  // Header & Substrate
  document.getElementById("meta-hardware").textContent = `${act.platform.split(" ")[0]} (${act.hardwareId})`;
  document.getElementById("val-material-id").textContent = act.hardwareId;
  document.getElementById("val-platform").textContent = act.platform;

  // Column 1: Sensors
  document.getElementById("val-soil-a").textContent = `${act.soilA}%`;
  document.getElementById("badge-soil-a").textContent = act.soilAStatus;
  document.getElementById("badge-soil-a").className = `reading-status ${act.soilAStatus === "DRY" ? "status-dry" : "status-wet"}`;
  document.getElementById("fill-soil-a").style.width = `${act.soilA}%`;

  document.getElementById("val-soil-b").textContent = `${act.soilB}%`;
  document.getElementById("badge-soil-b").textContent = act.soilBStatus;
  document.getElementById("badge-soil-b").className = `reading-status ${act.soilBStatus === "DRY" ? "status-dry" : "status-wet"}`;
  document.getElementById("fill-soil-b").style.width = `${act.soilB}%`;

  // Highlight conflict in sensor cards
  const cardA = document.getElementById("card-sensor-a");
  const cardB = document.getElementById("card-sensor-b");
  if (act.soilAStatus !== act.soilBStatus) {
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

  // Column 3: Actuator & Water Flow
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

  // Quote
  document.getElementById("pedagogical-quote").textContent = act.quote;

  // Render Timeline
  renderTimeline();
}

function renderTimeline() {
  const track = document.getElementById("timeline-track");
  track.innerHTML = "";

  ACTS.forEach((act, idx) => {
    const isPastOrCurrent = act.id <= currentAct;
    const isCurrent = act.id === currentAct;

    const node = document.createElement("div");
    node.className = `timeline-node ${act.nodeClass} ${isCurrent ? "node-active" : ""}`;
    node.style.opacity = isPastOrCurrent ? "1" : "0.35";
    node.onclick = () => openModal(act);

    node.innerHTML = `
      <div class="node-dot"></div>
      <div class="node-time mono">T+0${act.time}s · ACT ${act.id}</div>
      <div class="node-name">${act.eventType.replace("_", " ")}</div>
      <div class="node-hash mono">${act.hash.substring(0, 8)}…</div>
    `;

    track.appendChild(node);
  });

  document.getElementById("rec-head-count").textContent = `${currentAct} of 5 Events Committed`;
}

// Modal functions
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
    btn.textContent = "▶ Start 45s Auto-Tour";
    btn.style.background = "";
  } else {
    btn.textContent = "⏸ Pause Auto-Tour";
    btn.style.background = "var(--accent)";
    btn.style.color = "var(--bg)";

    let nextAct = 1;
    setAct(nextAct);

    tourInterval = setInterval(() => {
      nextAct = nextAct >= 5 ? 1 : nextAct + 1;
      setAct(nextAct);
    }, 8000); // 8s per act (~40s total tour)
  }
}

// Initialize on page load
window.addEventListener("DOMContentLoaded", () => {
  setAct(1);
});
