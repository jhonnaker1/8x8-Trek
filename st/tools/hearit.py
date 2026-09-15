#!/usr/bin/env python3
"""Does this build make a SOUND? Measured, not listened for.

THIS PORT SHIPPED SILENT AND EVERY CHECK PASSED, because tools/shot.py runs
Hatari with `--sound off` -- so every automated verification this port had was
a verification about the PICTURE. `make sndtest` proved the driver writes the
right values to the PSG; it could not prove anything reached a speaker.

Hatari records an AVI with an audio track. ffmpeg extracts it, and a peak
amplitude over a threshold is the answer. That is the same shape as the X16
port's octave-flat investigation, which recorded VICE to a WAV and measured
the pitch rather than trusting an ear.
"""
import argparse, os, signal, subprocess, sys, time, wave

HERE = os.path.dirname(os.path.abspath(__file__))
ST = os.path.dirname(HERE)
TOS = os.path.expanduser("~/hatari/rom/etos512us.img")


def send(fifo, line, timeout=12.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        if os.path.exists(fifo):
            try:
                fd = os.open(fifo, os.O_WRONLY | os.O_NONBLOCK)
                os.write(fd, (line + "\n").encode())
                os.close(fd)
                time.sleep(0.4)
                return True
            except OSError:
                pass
        time.sleep(0.25)
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--prg", default="EGATREK.PRG")
    ap.add_argument("--wait", type=float, default=16.0, help="seconds to boot")
    ap.add_argument("--record", type=float, default=10.0, help="seconds of audio")
    ap.add_argument("--out", default="/tmp/st-hear")
    a = ap.parse_args()

    os.makedirs(a.out, exist_ok=True)
    avi = os.path.join(a.out, "cap.avi")
    wav = os.path.join(a.out, "cap.wav")
    for p in (avi, wav):
        if os.path.exists(p):
            os.unlink(p)
    fifo = "/tmp/hatari-hear.fifo"
    if os.path.exists(fifo):
        os.unlink(fifo)

    cmd = ["hatari", "--machine", "st", "--tos", TOS, "--vdi", "off",
           "--memsize", "1", "--fast-boot", "on", "--confirm-quit", "off",
           "--gemdos-drive", "C", "-d", os.path.join(ST, "build"),
           "--cmd-fifo", fifo, "--avi-file", avi,
           "--auto", "C:\\" + a.prg]
    h = subprocess.Popen(cmd, stdout=open(os.path.join(a.out, "hatari.log"), "w"),
                         stderr=subprocess.STDOUT)
    try:
        time.sleep(a.wait)
        if not send(fifo, "hatari-shortcut recanim"):
            print("hearit: could not reach the fifo"); return 1
        time.sleep(a.record)
        send(fifo, "hatari-shortcut recanim")
        time.sleep(2.0)
    finally:
        send(fifo, "hatari-shortcut quit")
        try:
            h.wait(timeout=10)
        except Exception:
            h.send_signal(signal.SIGKILL); h.wait()

    if not os.path.exists(avi) or os.path.getsize(avi) < 1024:
        print("hearit: no AVI was written -- the recording never started, so "
              "nothing below would mean anything")
        return 1
    r = subprocess.run(["ffmpeg", "-y", "-i", avi, "-vn", "-acodec",
                        "pcm_s16le", wav], capture_output=True, text=True)
    if not os.path.exists(wav):
        print("hearit: the AVI has NO AUDIO TRACK:"); print(r.stderr[-400:])
        return 1

    with wave.open(wav) as w:
        n = w.getnframes()
        raw = w.readframes(n)
        sw, ch = w.getsampwidth(), w.getnchannels()
    import array
    a16 = array.array("h"); a16.frombytes(raw[:len(raw) // 2 * 2])
    peak = max(abs(v) for v in a16) if a16 else 0
    nz = sum(1 for v in a16 if abs(v) > 200)
    print("  %.1fs of audio, %d samples, %d channels" % (n / 44100.0, n, ch))
    print("  peak amplitude      %d of 32767" % peak)
    print("  samples over 200    %d (%.1f%%)" % (nz, 100.0 * nz / max(1, len(a16))))
    if peak < 500:
        print("hearit: SILENT -- nothing reached the speaker")
        return 1
    print("hearit: THERE IS SOUND")
    return 0


if __name__ == "__main__":
    sys.exit(main())
