"""Run the frozen H6 campaign, then restore the backed-up boot files."""
import datetime
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

SESSION = Path(__file__).resolve().parent
GATE = SESSION.parent / "h6/gate"
SSH = ["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=10", "karl@192.168.1.15"]
COMMAND = ["bash", str(GATE / "source/scripts/h6-repeat.sh"), "100", str(GATE)]
ENVIRONMENT = {"PI_HOST": "karl@192.168.1.15", "PORT": "/dev/cu.usbserial-B0043XBS",
               "PYTHON": "/opt/homebrew/bin/python3", "PYTHONDONTWRITEBYTECODE": "1"}


def now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def remote_script(name, log):
    with (SESSION / name).open("rb") as script:
        return subprocess.run(SSH + ["bash -se"], stdin=script, stdout=log,
                              stderr=subprocess.STDOUT, timeout=60).returncode


def interrupted(signum, frame):
    raise KeyboardInterrupt("received signal {}".format(signum))


signal.signal(signal.SIGTERM, interrupted)
state = {"started_utc": now(), "controller_pid": os.getpid(), "ssh": SSH,
         "driver_command": COMMAND, "environment": ENVIRONMENT,
         "deployment_script": "remote-h6-deploy.sh", "restoration_script": "remote-h6-restore.sh"}
state_path = SESSION / "h6-session.json"
with state_path.open("x") as output:
    json.dump(state, output, indent=2)
driver = None
result = 1
try:
    with (SESSION / "h6-deployment.log").open("xb") as log:
        state["deployment_exit_status"] = remote_script("remote-h6-deploy.sh", log)
    if state["deployment_exit_status"]:
        raise RuntimeError("H6 deployment failed; see h6-deployment.log")
    print("H6 deployed and verified; starting 100 fresh boots", flush=True)
    with (SESSION / "h6-driver.log").open("xb") as log:
        driver = subprocess.Popen(COMMAND, env={**os.environ, **ENVIRONMENT},
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                  start_new_session=True)
        state["driver_pid"] = driver.pid
        state_path.write_text(json.dumps(state, indent=2) + "\n")
        for line in driver.stdout:
            log.write(line)
            log.flush()
            sys.stdout.buffer.write(line)
            sys.stdout.buffer.flush()
        result = driver.wait()
        state["driver_exit_status"] = result
except BaseException as error:
    state["error"] = str(error)
    print("H6 session stopped: {}".format(error), flush=True)
finally:
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
    signal.signal(signal.SIGINT, signal.SIG_IGN)
    if driver is not None and driver.poll() is None:
        os.killpg(driver.pid, signal.SIGTERM)
        try:
            driver.wait(timeout=20)
        except subprocess.TimeoutExpired:
            os.killpg(driver.pid, signal.SIGKILL)
            driver.wait()
        state["driver_exit_status"] = driver.returncode
    print("Waiting for the vendor kernel to restore original boot files", flush=True)
    state["restoration_exit_status"] = None
    try:
        with (SESSION / "h6-restoration.log").open("xb") as log:
            deadline = time.monotonic() + 240
            while time.monotonic() < deadline:
                check = subprocess.run(SSH + ["true"], stdout=log, stderr=subprocess.STDOUT, timeout=15)
                if check.returncode == 0:
                    state["restoration_exit_status"] = remote_script("remote-h6-restore.sh", log)
                    break
                time.sleep(3)
    except BaseException as error:
        state["restoration_error"] = str(error)
    state["finished_utc"] = now()
    state_path.write_text(json.dumps(state, indent=2) + "\n")
    print(json.dumps(state, indent=2), flush=True)
    if state["restoration_exit_status"] != 0:
        result = 2
sys.exit(result)
