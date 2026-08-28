
## The settlers' distress call, and a deadline that started too early (2026-08-27)

`fn 0x151D0`, base 0x150C0, called from the turn loop at 0x005959. Its
strings: "COMMUNICATIONS: Planet ", "-", ", quad ", ", requests evacuation.
They can ", "only hold out until ", ".".

    if (stardate <= [0x1D9C]) return          ; the slot is not due
    [0x1D9C] = 9999                           ; disarm
    [0x1DA2] = stardate + 1.0 + Random*3.0    ; 0x01522A..0x015254
    print the message

and the slot itself is armed ONCE, at game start (0x00589B):

    [0x1D9C] = 3505.0 + Random*3.0

Both Randoms are the argument-less one, so both are one-sided: the call comes
between stardate 3505 and 3508, and the settlers then hold out between 1.0 and
4.0 stardates.

### What this core had wrong

`EVAC_WARNING_MIN_TENTHS 10` and `EVAC_WARNING_SPAN_TENTHS 30` were already
right -- one to four stardates. **But the clock started at game start**, so a
settlement could be lost by stardate 3501 where the original cannot lose one
before 3506. `planet_evac_end` is SCHED_NEVER until the call fires now, and
the call is a scheduled event of its own.

### The third slot is back, and so is a retired test

SCHED_DISTRESS takes slot 2 -- the one the death pod vacated when it turned
out not to be an event. "Two events due in the same window both fire, oldest
first" was retired this morning because the two remaining slots reschedule
each other; SCHED_DISTRESS is independent of both, so it is restored, with the
note explaining why it could come back.

### [0x1F31] identified in passing

`if ([0x1F31] == 'Y')` skips `fn 0x15F51` at game start (0x0058A6) and skips
the enemy's first turn in the loop before the loop forces it to 'N'. Both fit
one answer: **"Restore a saved game <Y/N>?"** -- restoring skips the opening
sequence and does not let the enemy shoot before the board is on screen. It
was recorded as "one of the setup's two Y/N answers, which is unread" this
morning.

### A THIRD harness blind spot, and the worst of the three

The extra schedule slot grew the save record by two bytes past
`TREK_SAVE_SIZE`, and `test_serial` **aborted** -- a buffer overrun. Checking
with `make test | grep -c "FAIL$"` reported **0 failures**, because a crash
prints no FAIL line. `make` itself returned non-zero the whole time and the
pipeline threw that away.

Today's three: `timeout` not existing on macOS (no output reads as no
failure), an RNG reseeded per iteration (measures the seeding), and now
grepping for FAIL while ignoring the exit status (a crash reads as a pass).
**Check the exit status, then read the output.**

### Cost

449 resident bytes, free 1,234 -> **785**. That is the largest single-feature
cost of the day for what looks like a small mechanic, and it is worth watching
before the next one.

## RAY built -- 25 of 25 commands (2026-08-27)

The last command. Its odds were read on 2026-08-26 and the one gap left then
was *"what each [misfire] does is NOT yet read"*. Read now, and both are
COSMETIC: `fn 0x70C0` prints its line and, on the arg-1 path only, draws a
screen effect and delays. Neither touches game state. The two variants differ
in pixels and nothing else.

    roll   handler   outcome                                      odds
    0      0x7026    "It worked!" -- every enemy here dies         1/6
    1, 3   0x70C0    "Death ray misfires."  (cosmetic, x2)         2/6
    2      0x71F5    mutants -- sets [0x26D1]                      1/6
    4, 5   0x7247    "The apparatus is going unstable!" SHIP LOST  2/6

Six rolls over four handlers: **it works one time in six and kills you one
time in THREE.**

### The mutants are a condition, not damage

`[0x26D1]` persists and the turn loop at 0x005B1A does the rest: one turn in
ten clears it, otherwise it prints one of five reports in a colour chosen by
`Random(6)+2`. **No mechanical penalty was found anywhere** -- it is a state
the ship carries. The five reports are Anderson's prose and are NOT
transcribed; this port writes its own five.

### Not built, and said plainly

The fatal outcome's **Top Secret loss memo** -- "Dept. of Space / EARTH
HEADQUARTERS / Top Secret ... destroyed by death ray explosion this stardate,
with loss of all aboard" -- is a screen this port does not have. Ending code 9
becomes an ordinary loss and the usual evaluation runs.

### Two test bugs, one of them mine twice over

The outcome-odds test counted 1,200 rolls into **uint8_t** counters, so two of
the four wrapped at 256 and read as the odds being wrong. Widened. And the
whole block was verified by breaking four ways -- equal odds, killing only one
cell, firing with no target, mutants that never clear -- each failing its own
assertions and nothing else.

### Cost

RAY and the mutants together: the command and its prose are in OVL_CMDS, and
resident free is 1,320. Save v11, 556 bytes. **BINARY 145.**
