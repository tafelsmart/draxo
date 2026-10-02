import os, shutil, subprocess, sys, time
from datetime import datetime
from pathlib import Path

WORKDIR = Path(__file__).resolve().parent
BUILD_SCRIPT = WORKDIR / "tools" / "vanilla_builder.py"
MINECRAFT_DIR = Path(os.environ.get("APPDATA", "")) / ".minecraft"
CLIENT_LOG = WORKDIR / "build" / "vanilla" / "Release" / "draxo_client.log"
CRASH_TEST_LOG_DIR = WORKDIR / "build" / "vanilla" / "Release" / "crash_test_logs"
COOLDOWN = 15

def ts():
    return datetime.now().strftime("%Y%m%d_%H%M%S")

def log(msg):
    t = datetime.now().strftime("%H:%M:%S")
    print(f"[{t}] {msg}", flush=True)

def find_mc():
    try:
        import psutil
        bedrock = False
        for p in psutil.process_iter(["pid","name","cmdline"]):
            try:
                n = (p.info.get("name") or "").lower()
            except Exception:
                continue
            if n == "minecraft.windows.exe":
                bedrock = True
                continue
            if n not in ("javaw.exe","java.exe"):
                continue
            c = " ".join(p.info.get("cmdline") or []).lower()
            if "minecraft" in c or "net.minecraft.client.main" in c:
                return {"pid": p.info["pid"], "name": p.info["name"]}
        if bedrock:
            log("  WARN: Bedrock is running, but we need Java Edition (javaw.exe)!")
    except Exception:
        pass
    return None

def kill_mc():
    try:
        import psutil
        for p in psutil.process_iter(["pid","name","cmdline"]):
            try:
                n = (p.info.get("name") or "").lower()
            except Exception:
                continue
            if n in ("javaw.exe","java.exe"):
                c = " ".join(p.info.get("cmdline") or []).lower()
                if "minecraft" in c or "net.minecraft.client.main" in c:
                    try:
                        p.kill()
                    except Exception:
                        pass
            # Also kill Bedrock if it was accidentally launched
            elif n == "minecraft.windows.exe":
                try:
                    p.kill()
                    log("  Killed Bedrock process (PID={}) — we need Java Edition!".format(p.info["pid"]))
                except Exception:
                    pass
    except Exception:
        pass

def wait_mc(to=90):
    log("Waiting for MC (timeout={}s)...".format(to))
    t0 = time.time()
    while time.time() - t0 < to:
        p = find_mc()
        if p:
            log("Found: PID={}".format(p["pid"]))
            return p
        time.sleep(2)
    log("TIMEOUT: MC not found")
    return None

def wait_death(pid, to):
    t0 = time.time()
    while time.time() - t0 < to:
        try:
            import psutil
            if not psutil.Process(pid).is_running():
                return True
        except Exception:
            return True
        time.sleep(1)
    return False

def launch_mc(ver):
    log("Launching Minecraft JAVA {}...".format(ver))

    # ONLY search for Java Edition launcher paths.
    # The minecraft:// protocol opens Bedrock on many Windows 11 machines.
    java_launcher_paths = [
        # Official standalone launcher (most common for Java players)
        Path("C:/Program Files (x86)/Minecraft Launcher/MinecraftLauncher.exe"),
        # Xbox app installation (contains both Java + Bedrock, but this exe is the unified launcher)
        Path(os.environ.get("LOCALAPPDATA","")) / "Packages" / "Microsoft.4297127D64EC6_8wekyb3d8bbwe" / "Minecraft.exe",
        # Alternative Xbox path
        Path("C:/XboxGames/Minecraft Launcher/Content/MinecraftLauncher.exe"),
        # Prism Launcher (popular third-party)
        Path(os.environ.get("LOCALAPPDATA","")) / "Programs" / "PrismLauncher" / "prismlauncher.exe",
        Path("C:/Program Files/PrismLauncher/prismlauncher.exe"),
        # MultiMC
        Path("C:/Program Files/MultiMC/MultiMC.exe"),
    ]

    for exe in java_launcher_paths:
        if exe.exists():
            log("  Found launcher: {}".format(exe))
            try:
                subprocess.Popen([str(exe)])
                log("  Launcher opened. Select Java Edition + version {} and click Play.".format(ver))
                return True
            except Exception:
                pass

    log("  No Java launcher found. Start Minecraft JAVA Edition manually!")
    log("  (NOT Bedrock/Windows Edition — we need javaw.exe)")
    return False

def do_inject(ver):
    log("Building + injecting for {}...".format(ver))
    try:
        r = subprocess.run(
            [sys.executable, str(BUILD_SCRIPT), "--version", ver],
            cwd=str(WORKDIR), capture_output=True, text=True, timeout=400
        )
        if r.returncode == 0:
            log("SUCCESS")
            return True
        log("FAILED (code={})".format(r.returncode))
        for line in r.stderr.splitlines()[-5:]:
            log("  " + line)
        return False
    except subprocess.TimeoutExpired:
        log("TIMED OUT")
        return False
    except Exception as e:
        log("ERROR: " + str(e))
        return False

def collect(run):
    saved = []
    t = ts()
    CRASH_TEST_LOG_DIR.mkdir(parents=True, exist_ok=True)
    mc_dir = MINECRAFT_DIR
    if mc_dir.exists():
        hs = sorted(mc_dir.glob("hs_err_pid*.log"), key=lambda p: p.stat().st_mtime, reverse=True)
        if hs:
            f = hs[0]
            d = CRASH_TEST_LOG_DIR / "run{:02d}_{}_{}".format(run, t, f.name)
            try:
                shutil.copy2(f, d)
                saved.append(d.name)
                log("Saved: " + d.name)
            except Exception:
                pass
    if CLIENT_LOG.exists():
        d = CRASH_TEST_LOG_DIR / "run{:02d}_{}_draxo_client.log".format(run, t)
        try:
            shutil.copy2(CLIENT_LOG, d)
            saved.append(d.name)
            log("Saved: " + d.name)
        except Exception:
            pass
    return saved

def clean():
    if MINECRAFT_DIR.exists():
        for f in MINECRAFT_DIR.glob("hs_err_pid*.log"):
            try:
                f.unlink()
            except Exception:
                pass

def run_crash_test(version="1.21.11", runs=5, duration=180, cb=None):
    R = {"runs": runs, "crashed": 0, "survived": 0, "logs": []}
    CRASH_TEST_LOG_DIR.mkdir(parents=True, exist_ok=True)
    log("=" * 60)
    log("DRAXO CRASH TEST -- {} runs, {}s each | v{}".format(runs, duration, version))
    log("Logs: " + str(CRASH_TEST_LOG_DIR))
    log("=" * 60)
    for run in range(1, runs + 1):
        log(""); log("-" * 50)
        log("RUN {}/{}".format(run, runs))
        log("-" * 50)
        if cb:
            cb(run, runs, "starting", "Run {}/{}".format(run, runs))
        clean()
        kill_mc()
        time.sleep(3)
        launch_mc(version)
        if cb:
            cb(run, runs, "waiting_launch", "Start MC manually if needed")
        proc = wait_mc()
        if not proc:
            log("SKIP run {}: no MC".format(run))
            continue
        log("Waiting 60s for title screen...")
        if cb:
            cb(run, runs, "waiting_title", "Title screen (60s)...")
        time.sleep(60)
        if cb:
            cb(run, runs, "injecting", "Building + injecting...")
        if not do_inject(version):
            log("SKIP run {}: inject failed".format(run))
            kill_mc()
            time.sleep(COOLDOWN)
            continue
        if cb:
            cb(run, runs, "waiting", "Waiting {}s...".format(duration))
        log("Waiting {}s...".format(duration))
        crashed = wait_death(proc["pid"], duration)
        if cb:
            cb(run, runs, "collecting", "Collecting logs...")
        saved = collect(run)
        R["logs"].extend(saved)
        if crashed:
            log("CRASHED!")
            R["crashed"] += 1
            if cb:
                cb(run, runs, "crashed", "CRASHED ({} logs)".format(len(saved)))
        else:
            log("Survived -- no crash")
            R["survived"] += 1
            if cb:
                cb(run, runs, "survived", "No crash")
        kill_mc()
        if run < runs:
            log("Cooldown {}s...".format(COOLDOWN))
            if cb:
                cb(run, runs, "cooldown", "Cooldown {}s...".format(COOLDOWN))
            time.sleep(COOLDOWN)
    log("")
    log("=" * 60)
    log("COMPLETE: {} crashed, {} survived".format(R["crashed"], R["survived"]))
    log("Logs: " + str(CRASH_TEST_LOG_DIR))
    for f in R["logs"]:
        log("  " + f)
    log("=" * 60)
    return R

if __name__ == "__main__":
    import argparse
    p = argparse.ArgumentParser()
    p.add_argument("--version", default="1.21.11")
    p.add_argument("--runs", type=int, default=5)
    p.add_argument("--duration", type=int, default=180)
    a = p.parse_args()
    run_crash_test(a.version, a.runs, a.duration)
