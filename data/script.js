/* script.js - UI v2 */

(() => {
  "use strict";

  // ---------- DOM helpers ----------
  const $ = (id) => document.getElementById(id);

  const wsStatusEl = $("wsStatus");
  const lastMsgEl = $("lastMsg");

  const stateEls = {
    idNode: $("idNode"),
    p00: $("p00"), p01: $("p01"), p10: $("p10"), p11: $("p11"),
    m00: $("m00"), m01: $("m01"), m10: $("m10"), m11: $("m11"),
    sensMarche: $("sensMarche"),
    maxSpeedValue: $("maxSpeedValue"),
  };

  const locoEls = {
    address: $("locoAddress"),
    speed: $("locoSpeed"),
    direction: $("locoDirection")
  };

  const imgHoraire = $("imgHoraire");
  const imgAntiHor = $("imgAntiHor");

  const wifiOnEl = $("wifi_on");
  const discoveryOnEl = $("discovery_on");

  const maxSpeedRange = $("maxSpeedRange");

  const throttleRange = $("throttleRange");
  const throttleVal = $("throttleVal");

  const btnSave = $("btnSave");
  const btnRestart = $("btnRestart");

  const servoContainer = $("servoContainer");

  // ---------- WS ----------
  let ws = null;
  let reconnectAttempt = 0;
  let manualClose = false;
  let lastState = null;

  // debounce map for sliders (avoid spamming)
  const debounceTimers = new Map();
  function debounce(key, delayMs, fn) {
    if (debounceTimers.has(key)) clearTimeout(debounceTimers.get(key));
    debounceTimers.set(key, setTimeout(() => {
      debounceTimers.delete(key);
      fn();
    }, delayMs));
  }

  function setWsPill(kind, text) {
    wsStatusEl.className = "pill " + (kind === "ok" ? "pill-ok" : kind === "bad" ? "pill-bad" : "pill-warn");
    wsStatusEl.textContent = text;
  }

  function sendCmd(obj) {
    if (!ws || ws.readyState !== 1) return;
    ws.send(JSON.stringify(obj));
  }

  function connectWs() {
    if (ws && (ws.readyState === 0 || ws.readyState === 1)) return;

    const proto = (location.protocol === "https:") ? "wss://" : "ws://";
    const url = proto + location.host + "/ws";

    manualClose = false;
    setWsPill("warn", "connexion…");

    ws = new WebSocket(url);

    ws.onopen = () => {
      reconnectAttempt = 0;
      setWsPill("ok", "connecté");
      // à la connexion, le firmware V2 envoie l’état en unicast
    };

    ws.onclose = () => {
      setWsPill("bad", "déconnecté");
      if (manualClose) return;

      reconnectAttempt++;
      const delay = Math.min(8000, 300 + reconnectAttempt * 400); // backoff simple
      setTimeout(connectWs, delay);
    };

    ws.onerror = () => {
      // l'onclose gère la suite
      setWsPill("bad", "erreur");
    };

    ws.onmessage = (evt) => {
      if (typeof evt.data !== "string") return;

      lastMsgEl.textContent = new Date().toLocaleTimeString();

      let msg;
      try {
        msg = JSON.parse(evt.data);
      } catch (e) {
        return;
      }

      // ACK/ERR facultatifs
      if (msg && msg.ok) {
        // tu peux afficher un toast si tu veux
      }
      if (msg && msg.error) {
        // idem
      }

      // state payload ?
      if (msg && typeof msg.idNode !== "undefined") {
        applyState(msg);
      }
      //-> Ajout 16/06/26 22:52
      document.getElementById("locoAddress").textContent = data.locoAddress ?? "--";
      document.getElementById("locoSpeed").textContent = data.locoSpeed ?? "--";
      document.getElementById("locoTargetSpeed").textContent = data.locoTargetSpeed ?? "--";
      document.getElementById("locoDirection").textContent = data.locoDirection ?? "--";
      document.getElementById("locoSens").textContent = data.locoSens ?? "--";
      document.getElementById("busy").textContent = data.busy ? "Oui" : "Non";
      //-> End ajout
    };
  }

  // ---------- UI building (servos) ----------
  function servoCard(i) {
    const wrap = document.createElement("div");
    wrap.className = "w3-card w3-white w3-padding w3-margin-bottom card";

    wrap.innerHTML = `
      <div class="w3-row">
        <div class="w3-col s12 m6">
          <h5 class="w3-margin-top-0">Servo ${i} : <span class="mono value" id="s${i}_status">--</span></h5>
        </div>
        <div class="w3-col s12 m6 w3-right-align">
          <button class="w3-button w3-green w3-round" id="btnServoTest${i}">Tester</button>
        </div>
      </div>

      <div class="grid-3 w3-margin-top">
        <div>
          <div class="label">Pos droite</div>
          <input type="range" min="500" max="2500" step="1" id="s${i}0" />
          <div class="tiny">Valeur : <span class="mono value" id="s${i}0v">--</span></div>
        </div>

        <div>
          <div class="label">Pos déviée</div>
          <input type="range" min="500" max="2500" step="1" id="s${i}1" />
          <div class="tiny">Valeur : <span class="mono value" id="s${i}1v">--</span></div>
        </div>

        <div>
          <div class="label">Vitesse (1..10)</div>
          <input type="range" min="1" max="10" step="1" id="s${i}2" />
          <div class="tiny">Valeur : <span class="mono value" id="s${i}2v">--</span></div>
        </div>
      </div>
    `;

    // handlers
    wrap.querySelector(`#btnServoTest${i}`).addEventListener("click", () => {
      sendCmd({ servoTest: [String(i)] });
    });

    // sliders -> debounce
    const s0 = wrap.querySelector(`#s${i}0`);
    const s1 = wrap.querySelector(`#s${i}1`);
    const s2 = wrap.querySelector(`#s${i}2`);

    const s0v = wrap.querySelector(`#s${i}0v`);
    const s1v = wrap.querySelector(`#s${i}1v`);
    const s2v = wrap.querySelector(`#s${i}2v`);

    s0.addEventListener("input", () => { s0v.textContent = s0.value; });
    s1.addEventListener("input", () => { s1v.textContent = s1.value; });
    s2.addEventListener("input", () => { s2v.textContent = s2.value; });

    s0.addEventListener("change", () => {
      const id = `s${i}0`;
      debounce(id, 120, () => sendCmd({ servoSettings: [id, Number(s0.value), String(i)] }));
    });

    s1.addEventListener("change", () => {
      const id = `s${i}1`;
      debounce(id, 120, () => sendCmd({ servoSettings: [id, Number(s1.value), String(i)] }));
    });

    s2.addEventListener("change", () => {
      const id = `s${i}2`;
      debounce(id, 120, () => sendCmd({ servoSettings: [id, Number(s2.value), String(i)] }));
    });

    return wrap;
  }

  function buildServoUI(count = 6) {
    servoContainer.innerHTML = "";
    for (let i = 0; i < count; i++) {
      servoContainer.appendChild(servoCard(i));
    }
  }

  // ---------- Apply state ----------
  function setText(el, v) {
    if (!el) return;
    const s = (v === null || typeof v === "undefined") ? "--" : String(v);
    if (el.textContent !== s) el.textContent = s;
  }

  function setChecked(el, v) {
    if (!el) return;
    const b = !!v;
    if (el.checked !== b) el.checked = b;
  }

  function setRange(el, v) {
    if (!el) return;
    const s = String(v ?? "");
    if (el.value !== s) el.value = s;
  }

  function cibleImg(n) {
    const x = Number(n);
    const safe = (x >= 0 && x <= 3) ? x : 0;
    return `/cible_${safe}.jpg`;
  }

  function applyState(s) {
    lastState = s;

    setText(stateEls.idNode, s.idNode);

    // voisins
    ["p00", "p01", "p10", "p11", "m00", "m01", "m10", "m11"].forEach(k => setText(stateEls[k], s[k]));

    // signaux / images
    if (typeof s.cibleHoraire !== "undefined") imgHoraire.src = cibleImg(s.cibleHoraire);
    if (typeof s.cibleAntiHor !== "undefined") imgAntiHor.src = cibleImg(s.cibleAntiHor);

    setText(stateEls.sensMarche, s.sensMarche);
    setText(stateEls.maxSpeedValue, s.maxSpeed);

    // toggles
    setChecked(wifiOnEl, s.wifi_on);
    setChecked(discoveryOnEl, s.discovery_on);

    // sliders
    if (typeof s.maxSpeed !== "undefined") setRange(maxSpeedRange, s.maxSpeed);

    if (typeof s.throttle !== "undefined") {
      setRange(throttleRange, s.throttle);
      setText(throttleVal, s.throttle);
    }

    // locomotive
    setText(locoEls.address, s.locoAddress);
    setText(locoEls.speed, s.locoSpeed);
    setText(locoEls.direction, s.locoDirection);

    // servos
    for (let i = 0; i < 6; i++) {
      const status = $(`s${i}_status`);
      const sActif = s[`s${i}`];      // "Actif" / "null"
      setText(status, sActif);

      const r0 = $(`s${i}0`), r1 = $(`s${i}1`), r2 = $(`s${i}2`);
      const v0 = $(`s${i}0v`), v1 = $(`s${i}1v`), v2 = $(`s${i}2v`);

      // quand "null", on désactive l’UI
      const enabled = (sActif !== "null" && sActif !== null && typeof sActif !== "undefined");
      [r0, r1, r2].forEach(r => { if (r) r.disabled = !enabled; });

      // valeurs
      if (typeof s[`s${i}0`] !== "undefined") { setRange(r0, s[`s${i}0`]); setText(v0, s[`s${i}0`]); }
      if (typeof s[`s${i}1`] !== "undefined") { setRange(r1, s[`s${i}1`]); setText(v1, s[`s${i}1`]); }
      if (typeof s[`s${i}2`] !== "undefined") { setRange(r2, s[`s${i}2`]); setText(v2, s[`s${i}2`]); }
    }
  }

  // ---------- UI events ----------
  function bindUI() {
    // build servo cards
    buildServoUI(6);

    btnSave.addEventListener("click", () => sendCmd({ save: ["save"] }));
    btnRestart.addEventListener("click", () => sendCmd({ restartEsp: ["restartEsp"] }));

    wifiOnEl.addEventListener("change", () => sendCmd({ wifi_on: [wifiOnEl.checked] }));
    discoveryOnEl.addEventListener("change", () => sendCmd({ discovery_on: [discoveryOnEl.checked] }));

    maxSpeedRange.addEventListener("input", () => {
      stateEls.maxSpeedValue.textContent = String(maxSpeedRange.value);
    });
    maxSpeedRange.addEventListener("change", () => {
      sendCmd({ maxSpeed: [Number(maxSpeedRange.value)] });
    });

    throttleRange.addEventListener("input", () => {
      throttleVal.textContent = String(throttleRange.value);
    });
    throttleRange.addEventListener("change", () => {
      // 5..120 pas 5 déjà garanti par le range
      sendCmd({ throttle: [Number(throttleRange.value)] });
    });
  }

  // ---------- start ----------
  function start() {
    bindUI();
    connectWs();

    // “soft keepalive” : si WS up, on peut envoyer un no-op ou juste laisser vivre
    // Ici on ne spam pas. Si tu veux, tu peux ajouter un ping JSON dédié.
    setInterval(() => {
      // si déconnecté, on retente
      if (!ws || ws.readyState === 3) connectWs();
    }, 2000);
  }

  window.addEventListener("load", start);
})();
