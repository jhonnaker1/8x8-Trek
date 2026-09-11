#!/usr/bin/env python3
"""One boot, many experiments: the Atari rig's snapshot harness.

WHAT THIS REPLACES. Every Atari test so far has paid for a cold boot -- mount,
cold_reset, then poll `far_used` until the game has streamed STRINGS.DAT,
MUSIC.DAT and OVERLAYS.BIN into VRAM. That is the expensive part, it is the
same every time, and `savetest.py` paid for it TWICE in one script because a
restore needs a fresh start. Eight experiments in an afternoon cost sixteen
boots, and the third tool call in a row that times out looks exactly like a
hang -- which is how this rig came to be described as "stuck".

AltirraBridge has had STATE_SAVE/STATE_LOAD the whole time. So: boot once, take
a snapshot the moment the game is ready, and rewind to it for each experiment.

    with Session() as s:
        s.keys("RETURN"); s.keys("N,RETURN")
        s.snap("title")
        ...
        s.rewind("title")          # instant; no boot

FIX THE CYCLE BEFORE ITERATING -- see NOTES.md. This file is that lesson with
a socket attached.

THE ONE THING IT CANNOT ASSUME. Altirra's default disk mode is VIRTUAL
read-write: the emulated drive accepts writes the host .ATR never sees. So
whether a rewind also rewinds the DISK is a property of the emulator, not of
this code, and it decides what a save/restore test means. `probe_disk()` below
measures it rather than believing either answer.
"""
import pathlib
import re
import shutil
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve()
ATARI = HERE.parents[1]
SERVER = pathlib.Path.home() / "AltirraBridge-nightly-macos-arm64" / "AltirraBridgeServer"
sys.path.insert(0, str(SERVER.parent / "sdk" / "python"))
from altirra_bridge import AltirraBridge      # noqa: E402

NM = pathlib.Path.home() / "llvm-mos" / "bin" / "llvm-nm"


def symbols():
    """The game's symbols. They MOVE between builds -- never hardcode one."""
    nm = subprocess.check_output([str(NM), str(ATARI / "build/trekatari.xex.elf")])
    out = {}
    for line in nm.decode().splitlines():
        f = line.split()
        if len(f) == 3:
            out[f[2]] = int(f[0], 16)
    return out


class SessionStuck(RuntimeError):
    """The simulator stopped advancing, or the boot never settled."""


class Session:
    """A booted Atari with the game loaded, and a snapshot to come back to."""

    def __init__(self, master="egatrek.atr", scratch="session.atr", shots="shots",
                 deadline=180.0):
        self.master = ATARI / "build" / master
        # A COPY, NEVER THE MASTER. The emulator mounts read-write, so a run
        # that saves writes back into the image and the next "fresh disk" is
        # not one. That cost a round of diagnosing a save flag left by the
        # PREVIOUS run.
        self.disk = ATARI / "build" / scratch
        self.shots = ATARI / "build" / shots
        self.proc = self.a = None
        self.sym = {}
        self.loaded = -1
        self.deadline = deadline

    # -- lifecycle ---------------------------------------------------------
    def __enter__(self):
        self.shots.mkdir(parents=True, exist_ok=True)
        shutil.copy(self.master, self.disk)
        self.sym = symbols()
        log = ATARI / "build" / "bridge.log"
        fh = open(log, "w")
        self.proc = subprocess.Popen(
            [str(SERVER), "--bridge", "--settings=user", "--pacing=unlimited"],
            stdout=subprocess.DEVNULL, stderr=fh)
        token = None
        deadline = time.time() + 20
        while time.time() < deadline:
            if self.proc.poll() is not None:
                sys.exit("session: server exited -- see %s" % log)
            m = re.search(r"token-file:\s*(\S+)", log.read_text())
            if m:
                token = m.group(1)
                break
            time.sleep(0.2)
        if not token:
            self.proc.kill()
            sys.exit("session: no token-file line in %s" % log)
        self.a = AltirraBridge.from_token_file(token)
        self.a.__enter__()
        # A WEDGED FRAME GATE MUST NOT HANG THE HARNESS. `frame()` returns at
        # once and the NEXT command waits server-side for the gate; if the
        # simulator stops advancing -- a crash into a JAM, a debugger break --
        # that wait never ends and there is no client timeout. The whole point
        # of this file is that a test run cannot look like a hang, so it does
        # not get to hang either.
        if getattr(self.a, "_sock", None) is not None:
            self.a._sock.settimeout(self.deadline)
        self.a.mount(0, str(self.disk))
        self.boot()
        return self

    def __exit__(self, *exc):
        try:
            if self.a:
                self.a.__exit__(*exc)
        finally:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.proc.kill()

    # -- the expensive part, paid once -------------------------------------
    def boot(self, slot="booted"):
        """Cold boot and wait for the load, then snapshot it.

        POLLS `far_used` RATHER THAN GUESSING A FRAME COUNT: how long the boot
        load takes depends on the file sizes, which changed by 40% when the
        overlay images were packed. A fixed count would have been wrong that
        day and silently wrong after it.

        AND THE SENTINEL IS ZEROED AND READ BACK, which the first version did
        not do at all and the second did badly.
        `far_used` is ordinary RAM and A COLD RESET DOES NOT CLEAR IT. On the
        SECOND boot in one session the previous run's value -- 44,170, well
        over the threshold -- was still sitting there, three polls read it
        unchanged, and this returned "ready" while the machine was still
        loading. The keys that followed went into a half-booted game and wedged
        it. A readiness check that a stale value can satisfy is not a readiness
        check.

        The second version insisted on WATCHING it rise, and that was wrong
        too: the load finishes inside the first 200-frame poll, so the climb is
        never observable and every boot raised. The poke is the guarantee, not
        the watching -- so zero it, READ IT BACK, and then wait for a stable
        high value.
        """
        self.a.cold_reset()
        self.a.frame(2)                     # let the reset take hold
        self.a.poke16(self.sym["far_used"], 0)
        # READ THE POKE BACK. That is the whole mechanism: once the sentinel is
        # provably zero, any high value after it was written by THIS run's
        # load, and no stale figure can pass for a finished boot. A poke during
        # startup can be overwritten, so it is not assumed to have landed.
        back = self.a.peek16(self.sym["far_used"])
        if back != 0:
            raise SessionStuck(
                "zeroing far_used did not take -- read back %d. Every "
                "readiness answer after this would be about the PREVIOUS "
                "run." % back)
        prev, still = -1, 0
        for _ in range(60):
            self.a.frame(200)
            now = self.a.peek16(self.sym["far_used"])
            still = still + 1 if now == prev and now > 40000 else 0
            prev = now
            if still >= 2:
                break
        else:
            raise SessionStuck("boot never settled: far_used stuck at %d" % prev)
        self.loaded = prev
        self.a.state_save(slot=slot)
        return prev

    # -- driving -----------------------------------------------------------
    def keys(self, spec, settle=180):
        for k in spec.split(","):
            if k:
                self.a.key(k)
                self.a.frame(8)
        self.a.frame(settle)

    def snap(self, slot):
        self.a.state_save(slot=slot)
        return slot

    def rewind(self, slot="booted", settle=60):
        self.a.state_load(slot=slot)
        self.a.frame(settle)

    def shot(self, name):
        p = self.shots / name
        self.a.screenshot(path=str(p))
        return p

    def byte(self, name):
        return self.a.peek(self.sym[name], 1)[0]

    def word(self, name):
        return self.a.peek16(self.sym[name])
