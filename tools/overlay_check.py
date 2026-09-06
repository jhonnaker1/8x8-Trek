#!/usr/bin/env python3
"""Overlay checks that are the same on every port, because the hazard is.

Both ports load code into ONE window and run it there. That makes two mistakes
possible which no compiler, linker or test suite can see, and both of them are
fatal on the machine:

  * an overlay calling into a DIFFERENT overlay's window address -- the call
    lands on whatever is loaded rather than on what it names;
  * an overlay calling ovl_load() -- which overwrites the code making the call.

`core/overlay.h` states both as rules 2 and 3. This module is what enforces
them. It was C128-only until 2026-09-05, when the MEGA65 -- same architecture,
same window, same `.ovl_*` section names -- turned out to have no verify step
of any kind.

Everything here is driven from the linked ELF, so it is toolchain-independent
as long as the port uses llvm-mos and names its sections `.ovl_<id>`.
"""
import re
import subprocess


def overlay_spans(elf, objdump):
    """{section: [(start, end, symbol)]} for every function in an overlay."""
    syms = subprocess.run([objdump, "--syms", str(elf)],
                          capture_output=True, text=True).stdout
    spans, lo, hi = {}, None, None
    for ln in syms.splitlines():
        m = re.match(r"^([0-9a-f]{8})\s+\S*\s+F\s+(\.ovl_\w+)\s+([0-9a-f]{8})\s+(\S+)", ln)
        if not m:
            continue
        a0, sec, sz, nm = int(m.group(1), 16), m.group(2), int(m.group(3), 16), m.group(4)
        spans.setdefault(sec, []).append((a0, a0 + sz, nm))
        lo = a0 if lo is None else min(lo, a0)
        hi = max(hi or 0, a0 + sz)
    return spans, lo, hi, syms


def check_overlay_calls(elf, objdump, die, label="verify"):
    """No overlay may call INTO another overlay, or load one.

    THERE IS ONE WINDOW. Code in .ovl_X runs at the window base and so does
    code in .ovl_Y, so a call from one to the other lands on whatever is
    loaded rather than on what it names -- and an ovl_load() from inside an
    overlay overwrites the very code making the call. Neither is a link error,
    neither fails a test, and both die on the machine.

    FOUND THE HARD WAY 2026-08-29: fire_one_torpedo was moved to an overlay on
    its own, and it did `ovl_load(OVL_MSGS); report_nova(dmg)` to reach a
    callee an earlier pass had put in the msgs window. It built, it verified,
    all three suites passed, and it would have crashed the first time a torpedo
    hit a star. Caught by reading the call graph, which is not a thing to rely
    on twice.

    AN ADDRESS CANNOT NAME A SECTION HERE -- every overlay starts at the same
    address, so that address belongs to ten different functions at once. The
    question is not "who owns this address" but "does the CALLING section own
    it": a call inside .ovl_X to a window address is fine exactly when some
    symbol of .ovl_X covers it. The first draft of this check asked the first
    question and reported a call that was perfectly correct.
    """
    spans, lo, hi, syms = overlay_spans(elf, objdump)
    if not spans:
        return

    # ovl_load is RESIDENT, so its address is unambiguous -- and a call to it
    # from inside a window is always fatal.
    loader = None
    for ln in syms.splitlines():
        m = re.match(r"^([0-9a-f]{8})\s+\S*\s+F\s+\.text\s+[0-9a-f]{8}\s+ovl_load$", ln)
        if m:
            loader = int(m.group(1), 16)

    stray, loads = [], []
    for sec, own in sorted(spans.items()):
        d = subprocess.run([objdump, "-d", "--section=" + sec, str(elf)],
                           capture_output=True, text=True).stdout
        here = None
        for ln in d.splitlines():
            m = re.match(r"^\s*([0-9a-f]+)\s+<(\S+)>:", ln)
            if m:
                here = m.group(2)
                continue
            m = re.search(r"\b(jsr|jmp)\s+\$([0-9a-f]{4})\b", ln)
            if not m:
                continue
            tgt = int(m.group(2), 16)
            if loader is not None and tgt == loader:
                loads.append((sec, here))
                continue
            if not (lo <= tgt < hi):
                continue                                  # resident: fine
            if not any(a0 <= tgt < a1 for a0, a1, _ in own):
                elsewhere = sorted({s2 for s2, o2 in spans.items()
                                    for a0, a1, _ in o2 if a0 <= tgt < a1} - {sec})
                stray.append((sec, here, "$%04x" % tgt, ", ".join(elsewhere) or "nothing"))

    if loads:
        die("an overlay calls ovl_load, which overwrites the code making the\n"
            "         call. Move the callee into this window instead:\n"
            + "\n".join("         %s:%s" % l for l in loads))
    if stray:
        die("overlay code calls a window address its own section does not\n"
            "         own -- it will land on whatever is loaded:\n"
            + "\n".join("         %s:%s -> %s (lives in %s)" % t for t in stray))
    print(f"{label}: no overlay calls out of its own window -- ok")


def map_overlays(mapfile):
    """[(section, vma, lma, size)] read out of an lld map file."""
    ovl = []
    for line in mapfile.read_text().splitlines():
        parts = line.split()
        if len(parts) == 5 and parts[4].startswith(".ovl_"):
            vma, lma, size = (int(parts[i], 16) for i in range(3))
            ovl.append((parts[4], vma, lma, size))
    return ovl


def check_overlay_layout(mapfile, window, die, label="verify", reserve=0):
    """Every overlay runs at the window, loads from its OWN address, and fits.

    THE LOAD-ADDRESS HALF IS A REAL BUG THAT SHIPPED, and it was silent at
    every stage. Overlays deliberately share a RUN address -- that is the whole
    idea -- and the first version gave them all one staging region to take
    their LOAD addresses from. lld does not advance a region's pointer for a
    section that also carries an explicit run address, so both overlays came
    out at the same load address, occupied the same bytes of the ELF, and
    llvm-objcopy dumped the SAME image twice under two names. The link was
    silent, the extraction was silent, the disk looked right, and the game drew
    the hall of fame when it asked for the evaluation.

    `reserve` is bytes at the END of an image that are not the overlay's -- the
    MEGA65 writes its build stamp into the last slot's padding, so an overlay
    that grows to fill the window would have its final instructions overwritten
    by the stamp with nothing to say so.
    """
    ovl = map_overlays(mapfile)
    if not ovl:
        print(f"{label}: no overlays in this build")
        return

    vmas = {v for _, v, _, _ in ovl}
    if len(vmas) != 1:
        die("overlays do not share one run address: "
            + ", ".join("%s at $%04x" % (n, v) for n, v, _, _ in ovl))

    seen = {}
    for name, _, lma, _ in ovl:
        if lma in seen:
            die(f"{name} and {seen[lma]} both LOAD from ${lma:04x}, so they are\n"
                f"         the same bytes of the ELF and llvm-objcopy will dump\n"
                f"         one image twice. Give each overlay its own staging\n"
                f"         region in the linker script.")
        seen[lma] = name

    over = [(n, s) for n, _, _, s in ovl if s > window - reserve]
    if over:
        die("overlay does not fit the window (%d bytes, %d reserved at the end):\n"
            % (window, reserve)
            + "\n".join("         %s is %d bytes" % o for o in over))

    big = max(s for _, _, _, s in ovl)
    print("%s: %d overlays at $%04x, distinct load addresses, largest %d of %d bytes"
          % (label, len(ovl), ovl[0][1], big, window - reserve))


def check_resident_calls(objdir, objdump, die, label="verify", allow=("main",)):
    """RULE 4: only main() may call into an overlay from resident code.

    A resident->overlay call is the whole mechanism, so it cannot be banned --
    but it is safe ONLY when the caller has just loaded that overlay, and the
    load/call pairs all live in main(). Any OTHER resident function that calls
    an overlay function is a function whose correctness depends on which
    window happens to be loaded when someone calls it, which nothing states
    and nothing checks.

    FOUND THE HARD WAY 2026-09-06, on the first game ever played to the end:

        int16_t trek_score(void) {          /* resident */
            ScoreSheet s;
            trek_score_sheet(&s);           /* OVL_CODE("eval") */
            return s.total;
        }

    and main.c had `load_hof(); ui_hall_of_fame(name, level, trek_score());`.
    The window already held the hall of fame, the call went to the address
    trek_score_sheet has in the EVAL layout, the shorter hof image does not
    reach that far, and the CPU ran into unwritten window bytes. The C128 and
    MEGA65 carry the same call in the same order -- this was never an X16
    fault, only the first port anyone finished a game on.

    WHY THE OTHER TWO CHECKS MISS IT: check_overlay_calls asks what OVERLAY
    code calls, and the caller here is resident. And it cannot be asked of the
    final binary at all -- LTO inlines trek_score into main, at which point
    the call is indistinguishable from the thirteen legitimate ones. So this
    reads the call graph AS WRITTEN, from -fno-lto objects, before the
    optimiser folds the evidence away.
    """
    import glob
    import os

    sec_of = {}
    objs = sorted(glob.glob(os.path.join(str(objdir), "*.o")))
    if not objs:
        die("%s: no -fno-lto objects in %s -- rule 4 went unchecked" %
            (label, objdir))
    for o in objs:
        out = subprocess.run([objdump, "--syms", o],
                             capture_output=True, text=True).stdout
        for ln in out.splitlines():
            m = re.match(r"^([0-9a-f]{8})\s+\S*\s+F\s+(\.\S+)\s+[0-9a-f]{8}\s+(\S+)", ln)
            if m:
                sec_of[m.group(3)] = m.group(2)

    bad = set()
    for o in objs:
        d = subprocess.run([objdump, "-dr", o],
                           capture_output=True, text=True).stdout
        caller, sec = None, None
        for ln in d.splitlines():
            m = re.match(r"^Disassembly of section (\S+):", ln)
            if m:
                sec = m.group(1)
                continue
            m = re.match(r"^[0-9a-f]+\s+<(\S+)>:", ln)
            if m:
                caller = m.group(1)
                continue
            m = re.search(r"R_MOS\S*\s+(\S+)", ln)
            if not (m and caller and sec) or sec.startswith(".ovl."):
                continue
            target = m.group(1).split("+")[0]
            if sec_of.get(target, "").startswith(".ovl.") and caller not in allow:
                bad.add((os.path.basename(o), caller, target, sec_of[target]))

    if bad:
        for o, c, t, s in sorted(bad):
            print("%s: RESIDENT %s (%s) calls %s in %s" % (label, c, o, t, s))
        die("%s: rule 4 -- only %s may call into an overlay" %
            (label, "/".join(allow)))
    print("%s: rule 4 ok -- no resident caller outside %s reaches an overlay" %
          (label, "/".join(allow)))
