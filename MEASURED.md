# Measured against the original

Values read off EGA Trek v3.0 running in DOSBox-X. These supersede anything
marked PROVISIONAL in `core/trek.h`, and anything inferred from the 1978
Super Star Trek listing in `reference/combat-model.md`.

## Method, and why there is no debugger in it

The plan had been to breakpoint `EGATREK_unpacked.exe`. **The DOSBox-X cask
build has no debugger compiled in** — no `BPINT`/`MEMDUMP`/`BPLIST` strings, no
`-debug` in its help — so that would mean building DOSBox-X from source.

It turned out not to be needed. **The game reports its own numbers.** The
`E` command prints exact energy figures, the status panel carries the stardate
and enemy count, combat prints damage values, and quitting prints a fully
itemised score sheet. The original is a self-instrumenting oracle: the task is
reading its screen, not its memory.

Sessions are collaborative — a Claude Code session on this host cannot
screenshot an emulator (see NOTES.md), so Jamie drives and reports.

## Session 1 — 2026-08-15, command level 3

### Starting state

| Quantity | Value |
|---|---|
| Stardate | 3500.0 |
| Warp factor | **1.0** |
| Mongols (level 3) | **40** |
| Main energy | **5000**, and that is its maximum |
| Impulse engines | **500**, and that is its maximum |
| Shields | **2500**, and that is its maximum |
| Status | Green |

**Energy lives in three separate pools**, not one, and the `E` command diverts
between them. Every pool starts at its own maximum.

The percentage beside each figure is **charge, not state of repair** — 4500 of
5000 reads 90%, 2000 of 2500 reads 80%. Damage is shown by colouring the row
red instead. So those three maxima are confirmed, not inferred.

There is **no mission deadline** shown anywhere on the panel. Our
`stardate_end` is invented and remains unverified.

### Energy transfer destroys overflow

Diverting 500 from main into shields that were already full left main at 4500
and shields still at 2500. The energy was simply gone. The binary carries
strings for exactly this — `ILLOGICAL` and `ENERGY LOST` — which we had
extracted long before seeing it happen.

Total energy is **not conserved** across a careless transfer. Since every pool
starts full, *any* transfer at the opening of a game destroys what it moves.

Implemented as `trek_divert()`, with tests reproducing the observed case.

### Scoring — the complete rubric

Quitting prints an itemised evaluation. Every weight is given explicitly:

| Item | Weight |
|---|---|
| Rescues | +200 each |
| Mongols killed | +10 each |
| Commanders killed | +20 each |
| Enemy bases destroyed | +50 each |
| Kill/day ratio | +500 per day |
| Stars destroyed | −5 each |
| Bases hit | −200 each |
| Penalty for incomplete mission | −300 (flat) |
| Casualties on board | weight unknown — the count was 0, so no figure was shown |

Quitting immediately at stardate 3500.0 with nothing done scores **−300**,
which is the incomplete-mission penalty alone. That checks out.

**This supersedes the Super Star Trek model** in `reference/combat-model.md`,
which had scoring as `1000 × (kills/stardates)²`. EGA Trek's kill-rate term is
**linear at 500 per day**, and it sits alongside seven itemised bonuses and
penalties the 1978 game had no equivalent of. The ancestor was the right place
to start, and is now the wrong place to finish.

Note the kill/day term rewards finishing fast, while the flat −300 punishes not
finishing at all — so an aggressive short game and a thorough long one are both
viable, which is a more interesting shape than the ancestor's pure rate.

### Enemy count per level — UNRESOLVED

Read this section in order; the first model it proposes is contradicted by the
readings that follow it, and the conclusion is that we do not yet know.

One new game per command level:

| Level | Mongols | minus level×10 |
|---|---|---|
| 1 | 18 | 8 |
| 2 | 32 | 12 |
| 3 | 40 | 10 |
| 4 | 42 | 2 |
| 5 | 53 | 3 |

The raw counts are not a straight line — level 4 only just exceeds level 3 —
so the count carries a **random component**. Subtracting a `level × 10` base
leaves small, non-negative remainders that show no trend with level:

```
enemies = level * 10 + random(0..~12)
```

**Fitted, not confirmed.** Five samples fix the base convincingly, since every
reading lands at or above `level × 10`, but they only bound the spread from
below — the true upper limit may exceed the 12 we happened to see.

The core test asserts the *band* rather than a single number, across all five
levels and 199 seeds. Asserting an exact count would be asserting a fiction.

#### …and then three level-3 games in a row all gave 42

Which contradicts the model above. Under `level × 10 + rand(0..12)`, three
consecutive identical draws is roughly a 1-in-169 event. The count is very
probably **not drawn afresh per game**.

But it does not simply resolve to "deterministic per level" either:

| Level | Readings |
|---|---|
| 1 | 18 |
| 2 | 32 |
| 3 | **40**, then **42, 42, 42** |
| 4 | 42 |
| 5 | 53 |

Level 3 gave two different answers, and levels 3 and 4 both gave 42 — an odd
shape for a difficulty ladder. Gaps of 14, 10, 0, 11 are no more regular than
the random reading was.

**Leading hypothesis:** the seed is fixed for the lifetime of the *program*,
not the game, so games started without leaving EGA Trek reproduce the same
galaxy, while a fresh launch draws again. That would explain 42 three times in
one sitting and 40 in an earlier one.

**Discriminating experiment**, not yet run:

1. Quit all the way out to DOS. Relaunch. Play level 3, note the count.
2. Without exiting, start a second level-3 game. Note it.
3. Quit to DOS again, relaunch, level 3 once more.

If readings 1 and 2 match but 3 differs, the seed is per-launch. If all three
match, something else is going on and `level × 10` is wrong too.

**The code has not been changed on this evidence.** The current model produces
counts consistent with every reading taken so far (42 falls inside level 3's
band and level 4's), so nothing is failing — but its *shape* is in doubt, and
churning it on ambiguous data would be worse than leaving it flagged.

### Ranks

The Hall of Fame keeps a separate entry per command level, confirming the
manual's table (l.238-244): Lt. Commander, Commander, Captain, Commodore,
Admiral for levels 1 through 5.

## Applied to the code

| Constant | Was | Now |
|---|---|---|
| `ENERGY_START` / `ENERGY_MAX` | 3000 | 5000 |
| `IMPULSE_START` / `IMPULSE_MAX` | not modelled | 500 |
| `SHIELD_START` | 0 | 2500 |
| `SHIELD_MAX` | 2500 (from manual) | 2500 (confirmed) |
| `WARP_START` | 5.0 | 1.0 |
| stars per quadrant | forced 1..8 | 0..8 |
| enemy count | `12 + level*4`, fixed | `level*10 + rand(0..12)` |

Stars can be zero: the chart showed `000` at quadrant 8,3 — a quadrant with
nothing in it at all.

## Still outstanding

1. **Enemy count per level — REOPENED.** Three level-3 games in a row all
   gave 42, contradicting the per-game random model. See the discriminating
   experiment above: relaunch-vs-replay tells us whether the seed is fixed
   per program launch.

2. **Travel costs.** Energy and time for a warp jump of known distance at a
   known warp factor, and the same for impulse. Needs `E` before and after,
   since the panel gauges are analog. `WARP_ENERGY_SCALE` and
   `IMPULSE_ENERGY_UNIT` are still invented.
3. **Torpedo count at start**, and whether it varies by level.
4. **Combat.** Laser damage against distance, the effect of the temperature
   and efficiency gauges, enemy return fire, and the boarding-party mechanic
   that appears nowhere in the manual. The game prints damage figures, so this
   is measurable the same way.
5. **Casualty scoring weight**, which needs a game where casualties occur.
