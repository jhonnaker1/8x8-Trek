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

### Enemy count per level — RESOLVED (random confirmed, spread still loose)

Read this section in order. It records two wrong turns before the answer: a fit
from non-independent samples, then a hypothesis that a relaunch disproved.

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

Which contradicts the model above — and then a second fact undermined the
reasoning that produced it.

The first response to the three 42s was that under `level × 10 + rand(0..12)`,
three identical draws is a 1-in-169 event. **That argument was wrong**, because
it assumed independent samples. Every reading here — the five-level survey and
the three repeats alike — was taken via the game's own **Play Again** prompt,
within one program lineage. Nobody relaunched from DOS.

If Play Again does not reseed, three identical level-3 results are not a
coincidence at all; they are the expected outcome. And the same objection
applies retroactively to the five-level survey: those five numbers were never
five independent draws, so the "random spread" they appeared to show is
unsupported. The remainders 8, 12, 10, 2, 3 may be nothing more than one seed's
worth of arithmetic.

What survives is weaker but real: **every reading lands at or above
`level × 10`**, across five levels. That is consistent with a `level × 10`
base. It is not evidence of a per-game random term.

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

**Leading hypothesis:** Play Again does not reseed, so within one launch the
count for a given level is stable. The lone 40 is the odd one out — it was the
*first* game after launch, before any Play Again, which is the one reading
taken from a different state than all the others.

#### Resolved by relaunching: the prediction was wrong, the model stands

A fresh launch from the DOS prompt, level 3, gave **35**. The hypothesis above
predicted 40. It was wrong.

Level 3 has now produced three distinct counts:

| Reading | Context |
|---|---|
| 40 | launch 1, first game |
| 42, 42, 42 | launch 1, via Play Again |
| 35 | launch 2, first game |

Two conclusions, one of them costly:

- **A random component is confirmed.** The same command level yields different
  counts, so the count is genuinely drawn, not a function of level alone. The
  fit committed in `ecde3a4` survives — the observed 35–42 sits entirely inside
  the band that model gives level 3 (30–42); its lower half simply has not been
  sampled.
- **Per-launch reproducibility is dead.** Two fresh launches gave 40 and 35, so
  there is no stable galaxy to anchor experiments to. That was the outcome
  worth hoping for and it did not happen.

Still unexplained: why three consecutive Play Again games at one level returned
identical counts, when a fourth reading at the same level from a different
launch did not. Under a uniform 30–42 draw that is a 1-in-169 event; under the
narrower 35–42 actually observed, about 1 in 64. Uncommon, not impossible.
Possibly Play Again reseeds from a value that only changes on launch. Not worth
more readings right now — it does not affect the model, only the explanation.

**Practical consequence for everything that follows.** Since a galaxy cannot be
reproduced, no measurement may rely on comparing two games. Travel costs and
combat damage must be taken as **before-and-after readings inside a single
game**, which is how the protocols in "Still outstanding" are written. Any
experiment phrased as "play game A, then play game B and compare" is invalid
here.

### Travel costs — converter rate confirmed, impulse measured, warp pending

All readings taken inside one game, with main energy first dropped to 3000 by
diverting 2000 into already-full shields, so the 5000 ceiling could not clip
the result. Without that step the first attempt showed no energy use at all —
the jump cost less than the converter replaced, and the cap hid it.

**Converter rate: 400 per stardate, confirmed.** An impulse hop showed main
rising 3005.7 → 3022.4 with the Date apparently unchanged at 3501.8.
`16.7 ÷ 400 = 0.042` stardates, which rounds to the same displayed figure. The
manual's rate (l.265) is what the code actually does.

**Impulse: about 6.3 energy per sector, about 0.042 stardates per sector.**
Sector 6,6 → 6,5 is a distance of exactly 1; impulse fell 493.8 → 487.5. An
earlier hop cost 6.2. Impulse draws **only** from its own pool — main rose
across the same move, which independently confirms the three-pool model.

Our previous values were 60 energy and 0.1 stardates per sector: ten times too
expensive, and about twice too slow.

**Warp: roughly 194 units for one quadrant at warp 5, ±20.** From
`+5.7 = 400 × 0.5 − cost` over an interval containing one such jump. The error
bar comes from the Date display rounding to a tenth.

**Warp 8, one quadrant: about 710 units.** Main 3039.0 → 2409.3 over 0.2
stardates, so 629.7 spent plus 80 refilled. A clean reading — impulse (481.3)
and shields (2407.0) were identical across both frames, so nothing but the
warp touched main. The `>>ALERT<<` was a cloaked Vandal present in the
quadrant, not damage taken.

**Both earlier candidates are refuted.** Linear (`40 × warp`) predicted 320,
quadratic (`8 × warp²`) predicted 512. Neither is close to 710.

| Warp | Cost, one quadrant | Ratio to warp 5 |
|---|---|---|
| 5 | ~194 | — |
| 8 | ~710 | ×3.66 |

Warp rose ×1.6 and cost ×3.66. Square would give ×2.56, cube ×4.10. The
exponent sits **between square and cube, nearer cube**.

`1.33 × warp³ + 27` passes through both points exactly. That is not evidence:
two points fit any two-parameter curve, and quoting it as a result would be
numerology. **A third warp factor is needed**, and a low one discriminates
best — the candidate curves are far apart at the bottom of the range and
bunched at the top.

**Flight time goes as distance / warp², not distance / warp.** This came out
of an attempt to measure the warp-2 cost, which failed — 4 quadrants at warp 2
took **11.0 stardates**, the converter returned 4400 against 2591 of headroom,
and main clipped at 5000. The cost was lost, but the timing was the more
valuable result.

| Warp | Distance | Stardates | per quadrant |
|---|---|---|---|
| 8 | 1 | 0.2 | 0.20 |
| 5 | 1 | ~0.4 | ~0.40 |
| 2 | 4 | 11.0 | 2.75 |

Warp 2 → 8 is four times the speed but 13.75 times the time per quadrant.
`1/warp` predicts a factor of 4, `1/warp²` predicts 16. Fitting all three
gives `time ≈ 10 × distance / warp²`, reproducing 0.16 / 0.4 / 10.0 against
0.2 / 0.4 / 11.0.

The old `distance/warp` model was five times too fast at warp 2 — which is
exactly why that trip was expected to take 2 stardates, why the headroom was
sized for 800 units of refill instead of 4400, and why the reading clipped.

**The cost figures are unaffected**, since they were computed from *measured*
elapsed time rather than modelled.

#### Cost model applied: 1.5 × distance × warp³

Correcting the time model made the cost model's error visible: longer flights
mean more refill, and at warp 5 the corrected timing put 160 units back
against a modelled cost of 150, so travel turned a profit again and the core
test caught it — the same assertion that caught the original inversion.

Two well-separated points give an exponent of 2.76, so cube is adopted. It
reproduces 187 against 194 at warp 5, and 768 against 710 at warp 8. The
exponent is well supported; the leading 1.5 is not precise.

#### Distance linearity: refuted

Two quadrants at warp 8. Main 5000.0 → 3985.5 over 0.4 stardates, so 1014.5
spent plus 160 refilled: **cost ≈ 1174.5**.

Against ~710 for one quadrant at the same warp, that is a ratio of **1.65, not
2.0**. The linear model predicted 1536 and was over by 31%.

**Time is linear in distance** across the same pair — 0.2 → 0.4 stardates —
so only the cost needed changing.

A fixed overhead per jump plus a per-distance term fits both points: `a + b·d`
gives a ≈ 245, b ≈ 465 at warp 8. Normalised by warp³ that is roughly

```
cost = (0.5 + 0.9 x distance) x warp^3
```

which reproduces all three readings: 171 against 194 at warp 5 (−12%), 704
against 710 (−1%), and 1152 against 1174.5 (−2%). The warp-5 point is the
weakest of the three — its interval contained impulse hops as well as the
jump — so the two 8s carry the fit.

**Where this stops being worth refining.** The remaining ambiguity is whether
the sublinearity is a genuine fixed overhead per jump or a fractional power of
distance. Both forms fit the readings we have and predict the same thing for
one- and two-quadrant jumps; they only separate at four or more quadrants, and
then by about 8% — inside the noise our ±0.1-stardate date resolution allows.
For a port, either is indistinguishable in play.

Travel is therefore called done at three cost points and three time points.
The next measurement effort belongs on **combat**, which has no readings at
all and is a far larger body of unknowns.

**Stardate precision is a problem for our model.** The original resolves time
far finer than it displays: 0.042 stardates elapsed while the panel showed no
change. Our `stardate` is in tenths, so a one-sector impulse hop rounds to zero
elapsed time and earns no converter output. Storing elapsed hundredths since
mission start would fit `uint16_t` comfortably (a 30-stardate mission is 3000)
and display as `3500.0 + elapsed/100`. Not yet done — it touches the UI's
`put_tenths` as well, and belongs in its own change rather than bundled into a
measurement.

**The main viewer confirms the distance model.** It reads `∅ 45.0` and
`△ 1.41` — bearing in degrees, distance in sectors. 1.41 is √2, an object one
sector diagonally away, so distance is plain Euclidean in sector units. Our
8.8 table gives 362/256 = 1.414 for that case.

### Cloaking, seen in the viewer

The main viewer labelled a contact `VANDAL (CLOAKED)` while the status panel
showed `>>ALERT<<`. So cloaking is a state the viewer reports rather than an
absence of information, and a cloaked Vandal still raises the alert. The
binary's strings carry `Vandal activates cloaking device.` as a response to
being fired on, so the mechanic has at least two faces: entering cloaked, and
cloaking under fire.

### The Vandal Death Pod

Observed live, mid-measurement: *"Vandal Death Pod enters quadrant: 93 unit
hit on Lexington."* Shields absorbed it, 2500 → 2407.

**Not in the manual at all.** The binary's strings carry both this message and,
at line 535 of `reference/strings.txt`, a variant where the Lexington is
destroyed outright; the end-of-game text at line 495 lists death by Vandal
death pod among the ways a mission can end. So it is a lethal random event that
can arrive in a quiet quadrant with no enemies on the scan and the status
still Green.

Practically, it is a third hazard to measurement alongside the tractor beam and
ordinary combat.

### Incidental findings from a spoiled travel run

A travel-cost run was contaminated before it produced a usable point, but
confirmed three things on the way.

**The long-range tractor beam is real, and it is a hazard to measurement.**
The Lexington was seized mid-experiment and dragged into quadrant 3-3, which
held four Mongols. This is one of the mechanics found only in the binary's
strings — the manual never mentions it. Practically: any measurement run can
be ended at any moment by being yanked into a fight, so travel readings must
be taken over the shortest possible sequence, one move at a time, and
abandoned the instant anything else happens.

**Energy is fractional.** The report showed `3829.2`. The original tracks
energy as a real, not an integer. Our core uses `uint16_t` throughout by
design, so it cannot reproduce tenths — acceptable for a port, but it means
our figures will drift from the original's by sub-unit amounts over a long
game, and any future comparison should be made to a tolerance rather than
expecting equality.

**Enemy fire is heavy.** Three simultaneous hits of 299, 300 and 196 units,
from Mongols at varying distances, with shields down. Against a 2500 shield
pool that is a serious threat, and it gives an order of magnitude for the
combat work: hits are hundreds of units, not tens. Damage is printed per hit
with the firing ship's sector, which is exactly the instrumentation the combat
measurements will need.

### Ranks

The Hall of Fame keeps a separate entry per command level, confirming the
manual's table (l.238-244): Lt. Commander, Commander, Captain, Commodore,
Admiral for levels 1 through 5.

## Session 2 — 2026-08-15, combat

The run that produced these readings was a wreck from the first stardate: a
long-range tractor beam pulled the ship into quadrant 6,5 at date 3500.6 and
the ambush volley destroyed the short-range scanner, life support and the
shield generators outright. It was kept alive purely as a laboratory, and it
turned out to be a good one — laser measurement needs only the energy we
choose, the target's sector, and the efficiency figure the game prints.

### Laser damage — RESOLVED, linear in distance

Two volleys at a fixed 300 units per target, from sector 8,2:

| Target | distance | energy | damage | eff printed | damage ÷ eff |
|---|---|---|---|---|---|
| 7,3 | 1.414 | 300 | 236 | 85% | 277.6 |
| 5,4 | 3.606 | 300 | 195 | 89% | 219.1 |
| 2,1 | 6.083 | 300 | 145 | 93% | 155.9 |
| 1,6 | 8.062 | 300 |  84 | 82% | 102.4 |

Least squares on the last column gives slope −26.25, intercept 314.6, and
every residual under 1.1 on values spanning 102 to 278 — R² ≈ 0.9999. Laser
falloff is LINEAR in distance. Both the inverse-square shape of Super Star
Trek's combat math and every nonlinear guess we had are refuted.

An intercept of 1.05 × energy is not physical — a laser cannot deliver more
than is put into it. The efficiency figures above are the ones printed AFTER
each shot, and the shot's own heating is already in them. Each shot costs 3-4
points, so refitting against pre-shot efficiency (post + 3) moves the
intercept to 303 against 300 fired, and the zero crossing to d = 11.99:

    damage = energy * efficiency * (1 - distance / 12)

Intercept equal to the energy spent and a round 12 for the range constant is
a designed formula rather than a fitted curve, so the pre-shot reading of the
efficiency is taken as correct. That also fixes the order of operations:
damage is computed at the efficiency in force when the trigger is pulled, and
the printed figure is what is left afterwards.

The pre-shot efficiencies are themselves reconstructed (post + 3), not read,
so the 1/12 is the weaker half of this result. Reading the gauge immediately
before a volley would settle it.

NOT yet tested: linearity in energy. Every reading above is at 300. The
manual's claim that laser repair scales damage linearly ("100% working lasers
will do twice the damage of 50%") implies energy does too, but implication is
not measurement.

### Efficiency and heat

Each 300-unit shot costs 3-4 percentage points of efficiency, reported as
"Lasers overheat. Now running at N% efficiency." The message appears after
every shot, not only at a warning threshold. The manual (l.329-331) says the
gauge combines heat and battle damage, and "Laser efficiency reduced by
damage." is printed as its own line at the start of a volley when the laser
system itself is damaged — so the single printed percentage carries both
terms, which is what makes it usable as one divisor.

### Laser targeting does NOT need the scanner

Predicted that a short-range scanner at 0% would prevent the laser control
officer from acquiring targets — the binary carries "SCIENCE: Information not
available; scanners damaged." and the manual says the scanner is dead below
50%. Refuted directly: with the scanner at 0% the officer still prompted
"Amount to fire at 7-3:" and the main viewer still identified the target as
MONGOL COMMANDER. There is no scanner prerequisite for firing.

### Laser energy comes from main

"Captain, we have insufficient energy! May I suggest the use of a torpedo?"
fired on the third target of the second volley, with main energy below 300.
Lasers draw from the main banks, not a weapons pool.

### Enemy return fire — has a hidden per-ship term

Three volleys of incoming fire, ship at sector 8,2 throughout:

| Mongol at | distance | volley 1 | volley 2 | volley 3 |
|---|---|---|---|---|
| 7,3 (6,3 in v1) | 1.41 (2.24 in v1) | 584 | 388 | 133 |
| 5,4 | 3.61 | — | — | 91 |
| 2,1 | 6.08 | 176 | 100 | 30 |
| 1,6 | 8.06 | 117 |  79 | 53 |

This cannot be fitted the way our lasers can. The first row is one ship — the
scanner reported "The commander has moved. He is now at 7-3" — which moved
CLOSER and hit for LESS, twice. Distance alone cannot produce that.

Output decays every volley, and unevenly: 2,1 fell to 30% of its previous
shot while 1,6 only fell to 67%. 2,1 is the ship we hit for 145. That points
at enemy fire scaling with the firing ship's remaining strength, which would
make return fire a readout of enemy hit points — hit a Mongol and its next
shot reports how much came off it. Plausible, unconfirmed, and worth testing
deliberately because it would remove the need to measure enemy HP by killing
things and summing.

### Incidental

- Enemies retreat across quadrant boundaries mid-fight: "Scout escapes to
  quad 6-4", with the chart moving 6,5 from 403 to 303 and 6,4 from 403 to
  503 in the same instant. It came back a turn later.
- Enemies also arrive mid-fight: "A Mongol has appeared at 5-4".
- The scanner names the class when either happens (Scout, commander).
- The galaxy Mongol counter decremented 42 → 41 on the kill.
- EnTorp tubes took damage to 62%, which by the manual's rule (34-66% = one
  tube) leaves a single tube. Torpedoes were not measurable in this run.

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

1. **Enemy count — spread bound.** Randomness is confirmed and the committed
   band contains every reading, but only 35..42 of level 3's 30..42 has been
   sampled. Low-value now; the model is consistent either way.


2. ~~Travel costs~~ — **done.** Three cost points, three time points; further
   refinement is inside the noise. See above.


3. **Torpedo count at start**, and whether it varies by level. Still not
   taken — the scanner panel that carries the count was destroyed before it
   could be read.
4. ~~Laser damage against distance~~ — **done.** Linear, `1 - d/12`.
5. **Laser damage against energy.** Every reading is at 300 units; linearity
   in energy is assumed, not measured. One volley with two different amounts
   at known distances settles it.
6. **Enemy hit points**, and whether return-fire strength tracks them.
7. **Torpedoes** — damage, and accuracy against distance and shield state.
8. **The boarding-party mechanic**, which appears nowhere in the manual.
9. **Casualty scoring weight**, which needs a game where casualties occur.
