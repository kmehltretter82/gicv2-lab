"""Archive and audit H6 only after all boots and boot-file restoration pass."""
from collections import Counter
from datetime import datetime
import hashlib
import json
from pathlib import Path
import shutil
import statistics
import sys
import tarfile
import tempfile
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[3]
SESSION = Path(__file__).resolve().parent
GATE = SESSION.parent / "h6/gate"
OUT = ROOT / "results/2026-09-05-h6-pi400-100"
sys.path.insert(0, str(ROOT / "tools"))
import h5_runner as h5
import h6_gate as h6

session = h5.load_json(SESSION / "h6-session.json")
assert session.get("finished_utc")
assert session["deployment_exit_status"] == session["driver_exit_status"] == 0
recovery = h5.load_json(SESSION / "h6-restoration-recovery.json")
assert recovery["exit_status"] == 0
state = h6.verify(GATE)
assert state["status"] == "complete" and state["runs_passed"] == state["runs_attempted"] == 100
archive = OUT / "complete-gate.tar.gz"
h6.archive(SimpleNamespace(gate=GATE, out=archive))
with tempfile.TemporaryDirectory(prefix="gicv2-h6-archive-audit-") as temporary:
    extracted = Path(temporary)
    with tarfile.open(archive) as packed:
        packed.extractall(extracted, filter="data")
    relocated = h6.verify(extracted / "gate")
    assert relocated == state

rows = []
raw_hashes = Counter()
for number in range(1, 101):
    directory = GATE / "runs/{:03d}".format(number)
    attempt = h5.load_json(directory / "attempt.json")
    result = h5.load_json(directory / "result.json")
    trace = h5.load_trace(directory / "h5/normalized.json")
    kinds = Counter(record["kind"] for record in trace["records"])
    assert kinds == {"marker": 25, "field": 221}
    raw = directory / "serial.log"
    digest = h5.sha256_file(raw)
    raw_hashes[digest] += 1
    elapsed = (datetime.fromisoformat(result["finished_utc"]) - datetime.fromisoformat(attempt["started_utc"])).total_seconds()
    rows.append({"run": number, "status": result["status"], "started_utc": attempt["started_utc"],
                 "finished_utc": result["finished_utc"], "attempt_seconds": elapsed,
                 "serial_bytes": raw.stat().st_size, "serial_sha256": digest})

for name in ("h6-session.py", "h6-session.json", "h6-deployment.log", "h6-restoration.log",
             "h6-driver.log", "remote-h6-deploy.sh", "remote-h6-restore.sh", "preflight.log",
             "transfer-commands.json", "tryboot-h6.txt", "finalize-h6.py",
             "h6-restoration-recovery.json", "h6-restoration-recovery.log"):
    shutil.copy2(SESSION / name, OUT / name)
shutil.copy2(SESSION / "boot-backup/SHA256SUMS", OUT / "original-boot-files.sha256")
shutil.copy2(GATE / "runs/001/serial.log", OUT / "serial-run-001.log")
shutil.copy2(GATE / "runs/100/serial.log", OUT / "serial-run-100.log")
manifest = h5.load_json(GATE / "gate.json")
negative = h5.load_json(OUT / "negative-control/comparison.json")
assert not negative["matches"]
validation = {**state, "archive_sha256": h5.sha256_file(archive), "relocated_archive_verification": relocated,
              "image_sha256": manifest["image_sha256"], "flat_image_sha256": manifest["flat_image_sha256"],
              "markers_per_capture": 25, "fields_per_capture": 221, "raw_trace_hash_counts": dict(raw_hashes),
              "original_boot_files_restored": True, "temporary_boot_image_removed": True,
              "negative_control_rejected": True, "started_utc": rows[0]["started_utc"],
              "initial_restoration_exit_status": session["restoration_exit_status"],
              "restoration_recovery_exit_status": recovery["exit_status"],
              "restoration_verified_utc": recovery["finished_utc"],
              "last_trace_finished_utc": rows[-1]["finished_utc"], "session_finished_utc": session["finished_utc"],
              "median_attempt_seconds_after_first": statistics.median(row["attempt_seconds"] for row in rows[1:]),
              "runs": rows}
h5.write_json(OUT / "validation.json", validation)
print(json.dumps({key: value for key, value in validation.items() if key != "runs"}, indent=2))
