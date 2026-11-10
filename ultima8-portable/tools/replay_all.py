#!/usr/bin/env python3
"""Replay every recording in a folder, headless, in parallel, and report.

  replay_all.py --exe build/port/Ultima8.exe --data <game data folder> [--jobs 4] [replays-dir]

Exit code 0 only if every recording ended with "replay finished" (all checkpoints matched).
Each replay gets its own scratch home folder; the game data is read only.
"""
import argparse, os, re, subprocess, sys, tempfile, time
from concurrent.futures import ThreadPoolExecutor

def run(exe, data, rec, extra):
    home = tempfile.mkdtemp(prefix="u8replay-")
    env = dict(os.environ, U8_DATA=data)
    t = time.time()
    p = subprocess.run([exe, "--home", home, "--replay", rec] + extra,
                       env=env, capture_output=True, text=True, errors="replace")
    out = p.stdout + p.stderr
    m = re.search(r"u8: replay (finished|diverged)[^\n]*", out)
    return {
        "name": os.path.basename(rec), "code": p.returncode, "secs": time.time() - t,
        "line": m.group(0) if m else out.strip().splitlines()[-1:] or "no output",
        "endgame": "ENDGAME" in out, "fingerprint": "data differs" in out,
    }

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("replays", nargs="?", default="replays")
    ap.add_argument("--exe", required=True)
    ap.add_argument("--data", required=True)
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--only", help="substring of the recording folder names to run")
    ap.add_argument("extra", nargs="*", help="more arguments for the port, after --")
    a = ap.parse_args()
    recs = sorted(os.path.join(a.replays, d) for d in os.listdir(a.replays)
                  if os.path.exists(os.path.join(a.replays, d, "SESSION.U8R")) and (not a.only or a.only in d))
    with ThreadPoolExecutor(a.jobs) as ex:
        rs = list(ex.map(lambda r: run(a.exe, a.data, r, a.extra), recs))
    bad = 0
    for r in rs:
        ok = r["code"] == 0
        bad += not ok
        print(f"{'ok  ' if ok else 'FAIL'} {r['name']}  {r['secs']:7.0f}s  exit {r['code']}  "
              f"{'ENDGAME ' if r['endgame'] else ''}{'(data differs) ' if r['fingerprint'] else ''}{r['line']}")
    sys.exit(1 if bad else 0)

main()
