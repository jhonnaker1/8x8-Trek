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
| Casualties on board | −1 each |
| Loss of ship | −200 (flat) |

Quitting immediately at stardate 3500.0 with nothing done scores **−300**,
which is the incomplete-mission penalty alone. That checks out.

The casualty weight was taken in session 2, from a game quit at 3501.2 after
a heavy fight: 18 casualties scored −18, so −1 each. That sheet also settles
two more points and raises one question.

    0     Rescues @ 200 each.......................0
          Penalty for incomplete mission........-300
    1     Mongols killed @ 10 each................10
    1     Commanders killed @ 20 each.............20
    0     Enemy bases destroyed @ 50 each..........0
    0.00  Kill/day ratio @ 500 per day.............0
    18    Casualties on board Lexington..........-18
    0     Stars destroyed @ -5 each................0
    0     Bases hit @ -200 each....................0
          TOTAL.................................-288

The arithmetic closes exactly, so no term is hidden.

MONGOLS AND COMMANDERS ARE COUNTED SEPARATELY. Two ships died in that game, a
Commander and a standard battleship, and they are itemised as one of each --
so the +10 is for standard ships only.

LOSS OF SHIP DID NOT APPEAR, because that game was quit rather than lost. The
binary carries "Penalty for loss of ship.........-200", so the weight is
known; the line has never been seen to fire.

UNEXPLAINED: the kill/day ratio printed 0.00 against two kills in 1.2 elapsed
stardates, which is about 1.67 and would have been worth 833. Something gates
that term. The plausible guess is that it credits only on a completed
mission, given the incomplete-mission penalty is also present on this sheet,
but that is a guess and the term should not be implemented from it.

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

The pre-shot efficiencies above are reconstructed (post + 3), not read, which
made 1/12 the weaker half of that result — settled outright by the clean run
below.

### Laser damage — CONFIRMED exactly, clean run

Fresh level-3 game, shields raised first, all systems at 100%, lasers cold.
One volley from sector 4,4, predictions written down before firing:

| Target | dy,dx | distance | energy | predicted | actual |
|---|---|---|---|---|---|
| 3,5 | 1,1 | 1.414 | 500 | 441.1 | **441** |
| 4,6 | 0,2 | 2.000 | 250 | 208.3 | **208** |
| 8,2 | 4,2 | 4.472 | 250 | 156.8 | **157** |
| 8,7 | 4,3 | 5.000 | 250 | 145.8 | **146** |

Four for four, to the unit, at two different energies. The formula is exact:

    damage = energy * efficiency * (1 - distance / 12)

Energy linearity is confirmed in the same volley — 208 and 146 are precisely
half of what 500 would have delivered at those ranges.

Rounding is TO NEAREST, not truncation: 156.83 printed as 157 and 145.83 as
146. Both would have printed one lower under truncation.

### Efficiency and heat — the penalty is damage, not temperature

The clean volley printed NO overheat message at all. 1250 units went out at a
flat 100% efficiency while the Temp gauge sat around 700 of its 1500 scale.
Heat therefore costs nothing below some threshold well above 700.

In the wrecked run every single shot cost 3-4 points and printed "Lasers
overheat. Now running at N% efficiency." — but that run also printed "Laser
efficiency reduced by damage." as its own line at the start of the volley.
The penalty was coming from the damaged laser system, not from temperature.
The manual (l.329-331) says the gauge combines heat and battle damage; the
single printed percentage carries both, which is what makes it usable as one
divisor.

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

Written up here first as "cannot be fitted the way our lasers can", on the
grounds that the first row is a single ship — the scanner reported "The
commander has moved. He is now at 7-3" — which moved CLOSER and hit for LESS,
twice, which distance alone cannot produce.

THAT WAS WRONG. Enemies use the SAME falloff law. Dividing each incoming hit
by (1 - d/12) leaves an implied source energy per ship, and in the clean run's
first incoming volley (ship at 4,4, shields up) those come out as:

| Mongol at | distance | hit | implied energy |
|---|---|---|---|
| 3,5 (Commander) | 1.414 | 181 | 205 |
| 8,7 | 5.000 |  84 | 144 |
| 8,2 | 4.472 |  89 | 142 |
| 4,6 (just arrived) | 2.000 |  60 |  72 |

Two ships land within 2 units of each other. Running the wrecked run's first
volley through the same division, two of its ships come out at EXACTLY 357
each. Read as: one law governs both directions of fire, with the energy each
ship commits declining as it takes damage — the commander's implied energy
fell 718 → 440 → 151 across three volleys, and 2,1 fell 357 → 203 → 61 after
we hit it for 145.

THAT READING DOES NOT SURVIVE MORE DATA EITHER. Later in the clean run, in
quadrant 8,6 with the ship at 4,4, four enemies fired in one volley and the
short-range scanner showed all four in light blue — all the same class,
standard battleships:

| Mongol at | distance | hit | implied energy |
|---|---|---|---|
| 2,3 | 2.236 |  92 | 113 |
| 6,6 | 2.828 | 169 | 221 |
| 2,5 | 2.236 | 147 | 181 |
| 3,8 | 4.123 | 134 | 204 |

Same class, same instant, implied energies spread by a factor of two. The
ship at 2,5 gave 143 in the previous volley and 181 in this one. And 2,3 had
by then absorbed 162 units of our laser fire with its output flat across
three volleys — 115, 123, 113 — so damage does not suppress it.

So enemy fire carries a large random component, roughly 110-220 for a
battleship at these ranges. The earlier clustering (142/144, and the pair at
exactly 357) was signal read into a handful of draws that happened to land
near each other; two consecutive commits drew a confident and different
conclusion from it, both wrong.

And there is a confound underneath all of it. One volley later the same three
untouched battleships read 240, 251 and 260 implied — against 181, 204, 221
the volley before, and 142/144 at the start of the run. Enemy output appears
to climb steadily through an engagement, which it almost certainly does not.
What changed is OUR shields. Early hits printed

    Shields absorb 181 unit hit from 3-5

and later ones printed

    195 unit hit from Mongol at 2-5

If the printed figure is what PENETRATES rather than what was fired, every
enemy reading we hold is scaled by a shield state we never controlled, and
the rising trend is our own shields failing. That one confound accounts for
both failed readings above: the "declining output" of the wrecked run (where
the shield generators were destroyed early) and the "clustering" of this one.

What survives: the distance trend is real and strong. What does not: any
claim that the enemy's law has been identified, or that return fire reports
enemy remaining strength.

To measure enemy fire properly the shield state has to be held constant and
known — either shields down throughout, or verified at full before every
reading — and the firing ship's CLASS recorded against every hit, with enough
shots per class to separate the random component from the per-class mean.
None of that was done here, and no enemy-fire constant should be taken from
this data.

### Enemy hit points — READ, not inferred (2026-08-16)

Taken through dosbox-automation rather than by shooting things and summing.
The original keeps the current quadrant's enemies as a table of 6-byte
records, three 16-bit words each -- y, x, hit points -- starting at linear
**184146**, zero-filled past the last entry. The records appear in the same
order the laser control officer prompts for them.

Method: dump memory, fire a known shot, dump again, look for the value that
fell by exactly the printed damage. Doing that while the WEAPONS CONTROL
dialog is still open matters -- no enemy return fire has resolved yet, so our
shot is the only thing that changed.

A 400-unit shot at range 4.472 printed "251 unit hit on Mongol" and exactly
one 16-bit value in 338KB fell by exactly 251:

| addr   | before | after |
|---|---|---|
| 184150 |    292 |    41 |

Reading the table gave three enemies in that quadrant:

| y,x | hit points |
|---|---|
| 3,5 | 355 |
| 4,7 | 355 |
| 1,5 | 355 |

**HIT POINTS ARE FIXED PER CLASS, NOT ROLLED** -- three ships to the unit.
The targeted one was named MONGOL BATTLESHIP in the viewer; the other two
were never identified, so 355 is confirmed for a battleship and shared by two
unnamed ships. A MONGOL SUPPLY SHIP in another quadrant read **120**.

Level dependence is untested: every reading is from one level-3 game, and
neither 355 nor 120 sits near the other as a literal in the unpacked binary,
so both may be computed from the command level.

This also recovered a laser reading retroactively. The shot before it, 100
units at the same range, had its printed result scroll away -- but 184150
went 355 -> 292 across those dumps, a fall of exactly **63**, which is what
`energy * eff * (1 - d/12)` predicts. Eighth confirmation of the model.

### Enemy fire: the printed figure IS the damage (2026-08-19)

Measured by pinning energy to 5000 and shields to 2500 by direct write, then
taking a turn that deals no damage, then reading both back.

| printed | measured drain |
|---|---|
| 619 (plasma bolt) | 619.06 |
| 248 + 181 = 429 | 428.85 |

Two independent turns. The number the game prints is the real damage, and
with shields up and sufficient it comes out of the SHIELD pool with main
energy untouched.

**This weakens the reason given earlier for withdrawing the enemy-fire
data.** That withdrawal argued the printed figure might be the part that
penetrated rather than the shot fired, making every reading depend on an
uncontrolled shield state. It is not a residual. So the rising trend across
those old volleys -- implied energies of 142/144, then 181/204/221, then
240/251/260 -- still has no explanation, and the shield-state story should
not be treated as one. The withdrawal stands; the reasoning behind it does
not.

**Plasma bolts are a separate weapon.** "WARNING: Mongol at 4-7 fires plasma
bolt" arrives as its own message before the hit lands, and that hit was 619
against 179 from an ordinary shot in the same quadrant. Previously known only
as a string in the binary.

Still unmeasured: how enemy damage varies with distance, class and remaining
strength. The instrument is now good enough -- energy and shields can be
pinned, and the enemy table gives every firing ship's exact position and hit
points -- but the input scripting is not. Blind keystrokes desynchronise
against the game's modal dialogs, which is what spoiled three of the four
turns in this session. Any further work here needs the screen read between
every step rather than a fixed keystroke sequence.

### Repair rate: 20 points per stardate, and it does NOT divide

Measured by poking a system down and moving, with the elapsed time read out
of memory rather than inferred.

| elapsed stardates | repair | floor(20 * elapsed) |
|---|---|---|
| 0.172 | +3  | 3.44 -> 3 |
| 2.754 | +55 | 55.08 -> 55 |
| 2.750 | +55 | 55.00 -> 55 |

    repair = floor(20 * elapsed_stardates)

Three points, no capping, exact on all three. Runs where no time elapsed
repaired by exactly zero, so repair is a pure function of elapsed time and
nothing else -- not per turn, not per command.

**It does not divide among damaged systems.** With the scanner and the lasers
both at 5, a 2.750 stardate trip repaired BOTH by 55, each at the full 20 per
stardate. The manual says the crew "normally divide their time evenly among
all damaged systems" (l.451-456). That is not what the code does; every
damaged system repairs at the same rate in parallel. One clean two-system
measurement, against two clean one-system measurements.

### The stardate is at DS:1D42, not DS:1D36

Costly correction. DS:1D36, DS:1D42 and DS:1D48 all read 3500.0 at the start
of a game, so the first was taken as the stardate. It is not. With the
console showing 3520.8, DS:1D42 and DS:1D48 read 3520.7969 while DS:1D36 read
3520.625 -- a different quantity that merely tracks nearby.

Several repair readings were nonsense before a screenshot caught it: moves
that plainly succeeded reported 0.000 elapsed, which produced a spurious
"repair happens per turn, not per unit time" theory and a search for a master
copy of the repair array that does not exist. The instrument was wrong, not
the game. Reading the console and the memory together is what found it.

### The damage model, found and verified causally (2026-08-19)

Read out of the disassembly first, then confirmed by WRITING to the running
game and watching it obey. That is a step up from every earlier session: the
oracle stopped being purely observational.

**DS:235A is the system repair array** -- twelve 16-bit words, 1-based, every
one at 100 on a fresh game. Position 8 is DS:2368, and the console lists
systems in this order:

    1 EnergyConverter   5 EnTorp Tubes    9 L.R. Scanner
    2 Shields           6 Warp Engines   10 Computer
    3 Life Support      7 Impulse Engine
    4 Lasers            8 S.R. Scanner

Eighth is the short range scanner, which is exactly the entry the scanner
redraw code reads. TWELVE entries rather than ten is itself corroboration:
the manual documents two systems that never appear on the console, the
Transporter and the Shuttlecraft, both of which must be at 100% to use.

**The redraw rule, from the code at 0x01C37C:**

    cmp word [0x2368], 90 ; jg  -> show the real glyph
    cmp byte [cell], 'E'  ; je  -> show the real glyph
    cmp byte [cell], '*'  ; je  -> fall through to the 50 test
    cmp byte [cell], 'N'  ; jne -> show '.'
    cmp word [0x2368], 50 ; jle -> show '.'

The manual (l.388-391) says scanners are fully functional above 90%, cannot
detect anything smaller than a star below 90%, and do not work below 50%.
Same two numbers, and the code pins down what survives: your own ship ('E'),
stars ('*') and novas ('N').

**Verified by poking the value and forcing a repaint**, in a quadrant with
three stars and no enemies:

| value written | stars | ship | SYSTEMS STATUS bar |
|---|---|---|---|
| 100 | visible | visible | full green |
| 70  | visible | visible | short yellow |
| 40  | GONE    | visible | short red |

Both predictions made before looking. The console's own bar tracked the
written value, which confirms the address independently of the scanner
behaviour.

Not yet known: the exact colour thresholds for the status bars, and whether
the other eleven systems use the same 0..100 scale (they all read 100, which
is consistent but not proof).

### Hit points scale with command level

A level-1 game read through the same table: three battleships in quadrant 7,6
at **325** each, four more in 8,7 at 325, one in 7,7 at 325 -- nine ships,
three quadrants, all identical. The 3,2 target was named MONGOL BATTLESHIP in
the viewer, so this is like-for-like with the level-3 reading of 355.

    30 units across two levels  ->  battleship hp = 310 + 15 * level

Two points define a line; a level-5 game predicting 385 would settle it.
Whether supply ships scale the same way is untested -- their 120 is a single
level-3 reading.

Still not found: a Commander or a Scout. Nine ships at level 1 were all
battleships, so the class mix at low levels is heavily weighted toward them.

### More evidence against a mission deadline

That level-1 game reached stardate **3578** and was still running. Warp
defaults to 1.0 and our own measured time model (10d / warp^2) puts a
one-quadrant hop at ~10 stardates there, so a handful of moves burned 78
stardates. The `MISSION_TENTHS` we invented -- 30 stardates -- would have
ended it 48 stardates earlier. Independent of the memory scan that found no
deadline value anywhere in 338KB.

### The galaxy chart is enemies-bases-stars

Confirmed by corroboration rather than by squinting at columns: the message
"The StarBase in 7-6 reports that it is under attack" arrived while the cell
for 7,6 read `317`, i.e. 3 enemies, 1 base, 7 stars. An earlier reading in
this file guessed at the format from column alignment and got it wrong.

### Enemy fire does track remaining strength after all

With the ship at 3,5 down to 41 of its 355 hit points, its next shot was
absorbed as **16** units, against 27 and 48 from the two undamaged ships in
the same volley. So the very first reading of enemy fire -- output falling as
a ship takes damage -- was right, and the "flat output" counter-reading that
withdrew it was the shield-state confound rather than evidence against it.

What remains withdrawn is the quantitative part: no enemy-fire constant
should be taken from any of that data, because the printed figure still
depends on our shield state. The qualitative claim is now supported by a
directly-read hit-point value rather than by an inference.

### Battleship hit points — lower bound only (superseded)

Cumulative laser damage until a ship dies gives its hit points directly, now
that the laser formula is exact. Two shots of 100 units at the battleship at
2,3, range 2.236, each delivering exactly 81 as predicted. It survived both,
so a standard Mongol battleship has MORE THAN 162 hit points. The run ended
before a third shot — four battleships were firing and the ship was lost.

An upper bound comes free from the torpedoes: one torpedo destroyed an
undamaged battleship outright, so torpedo damage exceeds battleship hit
points, which are therefore somewhere above 162.

### Torpedo accuracy

Two torpedoes, two kills, at ranges 1.414 and 3.000, both fired with SHIELDS
UP. The manual says raised shields throw torpedoes off course; at these
ranges the penalty is not visible. Nothing measured at long range, and no
miss has been observed at all, so the accuracy model is untouched.

### Raising shields costs 50 units

The POWER DISTRIB report (the E command) taken at date 3500.0 immediately
after SHUP, with nothing else having happened:

    PMAX     PAVL     PPCT
    1:5000   1:4950   1:99.0
    2:0500   2:0500   2:100.0
    3:2500   3:2500   3:100.0

Main is down exactly 50. That is the manual's unquantified "small amount of
energy from the main energy banks" (l.339-341), now a number. It also
re-confirms the three pools and their maxima on a fresh game.

### The game prints range itself

The main viewer shows bearing and distance to the current target: "∠45.0
∆1.41" for the Commander at 3,5 with the ship at 4,4. Euclidean distance
confirmed against our own dist_tab, and future readings can be taken off the
viewer rather than computed from sector numbers.

### Torpedoes — count is 9

Not in the scanner panel where the manual's example puts it. The count is the
3x3 array of red stars below the shields dial, one star per torpedo: nine at
the start of a level-3 game, eight after firing one. Three tubes and nine
torpedoes. Our guess of 10 was wrong. Whether it varies by level is untested.

The command takes a count, then a sector per torpedo in the same two-digit
form the move command uses:

    Number to fire: 1
    Sector to fire #1 at: 35
    Tracking #1
    Mongol at 3-5 destroyed!

A kill reports only the kill, with no damage figure, so measuring torpedo
damage needs a target that survives one. The Commander it killed here was
already carrying 441 from the laser volley.

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
| laser damage | SST's inverse-square | `energy * eff * (1 - d/12)` |
| `SHIELD_RAISE_COST` | not modelled | 50 |
| `TORPS_START` | 10 (guessed) | 9 |

Stars can be zero: the chart showed `000` at quadrant 8,3 — a quadrant with
nothing in it at all.

## Still outstanding

1. **Enemy count — spread bound.** Randomness is confirmed and the committed
   band contains every reading, but only 35..42 of level 3's 30..42 has been
   sampled. Low-value now; the model is consistent either way.


2. ~~Travel costs~~ — **done.** Three cost points, three time points; further
   refinement is inside the noise. See above.


3. ~~Torpedo count at start~~ — **done.** Nine. Whether it varies by level is
   still untested.
4. ~~Laser damage against distance~~ — **done.** `energy * eff * (1 - d/12)`,
   confirmed exactly on four predicted-in-advance readings.
5. ~~Laser damage against energy~~ — **done.** Linear; confirmed in the same
   volley at 500 and 250.
6. ~~Battleship hit points~~ — **done.** 355 at level 3, 325 at level 1;
   scales at 15 per command level. Supply ships
   are 120. Commanders are still bracketed at 441..501 and scouts have never
   been seen; both are now one sighting away, since the table can just be
   read. Level dependence untested.
7. **Enemy fire.** Nothing here is usable; see the correction above. Needs a
   run with the shield state held constant and the firing ship's class
   recorded against every hit. Whether the printed figure is the shot fired
   or the part that penetrated is the first thing to establish, and a single
   volley taken with shields verified full, then repeated with shields down,
   would answer it.
8. **The heat threshold.** 700 of 1500 costs nothing. Where it starts to bite
   is unmeasured.
9. **Torpedo damage.** Needs a target that survives one, so a Commander or a
   base rather than a standard battleship.
10. **Torpedo accuracy.** No miss has been observed at all — two for two at
   ranges 1.4 and 3.0, both with shields up. Long range is untested.
11. **The boarding-party mechanic**, which appears nowhere in the manual.
12. ~~Casualty scoring weight~~ — **done.** −1 each.
13. **The kill/day term.** Printed 0.00 with two kills in 1.2 stardates. See
    the scoring section; do not implement it until the gate is understood.

## The console, captured at full 640x350 (2026-08-19)

`layout.h` had been carrying an open action item since milestone 1: its panel
table was traced by eye off a 320x175 half-scale JPEG, where one character
cell is 4x7 pixels and column boundaries cannot be read. dosbox-automation's
`GET /video/frame` now returns the real 640x350 frame, so the panels can be
measured instead of guessed. Frame borders, taken as maximal single-colour
runs, in pixels and then in 80x25 cells:

| panel | pixels | cells |
|---|---|---|
| SHORT RANGE SCAN | x3..160, y2..136 | cols 0..20, rows 0..9 |
| STATUS | x160..326, y2..136 | cols 20..40, rows 0..9 |
| CHART OF KNOWN GALAXY | x333..636, y2..136 | cols 41..79, rows 0..9 |
| LASERS | x3..166, y141..197 | cols 0..20, rows 10..14 |
| COMMAND | x3..166, y202..247 | cols 0..20, rows 14..17 |
| MAIN VIEWER | x173..316, y141..247 | cols 21..39, rows 10..17 |
| U.S.S. LEXINGTON | x1..158, y251..348 | cols 0..19, rows 17..24 |
| SYSTEMS STATUS | x161..318, y251..348 | cols 20..39, rows 17..24 |

COMMUNICATIONS and DAMAGE REPORT are not drawn at all on a fresh game -- the
lower right is plain background until there is a message -- so their frames
are still unmeasured.

**The 1:1 cell mapping in layout.h is true of the frames and false of the
contents.** EGA Trek runs in a 640x350 graphics mode and draws text at
whatever pitch it likes inside those frames. SYSTEMS STATUS puts ten bars at
an 8-pixel pitch (measured: bars at y267, 275, 283 ... 339, each 7px tall and
50px wide, x261..310) inside a panel only six 14-pixel character rows deep.
The short range scan does the same thing, fitting a header and eight rows into
eight character rows' worth of height.

So the original's geometry can be adopted for panel *positions* but not for
line counts: a character display cannot draw ten lines in six rows. The port
takes the measured rectangle and re-lays out the contents to fit it.

### SYSTEMS STATUS lists twelve systems, not ten

The console panel shows ten. The `REPAIR` command opens a modal STATE OF
REPAIR dialog which lists twelve -- the console's ten plus Transporter and
Shuttlecraft -- each with a percentage and Docked/Undocked repair-time
columns. That matches the twelve words at DS:235A exactly, and settles the
earlier open question of whether the extra two entries in the repair array
were really the two systems the manual documents but the console omits. They
are.
