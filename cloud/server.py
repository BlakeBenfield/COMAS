"""
COMAS cloud server — Team 18, CS147 (Architecture 2)

Run:            pip install flask && python server.py
Nodes POST:     /api/telemetry
Nodes GET:      /api/active_alert   (alert propagation between nodes)
Dashboard:      http://<host>:5000/
Analytics:      z-score anomaly detection per node/metric,
                labeled test events + confusion matrix (/api/confusion)

Deploy anywhere with a public IP (AWS EC2 / Azure VM free tier):
    python server.py   (listens on 0.0.0.0:5000)
"""

import sqlite3
import statistics
import time

from flask import Flask, g, jsonify, request

DB_PATH = "comas.db"
ALERT_ACTIVE_WINDOW_S = 60      # a node is "alarming" if it flagged within 60 s
ANOMALY_WINDOW = 20             # samples used for rolling mean/std
ANOMALY_Z = 3.0                 # |z| above this = anomaly

app = Flask(__name__)


# ----------------------------------------------------------------------------
# Database
# ----------------------------------------------------------------------------
def db():
    if "db" not in g:
        g.db = sqlite3.connect(DB_PATH)
        g.db.row_factory = sqlite3.Row
    return g.db


@app.teardown_appcontext
def close_db(_exc):
    d = g.pop("db", None)
    if d is not None:
        d.close()


def init_db():
    conn = sqlite3.connect(DB_PATH)
    conn.executescript(
        """
        CREATE TABLE IF NOT EXISTS telemetry (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            ts REAL NOT NULL,
            node_id INTEGER NOT NULL,
            co_raw INTEGER, methane_raw INTEGER,
            pm25 INTEGER, pm10 INTEGER,
            alarm INTEGER DEFAULT 0,
            anomaly INTEGER DEFAULT 0
        );
        CREATE TABLE IF NOT EXISTS events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            ts_start REAL NOT NULL,
            ts_end REAL,
            node_id INTEGER NOT NULL,
            label TEXT
        );
        """
    )
    conn.commit()
    conn.close()


# ----------------------------------------------------------------------------
# Anomaly detection: rolling z-score against the last ANOMALY_WINDOW samples
# ----------------------------------------------------------------------------
def detect_anomaly(node_id, co, ch4, pm25):
    rows = db().execute(
        "SELECT co_raw, methane_raw, pm25 FROM telemetry "
        "WHERE node_id=? ORDER BY id DESC LIMIT ?",
        (node_id, ANOMALY_WINDOW),
    ).fetchall()
    if len(rows) < 5:
        return 0
    for value, key in ((co, "co_raw"), (ch4, "methane_raw"), (pm25, "pm25")):
        series = [r[key] for r in rows if r[key] is not None]
        if len(series) < 5:
            continue
        mean = statistics.fmean(series)
        stdev = statistics.pstdev(series)
        if stdev < 1e-6:
            stdev = 1.0
        if abs(value - mean) / stdev >= ANOMALY_Z:
            return 1
    return 0


# ----------------------------------------------------------------------------
# API used by the ESP32 nodes
# ----------------------------------------------------------------------------
@app.post("/api/telemetry")
def post_telemetry():
    j = request.get_json(force=True)
    node_id = int(j["node_id"])
    co = int(j.get("co_raw", 0))
    ch4 = int(j.get("methane_raw", 0))
    pm25 = int(j.get("pm25", 0))
    pm10 = int(j.get("pm10", 0))
    alarm = int(j.get("alarm", 0))

    anomaly = detect_anomaly(node_id, co, ch4, pm25)
    db().execute(
        "INSERT INTO telemetry (ts,node_id,co_raw,methane_raw,pm25,pm10,alarm,anomaly) "
        "VALUES (?,?,?,?,?,?,?,?)",
        (time.time(), node_id, co, ch4, pm25, pm10, alarm, anomaly),
    )
    db().commit()
    return "ok"


@app.get("/api/active_alert")
def active_alert():
    """Plain-text node id of any node alarming in the last 60 s, else 0."""
    row = db().execute(
        "SELECT node_id FROM telemetry WHERE alarm>0 AND ts>? "
        "ORDER BY id DESC LIMIT 1",
        (time.time() - ALERT_ACTIVE_WINDOW_S,),
    ).fetchone()
    return str(row["node_id"] if row else 0)


# ----------------------------------------------------------------------------
# API used by the dashboard
# ----------------------------------------------------------------------------
@app.get("/api/history")
def history():
    node_id = int(request.args.get("node", 1))
    limit = int(request.args.get("limit", 200))
    rows = db().execute(
        "SELECT ts, co_raw, methane_raw, pm25, pm10, alarm, anomaly "
        "FROM telemetry WHERE node_id=? ORDER BY id DESC LIMIT ?",
        (node_id, limit),
    ).fetchall()
    return jsonify([dict(r) for r in rows][::-1])


@app.get("/api/nodes")
def nodes():
    rows = db().execute(
        "SELECT node_id, MAX(ts) AS last_ts FROM telemetry GROUP BY node_id"
    ).fetchall()
    out = []
    now = time.time()
    for r in rows:
        last = db().execute(
            "SELECT * FROM telemetry WHERE node_id=? ORDER BY id DESC LIMIT 1",
            (r["node_id"],),
        ).fetchone()
        out.append(
            {
                "node_id": r["node_id"],
                "online": now - r["last_ts"] < 60,
                "co_raw": last["co_raw"],
                "methane_raw": last["methane_raw"],
                "pm25": last["pm25"],
                "alarm": last["alarm"],
                "anomaly": last["anomaly"],
            }
        )
    return jsonify(out)


# ---- Ground-truth events for the confusion-matrix analytics ----------------
@app.post("/api/events")
def add_event():
    """Mark an induced test event: {"node_id":1,"label":"candle CO test",
    "duration_s":120}. Call this right when you start the test."""
    j = request.get_json(force=True)
    now = time.time()
    db().execute(
        "INSERT INTO events (ts_start, ts_end, node_id, label) VALUES (?,?,?,?)",
        (now, now + float(j.get("duration_s", 120)), int(j["node_id"]),
         j.get("label", "test")),
    )
    db().commit()
    return "ok"


@app.get("/api/compare")
def compare():
    """Room-to-room comparison: per-node mean/max over the last N samples,
    plus which node is currently worse per metric."""
    limit = int(request.args.get("limit", 100))
    out = {}
    for node_id in (1, 2):
        rows = db().execute(
            "SELECT co_raw, methane_raw, pm25 FROM telemetry "
            "WHERE node_id=? ORDER BY id DESC LIMIT ?",
            (node_id, limit),
        ).fetchall()
        if not rows:
            continue
        stats = {}
        for key in ("co_raw", "methane_raw", "pm25"):
            series = [r[key] for r in rows if r[key] is not None]
            stats[key] = {
                "mean": round(statistics.fmean(series), 1),
                "max": max(series),
            }
        out[f"node_{node_id}"] = stats
    if "node_1" in out and "node_2" in out:
        out["worse_room"] = {
            key: (1 if out["node_1"][key]["mean"] >= out["node_2"][key]["mean"] else 2)
            for key in ("co_raw", "methane_raw", "pm25")
        }
    return jsonify(out)


@app.get("/api/confusion")
def confusion():
    """TP/FP/FN/TN of the anomaly detector vs labeled event windows."""
    events = db().execute("SELECT * FROM events").fetchall()
    samples = db().execute("SELECT ts, node_id, anomaly FROM telemetry").fetchall()

    def in_event(s):
        return any(
            e["node_id"] == s["node_id"] and e["ts_start"] <= s["ts"] <= e["ts_end"]
            for e in events
        )

    tp = fp = fn = tn = 0
    for s in samples:
        truth, pred = in_event(s), bool(s["anomaly"])
        if truth and pred:
            tp += 1
        elif truth and not pred:
            fn += 1
        elif not truth and pred:
            fp += 1
        else:
            tn += 1
    total = max(tp + fp + fn + tn, 1)
    return jsonify(
        {"TP": tp, "FP": fp, "FN": fn, "TN": tn,
         "accuracy": round((tp + tn) / total, 3)}
    )


# ----------------------------------------------------------------------------
# Dashboard (single page, Chart.js from CDN)
# ----------------------------------------------------------------------------
DASHBOARD = """<!doctype html>
<html><head><meta charset="utf-8"><title>COMAS Dashboard</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4"></script>
<style>
  body{font-family:system-ui,sans-serif;margin:24px;background:#0f1420;color:#e8ecf4}
  h1{margin:0 0 4px} .sub{color:#8fa0b8;margin-bottom:20px}
  .cards{display:flex;gap:16px;flex-wrap:wrap;margin-bottom:24px}
  .card{background:#1a2233;border-radius:12px;padding:16px 20px;min-width:220px}
  .card.alarm{background:#5c1a1a;animation:pulse 1s infinite}
  .card.anom{outline:2px solid #e6b422}
  @keyframes pulse{50%{opacity:.75}}
  .big{font-size:1.6em;font-weight:700}
  .ok{color:#5dd39e}.bad{color:#ff6b6b}.dim{color:#8fa0b8;font-size:.85em}
  canvas{background:#1a2233;border-radius:12px;padding:12px;margin-bottom:20px}
</style></head><body>
<h1>COMAS &mdash; Carbon Monoxide &amp; Methane Alert System</h1>
<div class="sub">Team 18 &middot; CS147 &middot; live node telemetry, alerts and anomaly flags</div>
<div class="cards" id="cards"></div>
<canvas id="co" height="90"></canvas>
<canvas id="ch4" height="90"></canvas>
<canvas id="pm" height="90"></canvas>
<script>
function lineChart(id, title, sets){
  return new Chart(document.getElementById(id), {
    type:'line', data:{labels:[],datasets:sets},
    options:{animation:false,scales:{x:{ticks:{color:'#8fa0b8'}},y:{ticks:{color:'#8fa0b8'}}},
      plugins:{legend:{labels:{color:'#e8ecf4'}},title:{display:true,text:title,color:'#e8ecf4'}}}
  });
}
const coChart = lineChart('co', 'Carbon monoxide (raw ADC)', [
  {label:'Node 1 CO',data:[],borderColor:'#ff6b6b',tension:.3},
  {label:'Node 2 CO',data:[],borderColor:'#ff9e9e',borderDash:[6,4],tension:.3}]);
const ch4Chart = lineChart('ch4', 'Methane / combustible gas (raw ADC)', [
  {label:'Node 1 CH4',data:[],borderColor:'#e6b422',tension:.3},
  {label:'Node 2 CH4',data:[],borderColor:'#f4d97a',borderDash:[6,4],tension:.3}]);
const pmChart = lineChart('pm', 'Particulates PM2.5 (ug/m3)', [
  {label:'Node 1 PM2.5',data:[],borderColor:'#5dd39e',tension:.3},
  {label:'Node 2 PM2.5',data:[],borderColor:'#a8e6cf',borderDash:[6,4],tension:.3}]);

function fmtTs(ts){const d=new Date(ts*1000);return d.toLocaleTimeString();}

async function refresh(){
  const nodes = await (await fetch('/api/nodes')).json();
  document.getElementById('cards').innerHTML = nodes.map(n=>`
    <div class="card ${n.alarm?'alarm':''} ${n.anomaly?'anom':''}">
      <div class="big">Node ${n.node_id} ${n.alarm?'&#9888; ALARM':(n.online?'<span class=ok>&#9679; online</span>':'<span class=bad>&#9679; offline</span>')}</div>
      <div>CO raw: <b>${n.co_raw}</b> &middot; CH4 raw: <b>${n.methane_raw}</b></div>
      <div>PM2.5: <b>${n.pm25}</b> ug/m3</div>
      <div class="dim">${n.anomaly?'anomaly flagged on last sample':'no anomaly'}</div>
    </div>`).join('') || '<div class="card">No data yet - waiting for nodes...</div>';

  const h1 = await (await fetch('/api/history?node=1&limit=100')).json();
  const h2 = await (await fetch('/api/history?node=2&limit=100')).json();
  const labels = (h1.length>=h2.length?h1:h2).map(r=>fmtTs(r.ts));
  coChart.data.labels = labels;
  coChart.data.datasets[0].data = h1.map(r=>r.co_raw);
  coChart.data.datasets[1].data = h2.map(r=>r.co_raw);
  coChart.update();
  ch4Chart.data.labels = labels;
  ch4Chart.data.datasets[0].data = h1.map(r=>r.methane_raw);
  ch4Chart.data.datasets[1].data = h2.map(r=>r.methane_raw);
  ch4Chart.update();
  pmChart.data.labels = labels;
  pmChart.data.datasets[0].data = h1.map(r=>r.pm25);
  pmChart.data.datasets[1].data = h2.map(r=>r.pm25);
  pmChart.update();
}
refresh(); setInterval(refresh, 5000);
</script></body></html>"""


@app.get("/")
def dashboard():
    return DASHBOARD


if __name__ == "__main__":
    init_db()
    import os
    app.run(host="0.0.0.0", port=int(os.environ.get("PORT", 5000)))
