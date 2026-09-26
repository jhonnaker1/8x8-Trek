#!/usr/bin/env python3
"""A static bound on the Z80 stack the game can use, from SDCC's own .asm.

Under MSX-DOS the stack grows down from the top of the TPA into whatever the
image leaves, so it comes out of the same bytes as every driver,
and nobody had measured it on this compiler. The game cannot RUN yet (no
keyboard, storage or far memory), so a sentinel fill is not available; this
is the half that can be done now.

Per function, a small dataflow over the instructions: the depth pushed since
entry at every point, carried to each local label by the jumps that reach it
and iterated until it stops changing. Then the call graph from main():

    need(f) = max( deepest point in f,
                   depth at each `call g` + 2 + need(g),
                   depth at each `jp g` (a tail call) + need(g) )

WHAT IT CANNOT SEE, and says so rather than guessing:
  * calls through a pointer -- it fails if it finds one;
  * recursion -- it fails if the call graph has a cycle;
  * SDCC's runtime helpers (__mulint, __divuint, ...) -- no source here, so
    they are charged LIB bytes each and listed;
  * the BIOS: calls to a numeric address (CALSLT at $001C) are charged
    nothing here and LISTED, because what the BIOS and its interrupt handler
    push onto OUR stack is a dynamic measurement, not a static one.

Usage: stackdepth.py <entry> [--at f1,f2,...] file.asm ...

--at reports the deepest stack at which each named function is ENTERED from
<entry> -- where a BIOS call begins, which is what the dynamic figures from
STKTEST.COM are added to. """
import re, sys

LIB = 16            # per runtime helper: they push a few registers at most

# UNDER sdcccall(1) THE CALLEE POPS ITS OWN STACK ARGUMENTS -- `pop hl; pop
# af; jp (hl)` is a return that also drops two bytes of arguments. The first
# version of this tool counted the caller's pushes and never saw them come
# off, and put _ui_info_panel at 135 bytes when it allocates 72 + IX. So each
# function's cleanup is read from its own exits (the depth left below zero,
# less the return address), and the whole program is iterated until those
# settle. The runtime has no source here: these two take their third
# argument on the stack, and the call sites show the caller never pops it.
LIB_CLEAN = {"___memcpy": 2, "_strncpy": 2}

entry, files, at = sys.argv[1], sys.argv[2:], []
if files and files[0] == "--at":
    at, files = files[1].split(","), files[2:]

funcs = {}          # name -> list of (op, args) in order
area = None
cur = None
for path in files:
    for raw in open(path, errors="replace"):
        line = raw.split(";", 1)[0].rstrip()
        if not line.strip():
            continue
        m = re.match(r"^\s*\.area\s+(\S+)", line)
        if m:
            area = m.group(1)
            cur = None
            continue
        if line.lstrip().startswith("."):
            continue
        m = re.match(r"^([A-Za-z_][\w$]*)::?$", line.strip()) if not line[0].isspace() else None
        if m:
            name = m.group(1)
            if area in ("_CODE", "_HOME"):
                if re.match(r"^\d+\$$", name):          # a local label
                    funcs[cur].append(("label", name))
                else:
                    cur = name
                    funcs.setdefault(cur, [])
            continue
        m = re.match(r"^(\d+\$):\s*(.*)$", line.strip())
        if m and cur:
            funcs[cur].append(("label", m.group(1)))
            line = "\t" + m.group(2)
            if not m.group(2):
                continue
        if cur is None or area not in ("_CODE", "_HOME"):
            continue
        parts = line.strip().split(None, 1)
        op = parts[0].lower()
        args = [a.strip() for a in parts[1].split(",")] if len(parts) > 1 else []
        funcs[cur].append((op, args))

def is_local(a):
    return re.match(r"^\d+\$$", a) is not None

UNCOND = {"reti", "retn", "halt"}

cleanup = {}

def analyse(name):
    """Returns (deepest, calls, clean): calls is a list of (depth, target,
    tail), and clean the argument bytes this function pops on the way out."""
    body = funcs[name]
    at_label = {}
    for _ in range(8):
        changed = False
        depth, ix_depth, last_const = 0, None, {}
        deepest, calls, clean = 0, [], 0
        for op, args in body:
            if op == "label":
                lab = args
                if depth is None:
                    depth = at_label.get(lab)
                elif lab not in at_label or at_label[lab] < depth:
                    at_label[lab] = depth
                    changed = True
                continue
            if depth is None:           # unreachable until a jump says otherwise
                continue
            deepest = max(deepest, depth)
            if op == "push":
                depth += 2
            elif op == "pop":
                depth -= 2
            elif op == "dec" and args == ["sp"]:
                depth += 1
            elif op == "inc" and args == ["sp"]:
                depth -= 1
            elif op == "ld" and len(args) == 2 and args[0] in ("hl", "iy") and args[1].startswith("#"):
                try:
                    last_const[args[0]] = int(args[1][1:], 0)
                except ValueError:
                    last_const.pop(args[0], None)
            elif op == "ld" and len(args) == 2 and args[0] in ("hl", "iy") :
                last_const.pop(args[0], None)
            elif op == "add" and len(args) == 2 and args[1] == "sp" and args[0] in ("hl", "iy"):
                last_const["sp+" + args[0]] = last_const.get(args[0])
            elif op == "add" and args == ["ix", "sp"]:
                ix_depth = depth
            elif op == "ld" and len(args) == 2 and args[0] == "sp":
                src = args[1]
                if src == "ix":
                    if ix_depth is None:
                        sys.exit("%s: ld sp,ix with no frame" % name)
                    depth = ix_depth
                else:
                    k = last_const.get("sp+" + src)
                    if k is None:
                        sys.exit("%s: ld sp,%s from an unknown value" % (name, src))
                    depth -= k
            elif op == "ex" and args and args[0] == "(sp)":
                pass
            elif op == "call":
                tgt = args[-1]
                if tgt == "___sdcc_enter_ix":      # push ix; ix = sp
                    deepest = max(deepest, depth + 4)
                    depth += 2
                    ix_depth = depth
                elif tgt.startswith("___sdcc_call"):
                    sys.exit("%s: a call through a pointer (%s) -- the bound cannot see it" % (name, tgt))
                else:
                    calls.append((depth, tgt, False))
                    depth -= cleanup.get(tgt, LIB_CLEAN.get(tgt, 0))
            elif op in ("jp", "jr", "djnz"):
                tgt = args[-1]
                cond = len(args) == 2 or op == "djnz"
                if tgt.startswith("("):
                    if depth < 0:                   # a return that drops arguments
                        clean = max(clean, -depth - 2)
                    depth = None                    # ... or a switch table
                    continue
                if is_local(tgt):
                    if tgt not in at_label or at_label[tgt] < depth:
                        at_label[tgt] = depth
                        changed = True
                else:
                    calls.append((depth, tgt, True))
                if not cond:
                    depth = None
            elif op in UNCOND or (op == "ret" and not args):
                depth = None
            elif op == "ret":
                pass                                # conditional return
        if not changed:
            break
    return deepest, calls, clean

for _ in range(20):
    info = {f: analyse(f) for f in funcs}
    new = {f: info[f][2] for f in funcs}
    if new == cleanup:
        break
    cleanup = new
else:
    sys.exit("argument cleanup did not settle")
libs, bios = set(), set()
memo, stack = {}, []

def need(f):
    if f in memo:
        return memo[f]
    if f in stack:
        sys.exit("RECURSION: " + " -> ".join(stack[stack.index(f):] + [f]))
    if f not in funcs:
        if re.match(r"^(0x[0-9a-fA-F]+|\d+)$", f):
            bios.add(f)
            return 0, [f]
        libs.add(f)
        return LIB, [f]
    stack.append(f)
    deepest, calls, _ = info[f]
    best, path = deepest, [f]
    for depth, tgt, tail in calls:
        n, p = need(tgt)
        n += depth + (0 if tail else 2)
        if n > best:
            best, path = n, [f] + p
    stack.pop()
    memo[f] = (best, path)
    return memo[f]

total, path = need(entry)
print("static stack bound from %s: %d bytes (+2 for crt0's call)" % (entry, total))
print("deepest path:")
for f in path:
    print("  %-28s frame %4s" % (f, info[f][0] if f in info else "LIB" if f in libs else "BIOS"))
if libs:
    print("runtime helpers charged %d bytes each: %s" % (LIB, " ".join(sorted(libs))))
if bios:
    print("BIOS calls charged NOTHING here -- measure them: %s" % " ".join(sorted(bios)))
frames = sorted(((info[f][0], f) for f in info), reverse=True)[:8]
print("largest single frames: " + ", ".join("%s %d" % (f, d) for d, f in frames))

# Deepest entry to each --at function: the max over every call path from the
# entry of the stack already in use when it is called (return address in).
entered = {}
def walk(f, d, seen):
    if f not in funcs or f in seen:
        return
    for depth, tgt, tail in info[f][1]:
        e = d + depth + (0 if tail else 2)
        if tgt in at and e > entered.get(tgt, -1):
            entered[tgt] = e
        walk(tgt, e, seen | {f})
if at:
    walk(entry, 0, frozenset())
    for f in at:
        print("deepest entry to %-18s %s" % (f, entered[f] if f in entered else "never called"))
