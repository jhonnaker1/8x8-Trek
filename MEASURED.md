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
9. **Torpedo damage.** Shape now known from the ancestor, value still not --
   see "Torpedo damage, and why nothing survives one" below. The test is
   named: a Commander at 695 hit points is the one target that can survive a
   badly aimed shot.
10. **Torpedo accuracy.** No miss has been observed at all — two for two at
   ranges 1.4 and 3.0, both with shields up. Long range is untested.
11. **The boarding-party mechanic**, which appears nowhere in the manual.
    The ancestor has no equivalent either, so this one is EGA Trek's own and
    must come from the game or the disassembly.
14. **Enemy motion thresholds do not transfer.** See "The ancestor's motion
    constants are on its own power scale" below. The direction of every term
    is sound; the numbers are not ours.
15. **Are hit points fixed per class, or rolled per ship?** See "Hit points
    may not be constants" below. Our core assumes fixed; the ancestor rolls.
    One unexplained reading of 255 is the reason to ask.
12. ~~Casualty scoring weight~~ — **done.** −1 each.
13. ~~**The kill/day term.**~~ RESOLVED 2026-08-19, as far as one reading
    allows -- see "The kill-rate term" below. Implemented, gated on a
    finished mission, marked FITTED.

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

## The ancestor found: EGA Trek descends from the FORTRAN line, not the BASIC one (2026-08-19)

Nels Anderson has said in interview that he found Star Trek on a DECsystem-10
in the mid-1970s, "written in Fortran if I recall right, with source code
available even", and that for EGA Trek he "tried to stay pretty much true to
the original gameplay", with the status displays "pretty close to the
original mainframe versions".
(https://www.classicdosgames.com/interviews/nelsanderson.html)

That makes `reference/superstartrek.bas` the wrong ancestor. It is the
Mayfield -> Ahl -> Leedom BASIC line. Anderson's stated source is the FORTRAN
line, and the two diverged. Anderson's own command list decides it:

| feature | 1978 BASIC | FORTRAN / SST2K line | EGA Trek |
|---|---|---|---|
| death ray | absent | present | `RAY` |
| shuttlecraft | absent | present | in the repair array |
| planets, landing, crystals | absent | present | `LAND`, `ORBIT`, `USE` |
| self destruct | absent | present | `SELF` |

Four for four on the FORTRAN branch, none on the BASIC one.

`reference/sst2k/` is now a clone of ESR's super-star-trek
(https://gitlab.com/esr/super-star-trek). It is **BSD licensed**, which is
compatible with this repo's MIT, so it may be used with attribution and not
merely read. The closest thing to Anderson's source is
`historic/c-version/src/`, described in that repo as ESR's translation of the
UT FORTRAN version.

**How to use it: as a hypothesis generator, never as an authority.** Anderson
changed things, and the first thing checked proves it. But a specific
hypothesis costs one confirming shot; a law discovered from nothing costs
twenty.

### Laser falloff: the oracle proposes exponential, our data refutes it

SST's phaser damage (`battle.c`, `hittem`):

    dustfac = 0.9 + 0.01*Rand();
    hit = wham * pow(dustfac, kdist);

Exponential, roughly `0.9^d`. Ours is linear, `1 - d/12`. The two are close
at short range -- 0.729 against 0.75 at d=3, 0.531 against 0.5 at d=6 --
so a fit to short-range data alone could not tell them apart, which is
exactly the kind of thing worth re-checking.

The clean run already settles it. At d=1.414 with 500 units, linear predicts
441.1 and exponential 430.8; the game printed **441**. At d=5.0 with 250,
linear predicts 145.8 and exponential 147.6; the game printed **146**.
Exponential is excluded at both points. The measured linear law stands
unchanged, and Anderson demonstrably rewrote this formula.

### Laser heat: 1500 is the overheat threshold, and below it nothing happens

    static void overheat(double rpow)
    {
        if (rpow > 1500) {
            double chekbrn = (rpow-1500.)*0.00038;
            if (Rand() <= chekbrn) { ... phasers damaged ... }
        }
    }

`rpow` is the total energy fired in one phaser command. Three things line up:

- EGA Trek's Temp gauge is printed with a scale of `0 ... 1000 1500`. Full
  scale is the same 1500.
- Our own reading, "700 of 1500 costs nothing", is exactly what this predicts:
  at or below 1500 there is no effect at all.
- The penalty above 1500 is probabilistic, not gradual, which is why no
  gradual heat cost was ever found.

**Hypothesis, not yet confirmed for EGA Trek:** the Temp gauge reads total
laser energy fired in one LASERS command against a full scale of 1500, and
nothing is lost at or below it. One measurement discriminates: fire 750 in a
single volley and the bar should read half. A first attempt fired 300 and the
bar drew 20px of 121, where 300/1500 predicts 24px -- close but not exact, so
either the gauge decays, or it reads something slightly different.

### Enemy fire: proportional to the firer's remaining power, and firing drains it

This is item 7, the one three sessions failed on. SST's `attack()`:

    dustfac = 0.8 + 0.05*Rand();
    hit = kpower[loop] * pow(dustfac, kavgd[loop]);
    kpower[loop] *= 0.75;

Two things that would defeat exactly the measurements we attempted:

1. Hit strength is proportional to `kpower`, which is the enemy's **remaining
   hit points**. An earlier session concluded "output declines with damage
   taken" and then withdrew it. In the ancestor that conclusion is correct.
2. **Firing costs the firer a quarter of its power**, independent of damage
   taken. So a ship's output falls every turn it shoots, whether or not
   anything hit it. No measurement that ignores this can be stable, and ours
   did ignore it.

Distance enters as `kavgd`, the *average* distance to the enemy over the turn,
not the instantaneous one.

### Repair does not divide between systems, in the ancestor either

    repair = xtime;
    if (docked) repair /= docfac;        /* docfac = 0.25, so 4x docked */
    for (l = 0; l < NDEVICES; l++)
        game.damage[l] -= repair;        /* every device, full rate */

Every damaged device gets the whole elapsed time, in parallel. That is what we
measured in EGA Trek and recorded as contradicting its manual -- and the
manual's claim that engineers "divide their time evenly among all damaged
systems" is inherited fiction, wrong about the ancestor too.

**Testable immediately:** docking should repair 4x faster. EGA Trek's STATE OF
REPAIR dialog prints Docked and Undocked columns side by side, so the ratio
can be read straight off one screenshot with any system damaged.

### Torpedo damage against our own ship

    *hit = 700.0 + 100.0*Rand() - 1000.0*distance*fabs(sin(bullseye-angle));

700-800 at point blank, falling off with how badly the shot was aimed rather
than with range alone. Nothing measured for EGA Trek yet; recorded as the
shape to test.

## Confirming the ancestor against the game (2026-08-19)

The oracle's job is to propose; the game still decides. Four results, all from
one run of a level-3 game under dosbox-automation with the enemy table at
DS:25F2 read directly -- no pixel reading, which is what made this cheap.

### Enemies do NOT lose power by firing -- the ancestor's rule is REFUTED

SST's `attack()` ends every enemy phaser shot with `game.kpower[loop] *= 0.75`,
so a ship weakens itself by a quarter each time it fires. That was the headline
candidate for the confound that made three sessions of enemy-fire measurement
unstable.

Tested by firing lasers at **zero energy** -- a turn that costs time, moves
nothing and damages nothing -- five times in a row, reading the table each
turn:

| turn | Commander | second ship |
|---|---|---|
| 0 | 636 | 196 |
| 1 | 636 | 196 |
| 2 | 636 | 196 |
| 3 | 636 | 196 |
| 4 | 636 | 196 |
| 5 | 636 | 196 |

Not one point lost, while both were shooting at us throughout. **Anderson
dropped the drain.** So enemy output does not decay with shots fired, and
whatever destabilised the earlier measurements, it was not this.

Worth stating plainly: this is the oracle working as intended, not failing. A
specific hypothesis was cheap to kill. Discovering the same fact by measuring
blind is what cost three sessions.

### Enemies move, and our core does not model that at all

Across those same turns the Commander walked 3-8 → 3-7 → 4-6 → 5-5, one sector
per turn, closing on us, and then held. The second ship never moved. The
console narrates it: "SCANNER REPORT: The commander has moved. He is now at
5-5."

`trek_enemy_turn()` only fires. Stationary enemies are a large missing
behaviour, not a wrong constant.

### The Commander has 695 hit points at level 3, not 441..501

The table read 695 for the ship the console calls the Mongol Commander. The
earlier bracket of 441..501 was inferred from damage arithmetic and is wrong;
this is a direct read of the same table that gave 355 for a battleship.

A second ship read 255, which matches no class we have measured -- 355
battleship, 120 supply. Either it had already been damaged before we arrived,
or scouts are 255. ~~Unresolved.~~

**CLOSED 2026-08-27, and not by an emulator run.** `HP_SCOUT_AT(3)` is
`(3+4)*15 + 150` = **255 exactly**. The four class formulas read out of the
binary at 0x16119 on 2026-08-26 reproduce every reading in this session at
level 3 -- battleship 355, Commander 695, scout 255, supply 120 -- so the
255 ship was an undamaged SCOUT. This item had been carried on the emulator
list for a day after the evidence that closes it was already in trek.h.

### Vandal Death Pods are an area effect

"Vandal Death Pod enters quadrant: 59 unit hit on Lexington." In the same turn
both Mongols dropped by exactly 59 as well: 695 → 636 and 255 → 196. One
figure, applied to every ship in the quadrant including the enemy's own. This
first looked like evidence of the firing drain, which is a good example of why
the zero-energy turn was needed to isolate it.

### What the MAIN VIEWER shows

Enough to build ours. Against a starfield: the enemy class as a caption in the
top left ("MONGOL COMMANDER"), a line drawing of the ship, and two readouts in
the bottom left -- bearing in degrees after a slashed-zero glyph, and distance
in sectors after a triangle. With the Commander at 5-5 and the ship at 6,4 --
one row up and one column right -- the bearing read 45.0, so east is 0 degrees
and they increase anticlockwise.

## Docking: the manual outranks the ancestor (2026-08-19)

The ancestor resupplies everything at any base. EGA Trek's manual is more
specific, and where the two differ the manual wins -- it describes THIS game:

    "A StarBase is the most useful because you can replenish all ships
     supplies there. Supply stations can provide life support supplies and
     energy torpedoes. Research stations can provide only life support
     supplies." (l.356-359)

    "When docked at a StarBase its shields will protect your ship from enemy
     lasers." (l.440-441)

So the three base types are not interchangeable, and only a StarBase makes a
quadrant safe. Life support supplies are not a resource in our core -- life
support is one of the twelve repair percentages -- which leaves a Research
Station offering nothing but the docked repair rate. Faithful, if thin.

What IS taken from the ancestor: adjacency (any of the eight neighbouring
sectors, `abs(dx) <= 1 && abs(dy) <= 1`, which the manual's "directly
adjacent" agrees with), and the repair multiplier.

**Still to confirm: the 4x docked repair rate.** The ancestor divides the
repair period by `docfac = 0.25`. EGA Trek's own STATE OF REPAIR dialog prints
Docked and Undocked columns side by side, so one screenshot with any system
damaged settles it outright. Marked DERIVED until then.

### Presence relieves a besieged base

The ancestor's FBATTAK will not choose a base in the player's own quadrant --
`!same(game.state.baseq[j], game.quadrant)`. Read the other way round, being
there protects it, so arriving cancels a siege in progress.

That matters for more than fidelity. Without it the deadline in "they can last
until 3517.8" would be a countdown the player cannot affect, which is not what
a message like that is for.


## The kill-rate term, open item 13, resolved (2026-08-19)

Three sessions carried this: EGA Trek's evaluation sheet printed

    0.00  Kill/day ratio @ 500 per day.............0

against two kills in 1.2 elapsed stardates, which at 500 per day should have
been worth 833. Something gated it, and guessing at what was explicitly
forbidden here.

The ancestor supplies the shape and the condition, though not the whole answer:

    perdate = (initial_enemies - remaining) / timused;
    score_itemf("%6.2f Klingons per stardate  %5d", perdate,
                500 * perdate + 0.5);

    if ((timused == 0 || remaining != 0) && timused < 5.0)
        timused = 5.0;

Two things follow. First, **it is the same term** -- the coefficient is 500 in
both games, and EGA Trek's own sheet says "@ 500 per day". Second, and this is
the part that matters, the ancestor already singles out "enemies remain" as a
special case for THIS term and no other, inflating the divisor to a floor of
five stardates when the mission is unfinished.

That floor does not reproduce our reading: it gives 2/5 = 0.40, not 0.00. So
the ancestor's handling is refuted for EGA Trek. What survives is the
**condition**. Anderson gates on the same thing the ancestor clamps on, and
zeroes the term outright rather than inflating the divisor.

Still FITTED rather than measured -- one reading cannot distinguish "zero when
unfinished" from a much larger clamp -- but it is a far better guess than
before, because the condition is no longer invented. It was the earlier
session's own suspicion, and the ancestor now supports it.

**What would settle it:** finish a game. Every kill/day sheet we have is from a
quit, which is exactly the case where the term reads zero. One completed
mission at any speed shows whether the term fires at all, and its value pins
the divisor.

### Bases hit are ours, not theirs

The sheet carries "Enemy bases destroyed @ 50 each" as a positive item and
"Bases hit @ -200 each" as a negative one. They are different things: the
negative term is Union bases lost, which fits the manual's "You are
responsible for the protection of all bases in your designated area"
(l.360-361). Now that sieges can destroy a base, the core counts them.


## Torpedo damage, and why nothing survives one (2026-08-19)

Open item 9 has been stuck on a circular problem: torpedo damage cannot be
measured because no target has ever survived a torpedo, so no figure is ever
printed. The ancestor explains why. From `battle.c`, the damage a torpedo does
to an enemy -- the same expression it uses against our own ship:

    h1 = 700.0 + 100.0*Rand() - 1000.0 * distance * fabs(sin(bullseye-angle));
    if (kp < h1) h1 = kp;              /* capped at what the target has left */

700 to 800 on a dead-on hit, falling off with **aiming error** rather than
range: `bullseye - angle` is how far off the shot was, and a perfectly aimed
torpedo at any range loses nothing. Against a 355-hit-point battleship that is
overkill twice over, which is exactly what we have observed -- two for two,
kills with no figure printed.

**This names the experiment.** A Commander has 695 hit points (measured), so it
is the one target that can survive a shot, and only a poorly aimed one. Fire at
a Commander at long range where the aiming error is largest, and either it
survives and prints a figure, or it dies and puts a floor under the damage.
Either outcome is worth more than what we have.

DERIVED, not measured, and Anderson's record on rewriting formulas is mixed --
he kept the 1500 heat threshold and dropped the enemy firing drain outright.

## Hit points may not be constants (2026-08-19)

`core/trek.h` carries one hit-point figure per class: 355 battleship, 695
commander, 120 supply. The ancestor does not work that way. From `setup.c`:

    game.kpower[i] = Rand()*150.0 + 300.0 +  25.0*game.skill;   /* ordinary */
    game.kpower[i] = Rand()*400.0 + 450.0 +  50.0*game.skill;   /* commander */
    game.kpower[i] = Rand()*400.0 + 100.0 +  25.0*game.skill;   /* lesser    */

Every ship is **rolled within a band**, and the band's floor rises with skill.

That would explain the reading nothing else has: a second enemy in today's run
read **255**, which matches no class we have measured. Under fixed constants it
has to be a damaged ship or an unmeasured class; under a rolled band it is
simply an ordinary draw.

It also fits what we already have. Battleships read 355 at level 3 and 325 at
level 1 -- a difference of 30 across two levels, which is as consistent with
`base + level * k` as with two fixed constants, and both readings could sit
inside one band.

**Cheap to test, and it should be done before any more hit-point constants are
trusted:** enter several quadrants and read the enemy table at DS:25F2 in each.
If every battleship reads exactly 355 the constants stand. If they scatter, the
core needs a band per class and `HP_BATTLESHIP` and its neighbours become the
wrong shape rather than the wrong value.


## The ancestor's motion constants are on its own power scale (2026-08-19)

`enemy_motion()` is DERIVED from `movebaddy()`, and its shape is right --
advance, hold or retreat from a score built out of the enemy's own power, how
many are present, and how dangerous we look. Two things came out of testing it
properly.

**A real bug, found by a shields test.** The "100 per enemy present" term was
reading `ship.enemies_left`, the GALAXY-wide count, where the ancestor uses
`game.nenhere`, enemies in the current quadrant. On a fresh level-3 game that
added three thousand-odd to every score, so `forces > 1000` was always true and
every enemy charged regardless of anything else -- shields, our energy, its own
damage. Fixed to count the quadrant. Worth noting the bug was invisible while
it existed: enemies charging looks like enemies working.

**And then the constants showed themselves.** With the inflation gone, almost
nothing advances. The reason is scale. SST's commander rolls
`950 + 400*Rand() + 50*skill`, so 950-1500; ours is 695, and its ordinary
Klingon is `Rand()*150 + 300 + 25*skill`, which is about where our BATTLESHIP
sits at 355. EGA Trek's whole power scale is roughly SST's ordinary-ship scale.
The thresholds `forces/150 - 5` and `forces > 1000` are calibrated to the
former and simply do not mean the same thing against the latter.

A lone healthy Commander with our shields down scores 845 under them, which
lands in the hold band. The one observation we have says it should advance:
3-8 -> 3-7 -> 4-6 -> 5-5, one sector per turn.

**Not refitted, deliberately.** Three free parameters and one observation is
not a fit, it is a curve drawn through a point. What the tests assert instead
is the RELATIVE behaviour, which is certain because it follows from the terms
themselves: lowering our shields makes an enemy no less aggressive, and a
crippled ship (shields down, banks empty, no torpedoes) invites attack.

**What would settle it:** a handful of observations at known ship states.
Sit in a quadrant with one enemy, record its movement each turn while varying
shields up/down and energy high/low. That is a `make monitor` script now
rather than a human watching, so it is cheap -- and it is the same instrument
that would settle the four other DERIVED numbers still outstanding.

## Enemy motion: the ancestor's per-turn model is REFUTED (2026-08-20)

Open item 14 asked for "a handful of observations at known ship states" to
calibrate `enemy_motion`'s three constants. The observations say there is
nothing to calibrate: EGA Trek does not run `movebaddy` per turn at all.

### Method

Level 3, quadrant 8-2, two enemies read straight out of the table:
a **Commander at 3-8 with 695 hit points** and a **Battleship at 4-7 with
355**. Both figures match the classes the console names, which is the third
independent confirmation that hit points are fixed per class.

Every input to `forces` was pinned by writing memory between turns -- energy
to 5000, the twelve system percentages to 100, and in the first block the
shields to 2500 -- so the only thing varying across turns was the game's own
randomness. Each turn was a one-sector impulse move between two adjacent
sectors, so our own position stayed effectively constant.

| block | turns | shields | result |
|---|---|---|---|
| A | 8 | up | no enemy moved |
| B | 8 | down | no enemy moved |
| C | 10 | down, clock forced across the 3502.0 event | no enemy moved |

Twenty-six turns, zero movement by either ship.

### What each model predicted

**Our port**, shields up: `forces = 695 + 200 - 500 - 450 = -55`, saturating
to 0, so `motion = -5` clamped to -3 -- retreat at full speed, every turn.
Not observed.

**The ancestor**, shields down: `forces = 623 + 200 + 1000 - 500 - 450 = 873`,
so `motion = (873 + 200*Rand())/150 - 5` lies in 0.82..2.15 and truncates to
1 or 2 whenever `Rand() >= 0.135` -- an advance on about 86% of turns. Zero
advances in eight turns puts this at p ~ 1.5e-7. Refuted.

The shields-up case is the weaker of the two, because the Commander sits at
x=8 against the east wall and "retreat" has nowhere to go; holding is
consistent with the ancestor there. The shields-down block is the one that
kills it, and it kills it flatly.

### Two things the ancestor does say, and they are structural

Reading `moveklings()` rather than `movebaddy()` was the mistake all along.
The caller gates who moves at all:

    if (game.comhere)  ... movebaddy(..., IHC);   /* commander */
    if (game.ishere)   ... movebaddy(..., IHS);   /* super commander */
    if (game.skill >= SKILL_EXPERT && (game.options & OPTION_MVBADDY))
        ... movebaddy(...)                        /* everyone else */

**Ordinary ships do not move below expert skill.** Our port moves every enemy
every turn, which is a larger error than any constant in it, and the
Battleship holding still for 26 turns at level 3 is exactly right.

The one historical observation of movement -- a Commander walking
3-8 -> 3-7 -> 4-6 -> 5-5 -- was announced as "SCANNER REPORT: The commander
has moved." A scanner report is how the ancestor narrates *scheduled*
commander movement (`FSCMOVE`, `schedule(FSCMOVE, 0.2777)`), not per-turn
tactics, which narrate as "retreats to" / "advances to". Combined with 26
turns of nothing under conditions where per-turn tactics predict near-certain
motion, commander movement in EGA Trek is an **event on the queue**, not a
per-turn decision.

### Consequence

`enemy_motion()` should not be refitted; it should be replaced. The refit
that item 14 asked for would have fitted a model that does not apply. What
the port needs instead is:

1. Movement gated to commanders only (at these command levels).
2. Commander movement driven from the event queue, which the core already has.

The period of that event is the one number still unmeasured, and it is the
only thing item 14 still wants. The ancestor's 0.2777 stardates is the
starting guess.

### Incidental, from the same session

* **Laser damage confirmed again, exactly.** 400 units at range 4.472 dealt
  251: `400 * (1 - 4.472/12) = 250.9`. Same figure the earlier session read,
  reproduced from a cold start.
* **Enemies still do not lose power by firing.** Hit points held at 695/355
  across every firing turn. The ancestor's `kpower` drain stays refuted.
* **Vandal Death Pods, twice more.** "Vandal Death Pod enters quadrant: 72
  unit hit on Lexington" while both enemies dropped by exactly 72
  (695->623, 355->283); a second pod took 83 off all three. One figure
  applied to every ship in the quadrant, our own included.
* **Impulse moves cost ~0.042 stardates per sector** and roughly 110 energy;
  a multi-sector move cost 495.

## The scoring sheet, read live (2026-08-20)

Flying into a black hole on the first game turned out to be the cheapest
measurement of the session: losing the ship prints the Detailed Evaluation,
which is the whole scoring rubric with its coefficients.

| item | rate | this game |
|---|---|---|
| Rescues | 200 each | 0 |
| Penalty for incomplete mission | -300 | -300 |
| Mongols killed | 10 each | 0 |
| Commanders killed | 20 each | 0 |
| Enemy bases destroyed | 50 each | 0 |
| Kill/day ratio | 500 per day | 0.00 |
| **Casualties on board Lexington** | **-1 each** | **-430** |
| Stars destroyed | -5 each | 0 |
| Bases hit | -200 each | 0 |
| TOTAL | | **-730** |

Every coefficient we had is confirmed. Two corrections fall out of it:

**There is no ship-loss line.** `-300 + -430 = -730` exactly, with nothing
left over. Losing the ship is scored as the death of the crew -- all 430 of
them, at a point each -- and `SCORE_SHIP_LOST (-200)` in trek.h is a line
item the original does not have. Our score for this same game would have been
-930.

**430 is the crew complement**, and losing the ship kills all of them. The
core tracks `ship.casualties` and adds to it from combat, but never sets it to
the full complement on death, so it would have under-counted as well as
double-counted.

`SCORE_PER_ENEMY_BASE` is also defined in trek.h but never summed in
`trek_score()`.

## Reading the original's memory: what shifts and what does not (2026-08-20)

The addresses in this file are per-run and must be re-located every launch.
Between two launches of the same binary on the same machine the ship record
moved **+16 bytes** (energy 181940 -> 181956) and the enemy table moved with
it (184146 -> 184162, a fixed +2206 from energy). The twelve system
percentages did **not** move: they stayed at 183498 both times.

So they are separate allocations and no single base fixes them all. Anchor
each structure independently:

* **ship record** -- scan for the Turbo Pascal reals 5000.0 and 2500.0. The
  pairs six bytes apart are (current, display latch), NOT (current, maximum):
  both halves fall together when shields take a hit.
* **enemy table** -- energy + 2206. Six-byte records of `y, x, hit points` as
  three 16-bit integers, zero-terminated.
* **systems** -- a run of twelve 16-bit percentages; find it by the run, not
  by an offset. Index 9 is the computer.
* **live stardate** -- 181938 this run. 181926 is the *start* stardate and
  never changes, which is a trap: it reads a plausible 3500.0 forever.

The two unidentified reals just past the ship record are the **event queue**,
holding absolute stardates (3502.0 and 3507.31 on a fresh game).

**Check for game-over before trusting any read.** A five-turn firing loop
late in the session returned five identical, entirely plausible snapshots;
the ship had been destroyed on the first of them and the game had restarted
into its setup screen, so every reading after that was stale memory at
addresses the new process had not yet touched. Nothing in the numbers looked
wrong.

## Open item 7, the enemy fire law: shape found, our constants refuted (2026-08-20)

The motion session recorded damage messages it was not looking for, and they
turn out to be the best enemy-fire data we have. Item 7 previously said "no
enemy-fire constant should be taken from this data" because the firer's class
was not recorded. This time the enemy table gives the firer's **exact
remaining hit points**, which is better than its class, and our own sector is
a memory read, so range is exact too.

Nine hits, from a Commander at 3-8 and a Battleship at 4-7:

| shields | our sector | firer hp | range | damage |
|---|---|---|---|---|
| up   | 8,4 | 695 | 6.403 | 272 |
| up   | 8,4 | 355 | 5.000 | 143 |
| up   | 8,5 | 623 | 5.831 | 266 |
| up   | 8,5 | 283 | 4.472 | 117 |
| down | 8,6 | 623 | 5.385 | 313 |
| down | 8,6 | 283 | 4.123 | 182 |
| down | 7,6 | 540 | 4.472 | 334 |
| down | 7,6 | 200 | 3.162 | 154 |
| down | 7,6 | 200 | 3.162 | 147 |

The first four pairings are arithmetically certain, not inferred from message
order: `2500 - 272 - 143` and `2500 - 266 - 117` reproduce the shield readings
2085.074 and 2117 exactly. So the printed figure is the whole amount that
leaves the shield pool.

### The shape

Damage is proportional to the firer's **remaining** hit points -- not to its
class -- and falls off linearly with range. Fitting `dmg = k * hp * (1 - d/L)`
over all nine points, the spread of k is minimised near **L = 10** (cv 0.13);
dropping the range term entirely doubles the spread to 0.26, so the falloff
is real. Exponential falloff fits no better than linear (cv 0.13 at a scale
length of 6), which the nine points cannot separate.

The five shields-down readings alone are startlingly tight:

    dmg = 1.108 * hp * (1 - d/9.86)

reproduces all five to about 1%. Rounding that to `hp * (1 - d/10)` with a
coefficient near 1.1 predicts the very first observation -- 695 hit points at
range 6.403 -- as 275 against the 272 printed.

### What it says about our core

`enemy_fire_energy()` is wrong in two ways.

**It fires far too weakly.** Our Commander at full strength fires 300, which
at range 6.403 lands `300 * (1 - 6.403/12) = 140`. The original delivered
**272** in that exact situation. Our enemies hit at roughly half strength,
which is a large part of why the port's combat feels harmless.

**The per-class table is unnecessary.** Our model scales a per-class base by
`hp/full`, and those bases imply a different coefficient for every class
(Commander 0.43, Battleship 0.56, Supply 0.83). The data wants one coefficient
for everybody, applied to current hit points -- which already carry the class
difference, since that is what hit points are. `ENEMY_FIRE_BATTLESHIP`,
`_COMMAND`, `_SCOUT` and `_SUPPLY` would all go.

### CORRECTED 2026-08-21: the shields split was an artefact

The controlled run below tested exactly the worry stated in this section, and
the worry was justified. Holding range, hit points, ship and sector fixed and
toggling only the shields gives **358.4 with shields up and 367.5 with them
down** -- a 2.5% difference on five firing turns each, which is nothing. The
0.76 / 0.98 split above is the range confound it was flagged as, plus Vandal
Death Pods counted as enemy fire. The falloff length of ~10 fitted here is
also wrong; see the controlled figures.

What survives from this section is the part that mattered: damage is
proportional to the firer's REMAINING HIT POINTS and not to its class, and
our enemies fire far too weakly. Both are now confirmed directly.

### Not yet changed, and why

Nine observations with two variables moving together: every shields-down
reading is also at closer range than every shields-up one, so "shields down"
and "close" cannot be separated here, and the shields-up group's coefficient
(0.92) sits below the shields-down group's (1.10). That gap may be a real
effect, or it may be the range confound.

What settles it is a controlled run, which this rig can now do: hold our
sector fixed, do not fire (so enemy hit points stay put), and take many turns
at one range with shields up, then the same range with shields down. An
afternoon's worth of turns would give the coefficient, the falloff length,
and the size of the random component separately.

## Two smaller items the same session moved

### Open item 15, hit points per class: effectively settled

A second galaxy, generated from a cold start days after the first, produced a
Commander at **695** and a Battleship at **355** -- the same figures to the
unit. Across two independent games that is 695 twice and 355 four times, with
no variation. Hit points are fixed per class, not rolled per ship.

The unexplained 255 reading from the earlier session is still unexplained, but
"already damaged before we arrived" is now much the likelier reading of it,
since nothing else has ever varied.

### Open item 1, the enemy count spread: base confirmed, range widened

Two fresh level-3 games gave **34** and **38** Mongols. Every previous
level-3 reading was 40, 42, 42, 42 -- all crowded at the top of the fitted
0..12 offset, which was mildly suspicious. Offsets of 4 and 8 fill in the
middle and are the first evidence the spread is genuinely wide rather than
the base being 40.

Level 3 now spans 34..42 across six games, all inside the `level*10 +
rand(0..12)` we fitted. The base of ten per level survives a test it could
have failed; the upper limit of the spread is still only bounded from below.

## The controlled fire run (2026-08-21)

The rig: one enemy in the quadrant, its hit points **written back every turn**
so the firer's power is a controlled variable, our sector fixed, and the turn
taken by firing a nil volley so nothing about our own state changes. Damage is
read as the fall in the shield and energy pools, both pinned before each turn,
and corrected for Vandal Death Pods by the fall in the *enemy's* pinned hit
points -- a pod hits every ship in the quadrant for one figure, so the enemy
measures the pod for us.

### Damage is proportional to hit points, not to class -- DECISIVE

The cleanest result of the project so far, and it needed no statistics.

A **supply ship** with its own 120 hit points sat at range 2.83 and was
effectively silent: eight turns produced one ambiguous reading and otherwise
nothing. Writing **695** into that same ship's hit points -- same ship, same
class, same sector, same range, nothing else touched -- it immediately began
firing for 495.7, 514.7, 498.9, 527.0.

So the per-class fire table in our core has nothing to model. `hp` already
carries the class difference, because that is what hit points are.
`ENEMY_FIRE_BATTLESHIP`, `_COMMAND`, `_SCOUT` and `_SUPPLY` should all go, and
`enemy_fire_energy()` reduces to a coefficient on current hit points.

It also explains the supply ship's silence without any special case: at 120
hit points its volley is about 85, which is smaller than a death pod, and it
was being lost in them.

### Shields up or down makes no difference -- REFUTES the earlier reading

Same enemy, same 695 hit points, same sector, same range 5.657, toggling only
the shields:

| shields | turns fired | mean damage | damage/hp |
|---|---|---|---|
| up | 5 of 10 | 358.4 | 0.516 |
| down | 5 of 10 | 367.5 | 0.529 |

2.5% apart. There is no shields term in the damage the game reports. The
0.76-vs-0.98 split derived from the earlier uncontrolled data was the range
confound plus uncorrected pods, and is withdrawn.

### Enemies fire on about half of turns

Across every block: 5/10, 5/10, 7/10, 4/8. This is not lost turns -- a lost
turn shows up as a zero followed by a doubled reading, which is exactly what
the instrument did before it was fixed, and the surviving figures are tight
(±5%) with no doubles. Enemies genuinely hold fire roughly half the time.

Our `trek_enemy_turn()` fires every enemy every turn.

### Range matters, but not as a straight line to zero

Same ship, same 695 hit points, pod-corrected, shields up:

| range | n | damage/hp |
|---|---|---|
| 2.828 | 4 | 0.733 |
| 4.243 | 7 | 0.511 |
| 5.657 | 10 | 0.522 |

The two long ranges are **indistinguishable**. That rules out the linear
falloff to zero at 12 that our laser law uses and that the earlier fit
proposed -- under any such law 5.657 should deliver a third less than 4.243,
and it delivers slightly more. The shape looks like a falloff that flattens
into a floor somewhere around half the firer's hit points, but three ranges
cannot settle that, and all three sampled here are diagonal offsets (2,2),
(3,3) and (4,4), which is a poor design: it leaves the possibility that the
game keys on something other than Euclidean distance untested.

**Next run should vary the offset shape, not just its length** -- (0,4),
(1,4), (3,3) and (4,0) all at similar Euclidean distance -- and add ranges
below 2.8, where the only reading is from a different game and disagrees with
the trend by 21%.

### What the port should change now -- DONE in 1f9808b

Confirmed enough to act on: drop the per-class fire table for a coefficient on
current hit points, and make enemies hold fire on roughly half of turns. The
exact coefficient and the range curve are not settled and should wait.

Both landed the same day. `ENEMY_FIRE_BATTLESHIP`, `_COMMAND`, `_SCOUT` and
`_SUPPLY` are gone; `enemy_fire_energy()` takes hit points and nothing else,
at `ENEMY_FIRE_PCT` 90; and `trek_enemy_turn()` skips a firer one turn in
`ENEMY_FIRE_ONE_IN`. The falloff is deliberately left as our laser law, which
these measurements say is the wrong shape, because three diagonal ranges are
not enough to replace it with.

Worth knowing before judging the difficulty: the two changes very nearly
cancel. A fresh Commander fired 300 every turn before and now fires 625 on
half of them, so its average output moves about 4%. What changes is the
variance -- combat becomes occasional heavy hits rather than a steady drip,
which is what the original feels like.

### Method, dearly bought

Four instrument faults, each of which produced confident wrong numbers:

1. **The enemy table is not zero-terminated.** Killing a ship zeroes its
   record in place; a quadrant that started with four and lost three reads as
   `[live, 0, 0, 0, live]`. Stopping at the first zero hid an enemy that had
   wandered in from another quadrant, and its fire was being attributed to the
   ship being studied.
2. **A pool drop is not enemy fire.** Death pods hit us and every enemy for
   one figure. Against a 120-hit-point ship the pods were most of the signal.
3. **The pools settle after the volley animation.** Reading early gives a
   zero and lands that damage on the next reading, doubling it -- the 0 / 2x
   pattern that first looked like the enemy firing on alternate turns.
4. **The weapons dialog asks once per live enemy, and a nil volley still
   raises "Hit enter to continue."** Missing either swallows the next command
   and drops a turn.

The earlier session's nine readings came from the damage-report TEXT, which
names the firer. That method was immune to faults 1 and 2 and was, in
hindsight, the better instrument.

### One motion observation, against the grain

The Commander moved (1,5) -> (2,6), closing on us, on a turn when we FIRED
rather than moved. Twenty-six consecutive move-turns last session produced no
motion at all. That is one observation, but it points at the trigger being
something other than the passage of a turn, and it is worth designing for
directly: the same null-volley rig, watching position rather than damage.

## Four open items the fire run moved on the way past (2026-08-21)

### Open item 15, the odd hit points, is explained -- by pods

Two readings have never matched a class: 255 from the first session and 320
from this one. Both were ships we had never shot at, which made "already
damaged" look like special pleading.

This session watched the mechanism happen. Four enemies in quadrant 7-6 read
626, 286, 286 and 286 -- **each exactly 69 below** the class values 695 and
355 -- because a Vandal Death Pod had been through before we arrived. Pods hit
every ship in a quadrant for one figure, and they roam. So any enemy we meet
may already carry damage from a pod we never saw, and 255 and 320 need no
explanation beyond that.

> **PARTLY RETRACTED 2026-08-28.** "They roam" has no mechanism in the binary.
> The pod does not move, and the detonation's damage loop
> walks the CURRENT quadrant's object table -- so it cannot damage ships in a
> quadrant you are not in. The 626/286/286/286 reading stands as an
> observation; what it shows is a detonation DURING that visit, not a pod that
> had "been through before we arrived". 255 was separately explained below as
> a scout at full strength. See NOTES.md, 2026-08-28.

Hit points are fixed per class. Nothing observed contradicts it, and the one
thing that appeared to has a mechanism.

(`HP_SCOUT` is still 100 and still unmeasured; neither 255 nor 320 should be
read as evidence for it.)

### Open item 8, the heat threshold: nothing happens below 1240, not 400

A single command fired 450, 460 and 330 -- **1240 units in one volley** -- at
three battleships holding 286 hit points each, from sector 3-7. All three
died, which puts a floor under the laser efficiency at the moment each shot
landed:

| target | energy | range | damage at full efficiency | implies |
|---|---|---|---|---|
| 7-7 | 450 | 4.000 | 300.0 | eff >= 95.3% |
| 7-6 | 460 | 4.123 | 301.9 | eff >= 94.7% |
| 3-8 | 330 | 1.000 | 302.5 | eff >= 94.5% |

So firing 1240 in one command leaves the lasers at 95% or better, and the
last target in the volley is as unaffected as the first -- heat does not
accumulate *within* a volley either. The previous bound was 400. DERIVED 1500
survives, with the untested band now 1240..1500 rather than 400..1500.

### Open item 1, the enemy count: eight level-3 games now, and a specific doubt

Two more level-3 games, both **37**. The full set is now
40, 42, 42, 42, 34, 38, 37, 37 -- offsets from `level*10` of
10, 12, 12, 12, 4, 8, 7, 7.

Our `rand_n(13)` gives a uniform 0..12 with a mean of 6. The eight samples
average **9.0**, and not one has landed in 0..3 -- which a uniform model makes
a 5% event. The base is probably higher than ten per level, or the spread
narrower, or both.

Not refitting on this. But it is now a stated, testable doubt rather than a
gap, and six more level-3 games would settle it: if the low end really is 34,
values of 30..33 will keep not appearing.

### Open item 14, enemy motion: the trigger looks like the player's action

The Commander moved (1,5) -> (2,6), closing, on a turn when we **fired a nil
volley** rather than moved. Twenty-six consecutive move-turns the day before
produced no motion at all from two enemies.

That is one observation and it is not a law, but it is the first thing that
has ever correlated with motion, and it is cheap to test properly: the same
null-volley rig, alternating fire-turns and move-turns at a fixed position,
watching position instead of damage.

SST's forces formula still does not explain it either way -- with our shields
up it computes a retreat for this Commander, and the ship advanced.

## Open item 9, torpedo damage: capped at 355, falling with range (2026-08-21)

The item had been stuck because no target ever survived a torpedo, so no
figure was ever printed. The way past it is to make the target survive:
**write a large hit-point value into its record before the shot**. It then
takes the hit and the table reports the damage the game never prints.

Method: follow a ship by its **table slot** rather than its sector, pin that
slot's hit points, aim at whatever sector the slot currently occupies, and
diff the WHOLE table across the shot. See "How this went wrong" below.

| range | shots | hits | damage |
|---|---|---|---|
| 1.414 - 2.236 | 6 | 6 | 355, 355, 355, 355, 355, 355 |
| 5.000 | 6 | 6 | 210, 355, 355, 247, 209, 296 |
| 7.616 | 7 | 4 | 176, 209, 229, 247 |

**355 is a cap, not the damage.** At short range it binds every time, with
zero variance across six shots -- which is why one torpedo has always killed a
355-hit-point battleship exactly, and why nothing had ever survived to print a
number. By range 5 the cap only binds sometimes (twice in six), and by 7.6 it
never does.

Means: 355 at short range, 279 at 5.0, 215 at 7.6. So there IS a range term,
and there IS a random component -- it is simply invisible at close range
because the cap swallows it.

The cap is **355 regardless of the pin**: the target here was pinned to 700
and 1500 at different times and never took more than 355. 355 is a
battleship's class hit points, and this target was a battleship. Whether the
cap is the class figure or the ship's true current hit points is NOT
distinguished by this data -- both were 355.

An earlier reading has a Commander taking 355 as well, which is the tactically
important one: **a Commander survives a torpedo**, at 695 - 355 = 340, and
needs two. Our `trek_fire_torpedo()` zeroes whatever it hits, so the port
cannot express that.

The ancestor's `700 + 100*Rand()` falling off with aiming error is no longer
refuted, as an earlier draft of this section wrongly concluded from the
short-range data alone. Fit is loose: the spread at 5.0 is at least 146 and at
7.6 about 71, which one uniform 0..100 term does not obviously produce. Not
enough to pin a formula.

## Open item 10, torpedo accuracy: real, and it falls with range

Torpedoes are aimed at a **sector**, not an angle. Misses are genuine and the
game announces them -- *"Clean miss, sir!"* -- observed on screen, which is
also the message text the port should use.

| range | hit rate |
|---|---|
| 1.414 - 2.236 | 6 / 6 |
| 5.000 | 6 / 6 |
| 7.616 | 4 / 7 |

Perfect accuracy out to range 5, degrading beyond it. There are two further
ways to miss that are geometry rather than a roll, and both must be excluded
before reading the table above as a pure accuracy curve:

1. **The target moves.** Enemies move after every volley, so a torpedo aimed
   where a ship was arrives at an empty cell. Five consecutive shots read as
   misses while the enemy sat unmoved in a stale table record; following the
   live table fixed it. The figures above all aim at the slot's current
   sector, so they are clean of this.
2. **Something is in the way.** The ray is walked cell by cell and detonates
   on the first occupant, which need not be the target -- see the supernova
   below.

### Landed in the port

`trek_fire_torpedo()` now takes a `uint16_t *damage` out-parameter, deals
`min(355, 500 * (1 - d/12) + rand(0..100))`, and returns `TORP_OK` when the
target lives -- a branch that was unreachable before, because the old code
zeroed whatever it hit. Accuracy is certain inside five sectors and degrades
at 16% per sector past it. The C128 reports a survivor as "MONGOL DAMAGED --
n UNIT HIT" and a miss in the original's own words, "CLEAN MISS, SIR!".

Still NOT implemented, deliberately: the ray-march, and therefore the
supernova below. One observation, and several unknowns behind it -- whether
every enemy always dies, where the ship is thrown, how the damage scales. It
needs its own session.

## Torpedoes detonate stars, and a supernova is enormous

Firing from 5-3 at an enemy in 2-7, the ray passed through a star at 4-4:

    Star at 4-4 goes supernova!
    Lexington blown to quad 8-4.
    2 Mongols destroyed.
    46 unit hit absorbed by shields.

Every enemy in the quadrant died, our ship was **thrown into another
quadrant**, and we took damage. The chart then marks the burnt quadrant
`999`. NOTES item 14 lists "stars destroyed, -5 each" as a scoring line that
needs a torpedo able to hit a star; it is much more than a scoring line.

## How this went wrong, four times

Worth recording because three of the four produced plausible numbers.

1. **Pinning the target's hit points makes it fire proportionally harder.**
   A target pinned to 20000 fires roughly `20000 * 0.9 * falloff` -- about
   14000 -- and destroyed us in one shot. The very finding that makes this
   experiment possible (damage is proportional to hit points) is what makes a
   large pin lethal. 700-1500 is the usable window: above any torpedo, below
   anything that one-shots a pinned 2500 shield.
2. **The stardate guard could not see a quit or a death**, because a score
   screen does not move the stardate backwards -- only a *new game* does. The
   check that works is a **video** check: the COMMAND panel's magenta is
   present on every console frame, dialog open or not, and absent on every
   end screen.
3. **Aiming by sector re-targets whoever wandered in.** Ships move after
   every volley, so the ship at a sector is not the ship that was there.
   Follow the slot.
4. **Enemies migrate into the quadrant mid-fight.** A quadrant the chart
   called `104` -- one enemy -- held four a few turns later.

Pinning systems before a turn does not protect them during it either. The
real protection is keeping the shield pool above the incoming volley so
nothing penetrates.

## The damaged-ship block (2026-08-21)

Run by writing damage straight into the twelve system percentages rather than
getting shot at, which makes the whole block safe and repeatable.

### Open item: the docked repair rate. 2.35x, not 4x -- DERIVED value REFUTED

`R)epair` prints Docked and Undocked times side by side, so one screenshot with
a system damaged reads both rates at once. Six readings:

| points to repair | docked | undocked |
|---|---|---|
| 100 | 2.1 | 5.1 |
|  90 | 1.9 | 4.6 |
|  60 | 1.3 | 3.0 |
|  45 | 1.0 | 2.3 |
|  35 | 0.8 | 1.8 |

Solving across all of them, allowing for one-decimal rounding: **undocked
~19.75 points per stardate, docked ~46.6**. The advantage is about **2.35x**.
The ancestor's `1/docfac = 4x` was DERIVED and is refuted -- it was flagged in
trek.h as the one number there worth checking, and checking it was worth it.

Two things come free with it. The undocked figure confirms
`REPAIR_PER_STARDATE 20` to within the display's rounding. And the rate was
identical with one system damaged and with three, which independently confirms
that repair does not divide between systems -- previously measured by watching
repair happen, now confirmed from the game's own predicted times.

The core now carries `REPAIR_PER_STARDATE_DOCKED 47` rather than a factor.

### HP_SCOUT is 255, and the long-unexplained reading was never damage

The `INFO` panel names the 255-hit-point ship **"Mongol Scout"** at
**Shields: 100%**. `HP_SCOUT` had been 100 and marked unmeasured.

That also closes the 255 that has been sitting unexplained since the first
session, and the guess recorded for it -- "already damaged before we arrived"
-- was wrong. It was a scout at full strength all along.

`INFO` shows a silhouette, class name, Sector, Range, Bearing and Shields as a
percentage, with up/down to step through the enemies present and ESC to exit.
So the game calls an enemy's hit points its **shields**, and shows them as a
percentage of the class maximum -- which is why the table's raw figure never
appears on screen.

### Laser heat: the gauge animates, and it resets on leaving the quadrant

Jamie watched this happen and it explains a run of inconsistent readings.
The Temp bar **rises gradually** after a volley rather than jumping, so a
screenshot taken at an arbitrary moment catches it mid-climb -- which is how
the same 400-unit volley read 26 pixels in one session and 6 in another. And
it **resets to zero on changing quadrant**, which is the other half: several
of the readings straddled a warp.

So heat accumulates across volleys within a quadrant visit and is cleared by
leaving. trek.h currently says "per COMMAND, not cumulative over the game",
which is wrong in the first half and right in the second.

The value itself is still not located: it is not stored as a Turbo Pascal real
or a 16-bit integer of the fired amount anywhere in the 338KB, so the gauge
remains the only readout and it needs a settle loop to read honestly. The
useful bound stands from elsewhere -- a 1240-unit volley leaves efficiency at
95% or better -- and that came from damage arithmetic, not from the bar.

### What kills the ship when energy cannot: life support has a reserve

Writing Life Support to zero does NOT destroy the ship. The SYSTEMS STATUS
panel is **replaced by a "LIFE SUPPORT -- RESERVE, DAYS" gauge** counting down
0/1/2.

That resolves the death recorded as unexplained on 2026-08-20, where the ship
was destroyed on a turn when energy had just been pinned to 5000. Jamie's
reading is the right one: it was the rig, not a mystery. That session pinned
the systems only BEFORE each turn, so life support could be knocked out during
one and sit at zero across the gap, turn after turn, until the reserve ran
out. The current rig repairs on both sides of every turn and the problem has
not recurred.

It is still a real mechanic we do not model: `SYS_LIFE` reaching zero should
start a countdown that kills the crew, and the console should swap that panel.

### Also seen

* **Plasma bolts** are a distinct enemy weapon -- "639 unit hit from plasma
  bolt", far above anything the laser law produces at that range.
* **Casualties are reported by deck**: "There are 7 casualties reported on F
  Deck", and they come from system damage rather than only from the hull.
* The **crew is 387 enlisted and 43 officers**, from the briefing's first
  page. That is 430, confirming `CREW_COMPLEMENT` from a source completely
  independent of the score sheet it was derived from.
* The chart's middle digit is the **base TYPE, not a count** -- the briefing
  says so outright: "the number of Mongols, base type, and number of stars".

## Audit of the open items after 2026-08-21

Sweeping the day's measurements against everything still open.

### Closed

**Open item 9, torpedo damage** -- capped at 355, falling with range beyond
about 3.5. Measured over nineteen shots at three ranges.

**Open item 10, torpedo accuracy** -- certain to range 5, degrading past it.
The original announces a miss as "Clean miss, sir!".

**[BOTH RE-READ 2026-08-26. The shots are right and the model drawn from them
was wrong. 355 is not a cap, it is `(level+4)*15 + 250`; there is no falloff;
and accuracy is an ANGLE, not a distance, because the original ray-marches
with a ONE-SIDED wobble. See the torpedo section at the end of this file.]**

**Open item 15, are hit points fixed per class** -- YES, and every class is now
measured: Commander 695, Battleship 355, **Scout 255**, Supply 120. The scout
figure came from the INFO panel naming the ship, and it removes the last
unmeasured hit-point constant.

Both readings that had ever looked anomalous are accounted for. **255 was a
scout at full strength**, not a damaged battleship as this file previously
guessed. The scattered values -- 610, 535, 345, 320, 230 -- are Vandal Death
Pod damage, which hits every ship in a quadrant by the same amount. That
mechanism was watched directly: four enemies reading 626, 286, 286, 286, each
exactly 69 below their class figure.

> **CORRECTED 2026-08-28.** This used to end "and roams between them, so any
> enemy may already be carrying some when we arrive". The detonation damages
> only the quadrant you are standing in, so the damage is picked up while you
> watch, not before you arrive. The readings are unaffected; the inference
> about unseen quadrants is withdrawn.

**The docked repair rate** -- 2.35x, not the ancestor's 4x. Refuted.

**Does laser Temp accumulate across volleys** -- YES, within a quadrant visit,
and it is **cleared on leaving the quadrant**. The gauge also animates upward
rather than jumping, so it must be read after it settles. Previously listed as
an open DERIVED question; trek.h's "per COMMAND, not cumulative over the game"
is wrong in its first half.

**What can destroy the ship when energy cannot** -- life support at zero starts
a RESERVE, DAYS countdown. Not a mystery mechanic; the death recorded as
unexplained was an artefact of a rig that repaired only before each turn.

### Advanced, not closed

**Open item 14, enemy motion.** Three of its four parts are now settled:

* *Who* -- commanders only. Watched across a full session, and it is the
  ancestor's own gate.
* *When* -- on turns the player fires, not merely when a turn passes.
* *Which way* -- toward the ship, one sector at a time. Six sightings, and not
  one retreat has ever been observed.

*How often* is what remains, and it is **intermittent**: three turns running in
one sighting, once in four or five in another. That intermittency is itself
evidence, because a deterministic score cannot produce it -- which is why the
ancestor's `200*Rand()` jitter has now been restored to `enemy_motion()`. It
was dropped when the function was first ported, spotted days ago, and set aside
when the motion model looked refuted. With movement confirmed real but
intermittent, it is the term that makes the observed pattern possible.

The absolute thresholds are still on the ancestor's power scale and still not
refitted.

**Open item 1, the enemy count.** Two more level-3 games, both read at the
start: 38 and 38. The series is now 40, 42, 42, 42, 34, 38, 37, 37, 38 -- nine
games, offsets from `level*10` of 10, 12, 12, 12, 4, 8, 7, 7, 8. The mean is
8.9 where our uniform 0..12 predicts 6, and nothing has landed in 0..3, which
that model makes a **3.6%** event over nine games. The doubt is now firm enough
to act on with a few more samples, and free to collect: every session starts
games anyway.

**Stars destroyed (-5)** -- the mechanism is now seen (a torpedo detonating a
star, taking the whole quadrant with it) but the scoring line has never been
observed non-zero.

### Untouched

Open item 7's falloff shape, open item 8's 1240..1500 band, open item 11
(boarding parties), the kill/day gate, and planets. The combat rig did not run.

## The combat rig: enemy motion and the fire law, both settled (2026-08-21)

Thirty-six turns against a single Commander in a cleared quadrant, its hit
points pinned to 600 so its output was a controlled variable, our own position
moved five times to sweep the geometry. Every turn a nil volley, which draws
the enemy's answer and provokes its movement; damage read from the pinned
pools and corrected for pods.

A first attempt used **shields toggling** as the null turn, on the theory that
it takes a turn without provoking movement. It does not take a turn at all:
every reading came back as exactly 50.0, which is `SHIELD_RAISE_COST`, not
enemy fire. SHUP/SHDN costs energy and passes no time.

### Enemy motion is deterministic, and the forces model is gone

| turns | moved |
|---|---|
| enemy NOT adjacent | **14 of 14** |
| enemy adjacent | 0 of 18 |

(Four further turns log as "adjacent and moved": those are the final approach
step, which lands on adjacency.)

**A Commander closes one sector toward the ship on every firing turn until it
is adjacent, and then holds.** No randomness. No retreat at any range. Never
more than one sector. Nothing about our state -- shields, energy, torpedoes --
enters into it.

That deletes `enemy_motion()` entirely: the forces score, the 150 divisor, the
-5 offset, the 1000 threshold, the skill clamp, and the `200*Rand()` jitter
restored earlier the same day on the strength of an apparent intermittency
that turned out to be the enemy simply being adjacent already. `enemies_here()`
went with it, and the 68000 port-check found that for me.

Holding when adjacent needs no code at all: the only closer cell is the ship's
own and `enemy_step()` refuses an occupied destination.

### The fire law: Euclidean, linear, zero at 12

    damage = hit_points * 0.78 * (1 - distance / 12)

k measured **0.782 with sd 0.038 over all thirty-six turns** across eleven
distinct ranges -- about 5% residual scatter, which is the random component and
is small.

The metric is Euclidean and provably not the alternatives:

| test | result |
|---|---|
| (4,2) vs (2,4), same Euclid | 314.7 vs 314.2 -- symmetric in dy/dx |
| (3,2) vs (2,3), same Euclid | 308.4 vs 311.2 -- same |
| same **Chebyshev** 4, Euclid 4.12..5.66 | 0.52, 0.52, 0.52, 0.41 -- varies, ruled out |
| same **Manhattan** 6, Euclid 4.24..5.10 | 0.54, 0.52, 0.52, 0.48 -- varies, ruled out |

Zero at 12 is exactly the constant our own lasers use, which is a satisfying
place to land: one falloff law for both sides.

**This supersedes the earlier four-point estimate** that put the coefficient
near 0.95 and claimed the curve "flattens into a floor". It does not flatten.
That reading was four shots at a single range, and this file said at the time
that three ranges could not settle the shape. Eleven can.

`ENEMY_FIRE_PCT` moves from 90 to 78.

## Play Again, and two things found beside it (2026-08-22)

Jamie asked for a play-again routine and I said the original had none, on the
strength of a grep for `again\|AGAIN` over `reference/strings.txt`. That
pattern cannot match `Again`. It is on line 18 of the file, four lines from
`Quit <Y/N>?`, and Jamie said so. The lesson is small and cheap: when the
answer is "the original does not do X", the grep that produced it is part of
the claim and should be shown.

### The prompt, measured

Played two games in the original end to end under dosbox-automation.

**It is a dialog, not a line of text.** A light-grey box with a magenta border
over the Hall of Fame, `Play Again?` in dark blue above two raised buttons
reading YES and NO in red. Both buttons are drawn the same -- there is no
visible focus ring on either, so nothing here says which one RETURN would take.

| | pixels (640x350) | cells (8x14) |
|---|---|---|
| box | x 150..280, y 70..130 | cols 19..35, rows 5..9 |
| YES button | x 170..205 | cols 21..25 |
| NO button | x 225..257 | cols 28..32 |

The box is not cell-aligned horizontally -- 150 is not a multiple of 8 -- which
is the original drawing in pixels because it can. On a character grid the port
rounds to the same cells and brackets the buttons instead of embossing them.

**YES returns to the TITLE screen**, not to a new game and not to the setup
screen: the ship animation plays again and every setup question is asked from
scratch, name, command level and self-destruct password. **NO exits to DOS**,
through a shareware farewell screen crediting Nels Anderson, and lands back at
`C:\>`.

### Q asks first, and our port does not

Typing `Q` in the original does not quit. It puts `Quit <Y/N>? _` in the
COMMAND panel and waits. Our port quits on the keystroke. NOT implemented --
noted here rather than fixed, because it is a change to a command rather than
part of the play-again routine.

### The incomplete-mission penalty is confirmed at -300

Quitting a fresh level-3 game printed `Penalty for incomplete mission...-300`
and a total of -300, with every other line zero. `SCORE_INCOMPLETE` in trek.h
was already -300, and the port's own Hall of Fame printed -300 for the same
quit in the same session. Two implementations agreeing on a number neither was
fitted to is the useful kind of corroboration.

## The music is note data, not audio (2026-08-22)

Jamie asked whether the soundtracks could be got out of the original or would
have to be recorded. They can be got out, and recording would be strictly
worse: the original is Turbo Pascal driving the PC speaker, so what is in the
binary is **exact frequencies in Hz and exact durations in timer ticks**. A
capture would produce square waves to pitch-detect back into the numbers that
are already sitting there.

`tools/extract_music.py` dumps all of it. Nothing it produces is committed --
see the copyright note at the end.

### Finding it

`tp_labels.csv` already had the runtime's `SOUND` at load-module offset
0x0227D6. Scanning the image for far calls to it gives **eight sites**, which
is few enough to read by hand: a 440Hz/250ms beep (twice), four procedural
sweeps, and one routine that walks a byte array. That routine is the player.

It is an **ISR** -- register-saving prologue, `mov ds, DGROUP`, `iret` -- on
the stock 18.2065Hz timer. Nothing in the game code writes PIT ports 40h or
43h, so that rate is never reprogrammed and one tick is 54.9ms.

    StartMusic(track: pointer; repeats: word; tempo: byte)

### The format

A track is a flat byte array of **(duration, frequency/10) pairs**, terminated
by a zero duration. Frequency 0 is a rest. Duration is in ticks, multiplied by
the tempo argument; both music tracks pass tempo 1.

Storing frequency as a byte times ten is the whole design, and it has a
consequence worth deciding about: **every pitch is quantised to 10Hz**, so the
notes are up to about 22 cents out. A4 lands on exactly 440, but D4 is 290Hz
(22 cents flat) and C5 is 520Hz (11 cents flat). That detuning is part of how
the game sounded. Reproducing it or correcting it is a port decision, not a
bug to fix silently.

### DGROUP, derived twice

Track pointers are DS-relative. The data segment base was brute-forced first
-- the only base in the entire file at which all five short tracks parse as
valid pairs with correct terminators, one candidate out of ~13,000 -- and then
confirmed independently by the ISR's own `mov ax, 0x28a2 / mov ds, ax`, since
0x28a2 * 16 + LOAD_BASE is the same address, 0x2D820. Two routes, one answer,
which is what turns a fit into a fact.

### The seven tracks

| DS offset | what | notes | length | repeats |
|---|---|---|---|---|
| 0x057E | **title music** | 205 | 45.5s | 99 |
| 0x071A | **end-of-game music** | 319 | 43.5s | 99 |
| 0x099A | effect | 2 | 0.1s | variable |
| 0x09A0 | effect | 6 | 0.3s | 1 |
| 0x09AE | effect | 2 | 0.8s | 1 |
| 0x09B4 | effect | 2 | 0.8s | 1 |
| 0x09BA | effect | 3 | 0.9s | 1 |

Which of the two long tracks was which was **read out of the running game**,
not inferred: dosbox-automation's memory API, the player's current-track
variable at DGROUP+0x1cb6, sampled on the title screen (0x057E, flag set, 99
repeats left) and again at the end of a game (0x071A). Both are real tunes --
the title opens on a two-second rest then D4 D4 D4 D4 F4 in D major, the
end-of-game on a descending F#5 E5 D5 figure.

This confirms what Jamie described: **a track on the opening screen, effects
during play, a different track at the end**. The in-game effects are the five
short tracks plus code that does not use the player at all.

### The effects that are not tracks

Four `Sound`/`Delay` sites at 0x00747B..0x0074E9 are **procedural sweeps**: a
loop running a frequency from 37Hz to 1000Hz playing f, 2f and 3f for 2ms
each, and a second from 1200Hz upward at 1ms. Around six seconds of rising
three-harmonic noise -- an explosion or self destruct, not a tune.

### What SND toggles

Every sound site tests `[0x1cc8]` first and skips if it is zero, and the
player's ISR clears the playing flag when it finds it zero. That byte is the
sound on/off switch, which is the mechanic behind the `SND` command still open
on the command list.

### Copyright

The note data is Anderson's creative work in the same way the message prose
is, so `tools/extract_music.py` writes into `build/` and **nothing extracted is
committed**. The repo keeps the method, not the material -- the same rule
`reference/` is gitignored under.

## The title track stops at the briefing question (2026-08-22)

Jamie said the opening track plays on the opening screen only and stops as soon
as the briefing question appears. Measured, and he is right.

The original's player keeps a "playing" flag at DGROUP+0x1cc9 and the current
track pointer at +0x1cb6. Read live through dosbox-automation:

| screen | track | playing |
|---|---|---|
| title | 0x057E | **1** |
| "Will you require a briefing (Y/N)?" | 0x057E | **0** |

The pointer still holds 0x057E because nothing clears it -- only the flag goes
down. So the track is stopped, not finished: 0x057E is 45.5 seconds long and
the flag was already 0 within a couple of seconds of leaving the title.

The port had this wrong and said so at the time: it let the track run through
the setup screen, recorded as an explicit guess rather than a measurement. It
now stops between `ui_title()` and `ui_setup()`.

**This is the second time this session that a claim about the original was
settled by Jamie rather than by my own search**, after the "Play Again?" prompt
I said did not exist. Both were cheap to check with instruments already
running. The rule is in the port plan memory now: when the question is what the
original does, measure it or ask -- do not infer it and write it down as if it
were known.

## All five sound effects identified (2026-08-22)

The five short tracks were extracted days before anything was known about what
they were for. Naming them by ear, or by the shape of their frequencies, would
have been invention. The routines name themselves.

### Method: resolve the strings each calling routine prints

Turbo Pascal pushes string constants CS-relative -- `mov di, imm; push cs; push
di` -- so resolving one needs the code segment of the routine doing the
pushing, which is not in the file anywhere. But a far CALL to that routine
carries it: `9A off seg`. So for each effect, find the enclosing procedure by
scanning back for `push bp; mov bp, sp`, find a far call whose target is that
procedure to learn its segment, and then every CS-relative string in its body
resolves.

Five procedures, five unambiguous answers:

| track | what its own routine prints | so it is |
|---|---|---|
| 0x099A | "Amount to fire at ", " unit hit on ", "Lasers overheat" | **lasers fire** |
| 0x09A0 | "ENERGY TORPEDO CONTROL", "Number to fire: ", "Tracking #" | **torpedo launch** |
| 0x09AE | "Status", "Green", "Yellow", "Alert", ">>ALERT<<" | **status goes to ALERT** |
| 0x09B4 | "Vandal Death Pod enters quadrant: " | **the death pod arrives** |
| 0x09BA | "WARNING:  Mongol at ", "StarBase shields protect Lexington" | **incoming Mongol fire** |

Jamie had already said the alert was "when you jump into a quadrant with
Mongols", and a dynamic run had caught 0x09AE firing on exactly one warp out of
seven. The strings say why: it belongs to the routine that draws the status
panel, and the panel goes to ALERT on arriving somewhere with Mongols in it.

### Most in-game noise is not the player at all

Worth recording, because it explains a run that looked like a failure. A full
combat exchange -- alert, lasers, hits, damage, casualties -- moved the track
pointer NOT ONCE. The player handles these five effects and nothing else. The
rest of the game's noise comes from code that calls the runtime's Sound()
directly: two 440Hz/250ms beeps, and four procedural sweeps running a frequency
from 37Hz to 1000Hz playing f, 2f and 3f for 2ms each.

### The rig's two traps, each of which cost a run

**The track pointer is never cleared.** Only the playing flag drops when a
track ends, so an effect replaying is indistinguishable from silence. Ten
actions reported "silent" against a game that was visibly firing lasers. The
fix is to poke the pointer to a sentinel before each action, which is safe only
while the flag is down -- the ISR returns before dereferencing it then.

**A rig that drives the setup screen must start from a fresh emulator.** Run it
against an emulator that already has a game going and it types the setup
answers into the game. That produced a whole table of nonsense and a baseline
reading that sent me looking for an effect at the wrong moment.

## The death ray's sound, measured but not ported (2026-08-22)

The four Sound/Delay sites at 0x00747B..0x0074E9 belong to the death ray. The
procedure that contains them prints "Captain, I wish to remind you that the
death ray is experimental in nature and has been highly prone to failures",
"wish to continue <Y/N>? ", "Preparing death ray..." and "Firing!".

Two sweeps, both procedural rather than note data:

| sweep | range | step | per step | length |
|---|---|---|---|---|
| firing | 37Hz to 1000Hz, playing **f, 2f and 3f** | 1Hz | 2ms each of the three | ~5.8s |
| second | 1200Hz to 3000Hz | 1Hz | 1ms | ~1.8s |

Both are gated on the sound flag at DGROUP+0x1cc8 and end with NoSound.

Not ported: `RAY` is not implemented, and a sound for a mechanic that does not
exist is dead code. This table is the specification for when it is.

## The refusal beep (2026-08-22)

`Sound(440); Delay(250); NoSound`, reached from **seven call sites**, all of
them refusals -- two inside the laser dialog and five inside the torpedo
dialog, alongside "Captain, we have no torpedos!", "Captain, all tubes are
damaged!" and "Captain, we have insufficient energy!". A second copy of the
same beep sits in the command parser, guarding a field that is too long.

So it is not a generic bleep: it is the sound of the ship declining an order.

## Auditing the port against EGA Trek rather than the ancestor (2026-08-22)

Jamie: "lets make sure the commands we do match egatrek. if its only in the
ancestor we should reconcile."

### The method

Extract every user-visible string literal from the port and ask two questions
of each: does it appear in EGA Trek's own string table, and does it appear in
the ancestor's source? The interesting set is **in the ancestor, not in EGA
Trek**, and it is small enough to read.

A first pass that only checked "does the ancestor contain these words" was
useless -- it flagged 52 of 114 strings, because both games are about captains,
shields, energy and torpedoes. The two-sided test cut it to ten, of which four
were real.

### What it found

**Self destruct was speaking the wrong game entirely.** The port printed
"PASSWORD-REJECTED; CONTINUITY-EFFECTED", "PASSWORD-ACCEPTED" and "ENTROPY
MAXIMIZED. n TAKEN WITH US." -- all of them the ancestor's. The comment
defending it read "the ancestor's own wording, which is too good to replace",
which is the wrong test for a port of EGA Trek.

EGA Trek has its own sequence, and it was sitting in the extracted strings the
whole time:

    "Enter self-destruct password: "        "Hit ESC to abort"
    ">>SELF-DESTRUCT SEQUENCE COMMENCING<<"
    ">>DESTRUCT ABORTED<<"
    ">>WRONG PASSWORD, DESTRUCT ABORTED<<"

**The abort path came with it.** EGA Trek offers ESC and this port had no way
out of the prompt at all, which is a different command, not a different
wording. ESC is row 7 column 7 in the matrix and was simply missing.

**The STATUS panel said ENEMIES.** EGA Trek says "Mongols:" -- on the panel, in
the ship classes, and in the score sheet. ENEMIES is the ancestor's word and
nobody else's. Now asserted by a test, because it is the kind of thing that
drifts back.

**"TORPEDOES" with two Es** in one docking message, where EGA Trek spells it
"torpedos" throughout and the rest of this port already did.

### What survived the audit

The three-pool energy transfer is EGA Trek's, CONFIRMED in DOSBox-X. The
scheduled-event mechanic is EGA Trek's, its deadline messages seen in captures;
only the timing model is the ancestor's. Enemy motion, the fire law, the docked
repair rate and hit points were all ancestor-derived once and are all measured
now.

### The one thing still taken from the wrong game

`SELFDESTRUCT_FACTOR 25` -- the blast model is the ancestor's `kaboom()`:
everything whose power times distance is within 25 times remaining energy dies.
EGA Trek's manual says only "With any luck, you will at least take a few
Mongols", so the mechanic is right and the threshold is unmeasured.

**The experiment:** destruct with known energy against enemies at known
distances and read the kill count off the evaluation screen, which counts
Mongols killed. One data point per game, so it needs several -- vary energy
across runs and bracket the threshold.

## One session, nine captures, and a rescue (2026-08-23)

The plan was four cheap command captures plus an enemy-count sample. An
evacuation deadline appeared mid-session and was worth abandoning the plan for.

### The four commands that were blocked on "no mechanic behind it"

**`MSGS`** opens a **scrollable overlay**, not a panel takeover: a grey box
titled `PREVIOUS MESSAGES`, entries carrying their stardate, and a footer
reading `t and t to scroll, ESC to exit` (up/down arrows). So the port needs a
deeper log AND a scrolling viewer, which is what the command table guessed, but
the presentation is now measured rather than assumed.

**`F)ix`** opens `ENGINEERING` and asks:

    System to concentrate repairs on:
    0 to abort, L for list: _

**It is not a priority list.** It is a single "concentrate on this one" pick by
number, with `L` listing the numbering and `0` aborting. That is a far smaller
core change than "needs a repair-priority model the core lacks" implied.

**`HAIL`** costs a turn -- the stardate moved 3500.1 to 3500.2 -- and opens a
COMMUNICATIONS box that was **empty**, presumably because no StarBase was in
range or known. The mechanic exists; what it says when it has something to say
is still uncaptured.

**`C)hart`** produced no visible change at all on a console already showing the
chart. Consistent with the manual's "displayed at all times unless overridden",
so its job is probably to restore the panel after something overrides it. Not
settled, but the cheap half is done: on a normal console it is a no-op.

### ~~New mechanic: warp speed can break the engines~~ IT IS DISTANCE, NOT SPEED

Setting warp 8 and moving printed:

    Warp engines damaged by excessive speed.

No time passed and the ship did not move. Warp 4 then worked. So high warp
carries a damage risk that nothing in trek.h models, and the ship's own Warp
Engines entry showed damage afterwards.

> **READ AND CORRECTED 2026-08-28.** The message says "excessive speed" and
> the observation was taken at warp 8, and both are misleading: **warp appears
> nowhere in either expression.** See "The warp engines break on DISTANCE"
> below.

### Planets, orbit and landing, all in one run

`O)rbit` **requires adjacency, exactly like docking** -- from three sectors away
it answered `NAVIGATION: Not adjacent to planet.` Moved to the neighbouring
sector and it worked:

    NAVIGATION: Entering standard orbit. Planet Androneda-4, Type N.
    SCIENCE: Scanners show a settlement on the planet.

The MAIN VIEWER switches to an **orbit display** -- an ellipse with the planet,
labelled `STANDARD ORBIT 301`, showing `APOGEE: 92.7` and `PERIGEE: 87.3`.

`LAND` opens `LANDING PARTY`:

    How do you wish to get to the planet?
    1) Shuttle Craft
    2) Transporter
    3) Abort landing

**That is why Transporter and Shuttlecraft are two of the twelve repair
entries** -- they are the two ways down, and damage to one presumably forces
the other.

### The rescue, which NOTES called unreachable

Choosing the transporter ran the whole sequence:

    Landing party to transporter...
    Landing party is on planet...
    Planet settlers found...
    Evacuating settlers...
    Landing party beaming up.

And the score sheet confirmed it:

    1     Rescues @ 200 each..............200
    0     Penalty for incomplete mission...-300
    TOTAL............................-100

**`SCORE_RESCUE` 200 is CONFIRMED**, not derived. NOTES item 14 listed rescues
as "worth +200 each in the scoring rubric and currently unreachable"; they are
reachable, and this is the whole path: a COMMUNICATIONS deadline message, warp
there before it expires, move adjacent, `O`, `LAND`, pick a route.

### CORRECTION: a torpedo aimed AT a star is absorbed

This file has a section titled "Torpedoes detonate stars, and a supernova is
enormous". The title is too broad. Firing one torpedo directly at a star at
5-3 gave:

    Tracking #1
    Torpedo absorbed by star.

and the star was still on the scan afterwards. Re-reading the original entry,
it describes firing **at an enemy** with a star **in the path** -- "the ray
passed through a star at 4-4". So the two observations agree once stated
precisely:

* **target a star** -> absorbed, nothing happens, star survives
* **star in the flight path** -> supernova, quadrant destroyed, ship thrown

Which means the `Stars destroyed @ -5` scoring line comes from the transit
case, and a port that only implements "aim at star" will never score it.

Also captured in passing: the torpedo dialog asks `Number to fire:` FIRST, then
`Sector to fire #1 at:` per torpedo -- our port asks only for a sector.

### Enemy counts

Level 3 gave **38**, the tenth sample. Level 2 gave **30** -- the **first
non-level-3 sample this project has**, which is the reading the count formula
actually needs. Series so far: level 3 = 40, 42, 42, 42, 34, 38, 37, 37, 38,
38; level 2 = 30.

## The font is two fonts (2026-08-23)

Item 6 assumed EGA Trek's panels wanted CP437 glyphs, and the last revision of
that item argued the opposite -- that a 640x350 graphics-mode game has no
character set to copy at all. **Both were half right**, which only came out by
dumping the fonts and comparing them glyph by glyph.

    digits, punctuation, space   ->  the BIOS ROM CP437 8x8 font
    letters A-Z and a-z          ->  a 464-byte table inside EGATREK.EXE

`tools/dump_font.py` extracts both and assembles the font the player actually
sees. Output goes to `build/` and is never committed, same rule as
`tools/gen_music.py`.

### How each was found, and why neither offset was assumed

Both bases are unaligned and both would have been got wrong by arithmetic, so
each is located by pattern-matching a glyph whose bitmap is known:

- **ROM font at 0xC01F5.** The video BIOS reads through `/memory` like any
  other address -- 32K at 0xC0000 with a valid 55AA signature. The probe is
  glyph 1, the CP437 smiley, whose 8x8 form cannot collide with the 8x14 set's
  version of the same glyph (fourteen bytes, blank rows top and bottom). Three
  assertions then confirm the base: glyph 0 blank, 0xDB solid, 0xB1 the 55/AA
  dither. The 8x14 table is also present, at 0x1924.
- **Letter table at 0x1B149 in `EGATREK_unpacked.exe`.** The probe is `'A'`
  as read off a 640x350 capture -- a measurement, not a font lookup. Every
  letter of "Welcome aboard Captain!" then resolved to a single hit at a
  consistent base.

**The table starts AT `'A'` and ends at `'z'`** -- 58 glyphs, 464 bytes. Below
0x41 and above 0x7A is x86 code, and reading it as glyphs produces convincing
nonsense: index 0x20, where a blank space belongs, holds `6E 00 5F 5E 06 E8 95
FB`. That is the trap this file exists to record. A dump that assumed a
256-glyph table would have shipped instructions as bitmaps and only found out
on screen.

### The audit that settled it

Every glyph on the briefing prompt and the setup screen was compared against
both sources:

| characters | ROM | EXE |
|---|---|---|
| `! / < > ? : ( ) , -` and space | match | absent |
| digits `1 2 3 5` | match | absent |
| `C Y a b c d e f g i l m n o p q r t u y` | differ | match |
| `I N W _ ` + backtick + ` x` | match | match (identical in both) |

The punctuation and digits are **not present anywhere** in the executable or in
640K of conventional memory -- searched as 8-byte runs, zero hits -- while they
are on screen at that moment. That is what makes the split a measurement rather
than an inference.

Six of the 58 letter slots are byte-identical in both fonts, so they prove
nothing either way; the other 52 are decisively the executable's.

### What the letters actually differ by

One pixel of width, consistently. The executable's face is rounder and fills
more of the cell:

        'e'  EXE            ROM
             .#####..       .####...
             ##...##.       ##..##..
             #######.       ######..
             ##......       ##......
             .######.       .####...

### Consequences for item 6

- The port needs **both halves**. Taking only the ROM font gives the right
  digits, punctuation and box-drawing with visibly wrong letters.
- 464 bytes of letters plus whichever CP437 glyphs the console actually uses is
  well inside what the C128 binary can carry, so item 6 stays un-gated by disk.
- **UNVERIFIED:** whether a real EGA card's ROM 8x8 font is byte-identical to
  the S3 Trio64 BIOS DOSBox emulates here (`machine = svga_s3`). The CP437 8x8
  set is meant to be common to both, but that has not been tested on this
  machine, and the game pulls these glyphs from the BIOS at run time -- so on
  real hardware the non-letter half was whatever the player's video card
  carried.
- **UNVERIFIED:** whose face the letter table is. It is linked into the
  executable, which is all that has been established. Turbo Pascal's Graph unit
  is the obvious suspect and has not been checked.

## MSGS and A#, captured whole (2026-08-23)

Both were listed as blocked on "no mechanic behind it". One session settled
both, and `A#` turned out to be a real mechanic that would have been invented
wrongly: the port's message panel is not the archive, and acknowledging is not
deleting. Captures in `reference/shots/msgs/`.

### A#: the panel is a queue, the log is a record

| | |
|---|---|
| `A1` | removes the **first** message box from the panel; the rest move up |
| bare `A` | clears **every** message from the panel |
| `A5` with an empty panel | silent no-op, no error, nothing drawn |
| any of them | **costs no turn** -- the stardate did not move |
| after `A1` | the acknowledged message is **still in MSGS** |

So messages are numbered **1-based by position in the panel, top to bottom**,
and acknowledging dismisses from the panel only.

**The numbers are not displayed anywhere.** No capture this project has taken
shows a digit on a message box, and this session looked for one deliberately.
The player counts boxes down from the top. That is worth knowing before
building it, because the obvious port -- print a number in each box border --
would be an addition to the original, not a port of it.

### MSGS: an archive, and it is not the panel

- **Costs no turn.** 3500.0 before, 3500.0 after.
- **Opens on an empty log**, drawing the box, the title and the footer with
  nothing between them. It does not refuse.
- **Opens scrolled to the BOTTOM**, newest visible. The topmost entry is
  routinely cut off mid-way, which is how it announces there is more above.
- **Scrolls one LINE per keypress**, not one entry, clamped at both ends.
  Verified by pressing up once and watching the window move exactly one line.
- Acknowledged messages remain. The log is not a view of the panel.

### The overlay, measured off a 640x350 frame

Outer box **x 200..520, y 160..302** -- 321x143 pixels, which is columns 25..65
and about rows 11.4..21.6. Fill is EGA dark grey (8); the border is a cyan
rectangle inset three pixels.

    y 162   PREVIOUS MESSAGES              cyan (3), centred
    y 177   StarDate: 3520.7               MAGENTA (5), x=210
    y 187     COMMUNICATIONS: ...          GREEN (2), x=218
    y 197     ...wrapped, up to 3 lines
    ...      eleven content lines in all
    y 288   ^ and v to scroll, ESC to exit RED (4), centred

**Content lines are on a 10-pixel pitch**, starting at y=177 -- neither the
14-pixel character row nor the 8-pixel font height. A graphics-mode game can
put text wherever it likes, and this is the third panel where it does (the
short range scan and SYSTEMS STATUS were the first two). Eleven lines of 10px
occupy what a character display would need eleven rows for, so on the VDC the
box has to be **taller in rows than the original is**, exactly as the scan and
chart panels already are.

**Entry shape:** one `StarDate: N.N` line, then the message indented one
character and wrapped to at most three lines. Variable height, 2 to 4 lines.

**Department colour does NOT carry into the overlay.** On the panel,
COMMUNICATIONS is yellow and DAMAGE REPORT orange. In the overlay both are
green, with only the `StarDate:` line coloured differently. Measured on an
entry of each kind in the same capture, so this is not a sampling artefact.

### A discrepancy left as observed, not explained

The panel stamps its boxes with one stardate and the log shows another for the
same message:

| message | panel border | MSGS |
|---|---|---|
| Ceti Alpha-8 evacuation | 3520.1 | 3520.7 |
| Life Support failing | 3520.2 | 3520.7 |
| Lasers failing (54%) | 3542.1 | 3542.7 |

The log's value looks like the stardate at the END of the turn and the panel's
like the moment inside the move when the event fired. Both readings ended in
.7, which is either the pattern or a coincidence of two moves of similar
length -- two samples cannot tell those apart, so nothing is concluded here.
The port stamps at message time, which matches the panel.

## One session, three commands: SAVE, boss mode and RAY (2026-08-24)

Run before implementing any of them, on the principle that the cost driver on
this list is what is still unmeasured rather than what needs writing. All three
came back with something that would have been got wrong by inference.

### SAVE, and restore

`SAVE` opens a **`SAVE GAME`** box asking:

    File Name: _
    <Enter> for default

The default is **`EGATREK.SAV`**. It **costs no turn** -- the stardate did not
move. The restore side is on the setup screen, after `Restore a saved game
<Y/N>? Y`:

    File name, <Enter> for default, <ESC> to abort:

Restoring works and comes back to the exact saved state -- same quadrant,
sector, stardate and Mongol count.

**The file is PLAIN TEXT**, 1,896 bytes, and its shape is worth knowing even
though this port's own format is binary and deliberately not compatible:

    EGATrek 3.0        <- version banner, first line
    CAPTURE            <- player name
    2
     2.0000000000E+00  <- Turbo Pascal reals, written with Write()
     3.5000000000E+03  <- stardate 3500
     ...

The version banner on line one is the same idea as `TREK_SAVE_VERSION` in
`core/serial.h`, arrived at independently. Anderson refused an unknown version
the same way we do.

### Boss mode: it SHELLS OUT, and that is not what "screen blanker" implied

The command table here has described `Shift-F1` as a screen blanker since the
reference card was first read. It is not. **It runs `COMMAND.COM` and you type
`EXIT` to come back**, with the game state completely intact.

The evidence, in the order it arrived:

- Pressing it drops to a **real, live DOS prompt** -- typing `VER` echoes.
- **`COMSPEC` is in the string table** as a standalone entry, which is exactly
  what Turbo Pascal's `GetEnv('COMSPEC')` compiles to.
- There is **no fake-prompt text anywhere in the binary**, so nothing is being
  drawn to imitate DOS.
- Typing `EXIT` returns straight to the console, stardate unchanged.

**Why it looked broken first.** Under automation it produced only "a flash":
`Exec` fails immediately when `COMSPEC` is not set, which is the failure
looking exactly like a screen blank. Injected Shift-F1 also never triggered it
at all -- Jamie pressing the key on the real window is what produced it, and
Jamie asking "are there special commands in boss mode?" is what sent the search
to the string table where `COMSPEC` was sitting.

**For the port:** a shell-out has no C128 equivalent worth imitating. Blank the
VDC and wait for a key, and record here that the original does something else.

### RAY: four outcomes, and one of them destroys your ship

Refused with no enemies present, costing no turn:

    SCIENCE: Scanners show no enemy ships in this quadrant.

With enemies, `WEAPONS CONTROL` asks first, in the original's own words:

    Captain, I wish to remind you that the death ray is experimental in
    nature and has been highly prone to failures. Are you sure that you
    wish to continue <Y/N>?

Answering Y prints `Preparing death ray...` then `Firing!`, then one of **four
outcomes**, which the string table holds in this order:

    It worked!
    Death ray misfires.
    Nothing happens...Reports coming in from all decks...half the crew
      has turned into some kind of mutants!
    The apparatus is going unstable!

**The fourth destroys the ship.** Observed live: Jamie watched
`Preparing / Firing! / The apparatus is going unstable!` and then the ship
self-destructed. The screenshots at four-second intervals missed the unstable
message entirely and caught only the aftermath -- a reminder that a polled
capture is not a recording.

**That aftermath is a screen this port did not have until 2026-08-29**, and
it is not the ordinary Detailed Evaluation. It is BUILT now -- `ui_loss_memo`,
off `ship.lost_how`, recorded at all seven death sites:

    Dept. of Space / EARTH HEADQUARTERS / Top Secret
    From: Commander, Earth Sector
    To:   Headquarters
    Date: 3526.6
    Re:   Loss of U.S.S. Lexington, RCB-92

    U.S.S. Lexington destroyed by death ray explosion this stardate, with
    loss of all aboard.  Results of operations prior to loss follow:

     Stardays in action: 26.6
     Mongol ships destroyed per stardate: 0.04
     Score: -701

ONE SAMPLE. ~~Which of the four outcomes is how likely is not measured~~ --
**READ 2026-08-28 and swept here 2026-08-29: `Random(6)` with every roll
mapped, 1/6 each for WORKED / MISFIRE / MUTANTS / MISFIRE_HOLES and 2/6 FATAL.
Also there are FIVE outcomes, not four -- roll 1 and roll 3 print the same
line and do different things, which is what the 2026-08-28 read separated.**
The ancestor's failure table was a hypothesis and is not needed.

### Bonus: shields are the ARROW KEYS

`F1` opens `COMMANDS AVAILABLE`, the in-game command list, and it says shields
are the up and down arrows. `EGATREK.REF` agrees: `SHUP Shields Up (use up
arrow)`. This port implemented `SHUP`/`SHDN` as word commands only. It now has
arrow keys in the matrix (added for `MSGS`), so binding them is a small change
and the original's own primary binding.

Note also that **boss mode is NOT in the F1 help** -- only on the reference
card. The in-game list is not the authority.

### Bonus: four M)ove refusals, none of them previously recorded

    But captain, that quadrant contains a supernova!
    Captain, that is our current location!
    But captain, the navigation computer shows no such location.

and `NAVIGATION` has **two input modes**: `Quad, Sector:` taking `8,6,4,4`,
and `DeltaX:` / `DeltaY:` taking two relative numbers one at a time.

**CORRECTION 2026-08-24: "what toggles between them" was written here as not
established, and it was documented in two places I already had.** The manual
(l.515-529) explains it, and NOTES.md has carried "Damaged computer changes how
M works" since an earlier session, citing that same passage. Straight into the
trap [[negative-claims-about-egatrek]] exists to prevent: I reported an unknown
without checking the sources on the shelf.

What the manual actually says:

- **Automated** is the default: absolute `Quad, Sector`, **vertical first**.
  Giving only a sector is an impulse move inside the current quadrant.
- **Manual** asks `DeltaX` (vertical) then `DeltaY` (horizontal), relative and
  signed. **The digit before the decimal point is QUADRANTS and the digit after
  it is SECTORS**, each 0-7. Moving from 1,8,1,8 to 2,6,1,6 is DeltaX `1.0`,
  DeltaY `-2.2`.
- It switches **automatically when the navigation computer is damaged** -- then
  it is the only way to move -- and **voluntarily by typing just `M`** at the
  coordinate prompt.

That last one explains the session above completely: the mode changed under me
because my desynced scripted keys put a stray `M` into the coordinate prompt.

## The manual answers a dozen open questions, and grep could not read it (2026-08-24)

`reference/manual.txt` is **ISO-8859 with high-bit box-drawing characters**, and
this project's `grep` silently returns nothing on it. Every search of the manual
this session and before came back empty, which read as "the manual does not say"
when it meant "the tool did not look". Converting it first --

    python3 -c "import re;print(re.sub(r'[^\x20-\x7e]',' ',
      open('reference/manual.txt',encoding='latin1').read()))"

-- turns 38K of documentation from invisible into searchable, and it answers
questions this project has been carrying as unmeasured for weeks.

### Open items the manual CLOSES

| item | the manual's answer |
|---|---|
| **Warp-speed engine damage** | "maximum warp speed is approximately **warp 1 plus 0.09 times percentage of repair**" |
| **`REPAIR_FOCUS_FACTOR`** (DERIVED 2) | **3x**. The full table: 1x normal, **2.5x docked**, **3x focused**, **5x focused and docked** |
| **What toggles NAVIGATION's two modes** | automatic mode needs the computer at **100%**; below that manual entry is the only way to move. Typing `M` at the coordinate prompt switches voluntarily |
| **`USE` / energium gate** | "regulations prohibit the use of raw energium except in extreme emergencies; your **shields must be under 50% and main energy under 20%**" |
| **Hall of fame depth** | "if you get one of the **top two scores for your command level**" -- independent confirmation of the place-major layout measured from a crafted TREK.SCR |
| **Boss mode** | "Hit Shift-F1 and you **shell to MS-DOS** ... when you're ready to return, type `EXIT`" -- stated plainly, and derived the hard way instead |
| **Supernova chart marker** | "quadrants with supernovas cause the scanners to overload and **display all 9's**" -- the `999` seen in a capture |
| **Base types** | chart digit **1 = StarBase, 2 = research station, 3 = supply depot**. StarBase replenishes everything; supply gives life support and torpedoes; research gives life support only |

### Open items the manual does NOT answer

Worth recording so they are not searched for again: **boarding parties** (the
word does not appear), **tractor beams**, the **laser heat band's numbers**
("watch the gauge to prevent overheating", no figures), the **kill/day scoring
gate**, and the **enemy-count-per-level formula** ("higher levels must contend
with more enemy ships"). Those stay measurement work.

### ~~The big one: eleven of twelve systems take damage that does NOTHING~~ BUILT (swept 2026-08-29)

**ALL TWELVE ROWS ARE BUILT, and one rule out of the table is not.** Every
accessor below exists in `core/trek.c` and every one has a live caller --
verified by walking the call sites, not by reading this heading:

    trek_max_warp        trek.c:1092   <- trek.c:1139 (the warp gate)
    trek_impulse_ok      trek.c:1100   <- trek.c:1373
    trek_tubes_available trek.c:1104   <- main.c:1561
    trek_srscan_level    trek.c:1112   <- ui.c:125
    trek_lrscan_level    trek.c:1119   <- ui.c:179
    trek_autonav_ok      trek.c:1126   <- main.c:817, 887, 918
    trek_transporter_ok  trek.c:1127   <- planet.c:181, main.c:418, 455
    trek_shuttle_ok      trek.c:1128   <- planet.c:179, main.c:413, 447
    trek_laser_eff       trek.c:1130   <- trek.c:1724, ui.c:572-573

plus the converter at `trek.c:477`, the shield efficiency as the second
`scale_pct` at `trek.c:1889-1890`, `LIFE_RESERVE_MAX_TENTHS` = 20 for the
manual's two days, and the shuttle's 0.2-stardate round trip at
`planet.c:197`.

**THE ONE ROW STILL OPEN IS HALF OF THE COMPUTER'S.** Auto-nav at 100% is
built; **chart entries being lost and needing a re-scan is not** -- there is
no `chart_erase` anywhere. That rule is already on the build list under its
own name (chart erasure, ~60 bytes together with reinforcements), so the
table below costs **nothing additional**.

**This heading survived being wrong for a while, and it is worth saying why.**
It was the project's standing answer to "what is the largest thing left", so
it got QUOTED rather than re-checked -- into NOTES.md, into the memory budget,
and into a verdict about whether the port still fits. A claim that is load
bearing is the one most worth re-deriving. See the note in NOTES.md.

The original text follows, as the specification it still is:

| system | documented effect |
|---|---|
| Warp Engines | max warp = 1 + 0.09 x pct |
| Impulse Engines | below **50%** they simply stop |
| Lasers | pct is directly the fraction of energy converted to damage -- 100% does twice the damage of 50% |
| EnTorp Tubes | **100% = three tubes, 67-99% = two, 34-66% = one** |
| Short Range Scanners | above 90% full; **below 90% cannot see anything smaller than a star**; below 50% dead |
| Long Range Scanners | **below 100% cannot detect enemy ships**; below 50% dead |
| Computer | chart entries can be lost and need re-scanning (**READ 2026-08-29, see below: it is in DamageReport, thresholds 70 and 30**); **automatic navigation needs 100%** |
| Life Support | must be 100% to make food and oxygen; without it the ship lasts **two days** on reserves |
| Transporter | must be **100%** to use |
| Shuttlecraft | must be **100%** to use, and takes **0.2 stardays** round trip |
| Shields | pct is how efficiently shield energy becomes actual shielding |

~~This is the largest single body of specified-but-unbuilt work in the
project.~~ **ALL TWELVE ROWS ARE BUILT** -- see the sweep above the table, and
chart erosion, the last of them, on 2026-08-29. None of it needed an emulator,
which was the one part of this sentence that held.

### CHART EROSION IS REAL, AND IT IS IN THE DAMAGE REPORT (READ 2026-08-29)

**A RETRACTION FIRST.** Earlier the same day this section said the binary
"appears not to do this", on the strength of classifying all 24 literal
references to the recorded chart at DS:0x2360. That was wrong, and the way it
was wrong is worth more than the fact: **site 0x021396 was in the write list
and was labelled "the L.R. SCAN loop" from its NEIGHBOURHOOD rather than by
disassembling it.** It is not the scan. It is the erasure, and the two
instructions before it are `xor ax, ax`. A search that finds the right address
and then guesses what it does is not a search.

**THE ROUTINE IS THE DAMAGE REPORT**, around 0x0212DE. `[0x1CF4]` is the
system index it is reporting on -- 0x0212EC loads it and 0x0212F0 turns it
into `0x1188 + index*16`, the sixteen-byte system NAME table. So this is
`DamageReport(sys)`, the same one the wear-and-tear rule calls.

    DamageReport(sys):
        ... print the report for sys ...
        if (sys != 9)  return                    ; 0x02134D, COMPUTER only
        for row 1..8, for col 1..8:              ; 0x021354, 0x02135B..0x0213A5
            if (computer <  30)                       erase   ; 0x02136A jl
            else if (Random(10) < 5 && computer < 70) erase   ; 0x02137A/0x02137F
            erase:  chart[row*16 + col*2] = 0             ; 0x021394, 0x021396

where `computer` is `[0x236C]`, which is sys[] index 9 -- the array is twelve
words at DS:0x235A and 0x235A + 9*2 = 0x236C.

**SO THE MANUAL'S "SUFFICIENTLY DAMAGED" IS TWO THRESHOLDS, NOT ONE:**

    computer >= 70    the chart is untouched
    computer 30..69   EVERY quadrant is forgotten on a coin flip, one roll each
    computer <  30    the whole chart is wiped

and it happens **on the damage report**, not per turn -- so it fires when the
computer is hit, once, across all 64 quadrants. A ship at 45% loses about half
its chart every time the computer takes another hit.

**This is the last row of the manual's per-system effect table**, and it is
now read rather than documented. The re-scan half needs nothing: the L.R.
SCAN already rewrites what it sees.

### Smaller rules, also documented and worth checking against the port

- Shields at 100% energy: **no enemy fire penetrates**, energy only drains from
  the shields. Once fire penetrates, main energy is lost and systems damage.
- Raising shields costs a little main energy; **lowering costs nothing**.
- Torpedo **accuracy is thrown off when fired with shields raised**.
- Torpedoes replenish at a StarBase **or supply station**.
- Docked at a StarBase, **its shields protect you** from enemy lasers.
- Warp travel with shields raised costs **double** -- already implemented
  (`core/trek.c`, `if (ship.shields_up) cost *= 2`).
- Move accepts `6235` as well as `6,2,3,5`, and `m6235` on the command line;
  `WARP` accepts `w5.2` inline.
- Enemy colours: battleship light blue, command red, scout purple, **supply
  green**.
- Directions are measured with **0 degrees directly to the right**.

## What the string table already answers (2026-08-24)

Asked what else is worth capturing from DOSBox. Checking `reference/strings.txt`
first turned out to answer more than a capture session would, and one of the
answers had been carried as "no lead at all" for weeks.

### Boarding parties: SPECIFIED, and they were never a mystery

The open item said there was no lead. The strings describe the whole mechanic:

    SECURITY: A Mongol boarding party has transported into
        Engineering.  |  Laser control.  |  EnTorp control.

and the consequence of each:

    Mongol boarding party controls the lasers.
    EnTorp control is held by the Mongol boarding party.
    Cannot raise shields; Mongol boarding party controls engineering.

and how it ends:

    SECURITY: The Mongol boarding party has been eliminated.

So a boarding party **seizes one of three stations and blocks the commands that
use it** until security clears them. Related, and separate:

    SECURITY: A spy has been captured aboard the Lexington, but he has
    damaged the <system>

**What is still unmeasured is only the numbers** -- how often they arrive, how
long they hold, whether the station is chosen evenly. The mechanic itself needs
no capture.

### Eight ways to lose, of which this port implements three

    lost in battle with Mongols
    crew lost due to failure of life support systems; the ship has been salvaged
    destroyed by Vandal death pod
    destroyed per order of captain            (self destruct)
    destroyed by explosion of star
    pulled into black hole & destroyed
    destroyed by death ray explosion          (RAY's fourth outcome)
    the cowardly captain ... surrendered his ship, with all aboard captured

**There is a SURRENDER ending**, which nothing in this project had noticed.
Black holes and star explosions are separate endings too -- NOTES has recorded
since 2026-08-20 that black holes exist and that item 14 never mentioned them.

### The scoring line items, confirming the evaluation screen

    Mongols killed @ 10 each        Commanders killed @ 20 each
    Enemy bases destroyed @ 50 each Kill/day ratio @ 500 per day
    Casualties on board Lexington   Stars destroyed @ -5 each
    Bases hit @ -200 each           penalties for loss of ship and for
                                    not completing mission

### Other mechanics named in the strings and not yet in the port

    SCANNER REPORT: Wreckage of Union ship present in quadrant.
    A distress signal is being received from a Union ship in quadrant
    COMMUNICATIONS: Mongol reinforcements are reported in quadrant
    Replenishing reserve life support. / Not on reserve life support.
    USE AN ITEM / Which item do you wish to use? / No items in inventory
    Landing party beaming up. / Landing aborted.
    NAVIGATION: Not orbitting a planet
    Captain, only <n> tubes ...        (the damaged-tube bands, in words)

### The lesson, and it is the same one three times this week

`strings.txt` has been in `reference/` since 2026-08-14 and is plain ASCII --
`grep` reads it perfectly. Nothing here needed an emulator, a debugger or a
capture session. Between this, the manual's encoding, and MEASURED.md's own
back catalogue, **the answer to "is this measured?" has been yes far more often
than I assumed.** Search what is already on disk before proposing to go and
find out.

## Five-item capture session: two closed, three not (2026-08-24)

Ran to close the numeric gaps the manual and the string table do not fill. One
item closed outright, one made real progress, three not reached. Recording what
was and was not done rather than implying a clean sweep.

### 1. Enemy count per level -- CLOSED

**[RE-OPENED AND CLOSED PROPERLY 2026-08-26. The fit below reproduces all
nineteen readings and is still the wrong shape -- there is a percentage shave
and a three-ship siege in it. See the enemy-count section at the end of this
file, and note that the reasoning used here to reject the PREVIOUS fit applies
just as well to this one.]**

New readings: **level 1 = 21, level 4 = 47, level 5 = 55.** With the ten level-3
samples and the earlier five, that is nineteen readings, and they fix the
formula:

    enemies = 10 + 8 * level + rand(0..8)

    level 1   18, 21                            predicted 18..26
    level 2   30, 32                            predicted 26..34
    level 3   34 37 37 38 38 38 40 42 42 42     predicted 34..42
    level 4   42, 47                            predicted 42..50
    level 5   53, 55                            predicted 50..58

Level 3's ten samples span **exactly 34..42**, both endpoints. The level-1 and
level-4 readings land exactly on this model's minima.

This replaces `level*10 + rand(0..12)`, which the same data also satisfies but
which allows level 3 to roll 30..33 -- ten samples never did, and missing four
of thirteen values ten times running is about one chance in forty.
`core/trek.h` and `core/trek.c` are updated and `core/test/test_trek.c` now
asserts the per-level ranges as literals, so changing the constants fails the
test instead of moving the band with it.

### 2. SYSTEM_DAMAGE_THRESHOLD -- PARTIAL, two samples

**[SUPERSEDED 2026-08-26 by the binary. There is no threshold; the roll is
per turn. Both samples below are consistent with it. See the damage-report
section at the end of this file.]**

    860 units across 3 hits  ->  Shields 100% -> 0%   (3 casualties)
                                 Transporter 100% -> 0% (8 casualties)
    495 units across 2 hits  ->  nothing damaged

So: **damage takes a system to ZERO, not to a reduced percentage** -- both
events did -- **each damage event carries casualties**, and **not every
penetrating hit damages anything**. Two samples fix neither the probability nor
whether 0% is always the result. Still open.

### A panel swap found by getting it wrong

The bar reader written for this returned nonsense for several turns because
**when Life Support is damaged the SYSTEMS STATUS panel is REPLACED by a LIFE
SUPPORT / RESERVE, DAYS gauge**, and the ten percentages move to the MAIN
VIEWER as text. The manual explains the two-day reserve; nothing recorded that
the console rearranges itself to show it. Any future bar-reading tool has to
check which panel is present first.

### 4. HAIL -- two outcomes captured, the one that was wanted was not

    COMMUNICATIONS: Hailing frequencies open...  No response.
    COMMUNICATIONS: Hailing frequencies blocked by subspace interference.

Both are the no-base-in-range case. What a StarBase actually says is still
uncaptured.

### 3 and 5 -- NOT DONE

The laser heat band and the docking quantities per base type were not reached.

### Incidental confirmations

- Enemy ship colours are the manual's: light blue battleship, red command,
  green supply ship, all three seen together in one quadrant.
- `w5` and `m8544` inline forms both work, as the manual documents.
- The laser dialog asks for an amount **per enemy ship**, naming each --
  "Commander...", "Mongol...", "Supply ship..." -- and reports "destroyed!".
- A long-range tractor beam pulled the ship a quadrant and the move drew fire
  from three ships in the same turn.

## The repair table is a table, not a product (2026-08-24)

Found while auditing what still needs the emulator against what the manual
already says -- an audit that should have run before the emulator list was
written, and which cost two items off it.

The manual prints all four repair rates (l.464-469):

    1x    normal repairs, work evenly divided among systems
    2.5x  normal repairs while docked at a starbase
    3x    repairing only a selected system
    5x    repairing a selected system while docked at a starbase

**Those four numbers are not a product.** 2.5 x 3 is 7.5, and the manual says
5. So the original does not compose the two conditions; it prices the combined
case separately, and cheaper than multiplying would make it.

`core/trek.c` composed them. It scaled the base rate by 47/20 when docked and
then multiplied by a `REPAIR_FOCUS_FACTOR` of 3, which is **about 7x** for a
focused system at a StarBase against the manual's 5x -- 69 points a stardate
where the table says 50 at half a stardate, a 38% overshoot on the one repair
case a player in trouble actually uses.

The table was quoted verbatim in `trek.h` directly above the constant, and the
note there had already done the arithmetic: "3 x 20 = 60 against 5 x 20 = 100
docked-and-focused". The rates were written down and then not used. Nothing
here needed measuring; it needed reading what was already on the page.

Now four constants rather than a multiplier, in points per stardate:

    REPAIR_PER_STARDATE                20   MEASURED off the original
    REPAIR_PER_STARDATE_DOCKED         47   MEASURED off the original
    REPAIR_PER_STARDATE_FOCUS          60   manual, 3 x 20
    REPAIR_PER_STARDATE_FOCUS_DOCKED  100   manual, 5 x 20

`core/test/test_trek.c` asserts all four at half a stardate, where no row
saturates and each lands on its own figure: 10, 23, 30, 50. The fourth is the
regression test -- composing gives 69.

**What is still open:** the two focused rows come from the manual's relatives,
not from the original. The measured docked rate is 47 against a stated 2.5x of
20, so the manual rounds somewhere. The STATE OF REPAIR dialog prints Docked
and Undocked times side by side and is what settled the first two rates; it has
never been read with a system concentrated on. That is a one-screenshot
measurement and it belongs on the emulator list -- unlike the focus factor
itself, which was on that list this morning and should not have been.

## Run 1 of the measurement plan (2026-08-24)

Four items, and the first one overturned two constants and a rule.

### The stardate is not what the message log prints

**`3500:34` in the message panel is `stardate : message number`, not a
stardate with a colon for a point.** Jamie caught this while a run was in
flight -- I had a counter at 182060 incrementing once per HAIL and read it as
elapsed tenths. It is the message counter, which is also what `A#` (acknowledge
message) numbers.

That closes the "panel-versus-log stardate discrepancy" as an open item:
**there is no discrepancy.** The panel and the log agree; the log was being
misread. `MSGS` prints each entry headed `Stardate: 3500.0` in full, which is
the check that settles it.

It also means an earlier note here -- "HAIL costs a turn, the stardate moved
3500.1 to 3500.2" -- was the same misreading. **HAIL costs no time at all.**

### The repair table, measured on actual repair

The STATE OF REPAIR dialog is an ESTIMATE and its rounding does not invert
cleanly -- 100 points to go displays 5.1 days at a rate that is exactly 20 a
day. Every earlier repair figure in this file came from that dialog. These come
from writing a system to 0%, letting known time pass, and reading the
percentage back.

    condition              points per stardate
    undocked, no focus      20      (every damaged system, floor'd)
    docked,   no focus      50
    undocked, focused       60      focused system only
    docked,   focused      100      focused system only

Sample: docked with no focus, fourteen consecutive DOCK turns of 0.1 stardates
each, two systems damaged, both climbing 0,5,10,...,70 in lockstep. Focused and
docked, eleven turns, the focused system climbing by 10 a turn and the other
sitting at 0 the whole way.

**So the manual's 1x / 2.5x / 3x / 5x is exact on a base of 20**, and
`REPAIR_PER_STARDATE_DOCKED 47` is REFUTED -- it is 50. The 47 came from
solving across the dialog's rounded estimates, which is measuring the display
instead of the mechanic.

### F)ix starves everything else

**While a repair focus is set, every other damaged system repairs at ZERO.**
Not at a reduced rate -- at nothing. Shields sat at 0% for eleven consecutive
turns while the focused lasers climbed 0 to 100.

That is the manual's "at the expense of other systems" read literally, and it
is a different mechanic from the one this port implements (every system at the
base rate, focused system faster). It also explains why the rates compose the
way they do: there is no budget being divided, there are four rates and a rule
about who gets one.

Note the dialog does NOT show this. With lasers focused it still prints a
finite Repair Time for the warp engines -- which will never arrive while the
focus stands.

### Docking

`DOCK` costs **0.1 stardates**, and it can be issued while already docked,
which is a convenient way to pass time in fixed steps.

A StarBase **restores energy and shields to full in a single turn** -- 1200 to
5000 and 400 to 2500 -- not a fixed quantity per visit. Supply and Research
stations were not reached this run.

### Turn costs, as far as run 1 got

    HELP  MSGS  SND  REPAIR  FIX  INFO  W<n>   0.0
    HAIL                                       0.0
    DOCK                                       0.1
    M (quadrant change)          measured 1.2222 and 2.5972 at warp 3

### Incidental, and all of it new

- **HAIL with a base in range**, which run 4 was going to go looking for:
  `COMMUNICATIONS: The StarBase in 5-5 is responding to our hail.`
- **A distress signal, with its full shape**: `Planet Xevious-8, quad 8-8,
  requests evacuation. They can only hold out until 3508.7.` So the mechanic
  carries a named planet, a quadrant and a DEADLINE STARDATE.
- **A base under attack**, same shape: `StarBase in 6-6 ... under attack` with
  a stardate.
- **A long range tractor beam**: `Lexington caught in long range tractor beam.
  Pulled to quadrant 5-3.`
- **Progressive damage**: `EnergyConverter failing. Now at 87%` -- systems
  degrade in steps and announce it, rather than only taking hits.
- **In-quadrant movement travels a straight line and is BLOCKED by objects on
  the path**: `Blocked by object at 5-4` for a move from 4-4 to 6-3 with a star
  at 5-4. This port teleports.
- `F)ix`'s numbering is 1..12 in exactly our `SYS_*` order, confirmed off its
  own list screen.
- Max warp = 1 + 0.09 x percentage, confirmed twice by refusals at 0%:
  `engines cannot take over warp 1.0`.
- The `DOCK` refusal with no base adjacent is `NAVIGATION: Not adjacent to
  planet.` -- the original's own wording, planet and all.

## Run 1, second half: the turn-cost table, and the planet chain entire

### Time advances ONLY on movement and docking

Measured by reading the stardate real either side of each command. Everything
below is **0.0 stardates**:

    HELP  MSGS  SND  REPAIR  FIX  INFO  W<n>  HAIL  MAX  A#
    shields up  shields down  LASERS  TORPS  ENERGY  SAVE
    ORBIT  LAND (transporter)  USE

    DOCK                     0.1
    M, quadrant change       variable

That is the whole table, and it is a much stronger statement than a list of
costs: **combat is free.** Firing lasers, launching torpedoes, raising shields,
diverting power, scanning an enemy -- none of it moves the clock. The clock is
distance and docking.

Quadrant-change samples at warp 3, all one or two quadrants: 0.8227, 0.9663,
1.2222, 2.5972, 2.7330. One-quadrant moves are not a constant, so the cost is a
real distance, not a quadrant count.

### Movement is a straight line and objects block it

Both kinds. An in-quadrant move from 4-4 to 6-3 with a star at 5-4 gives
`Blocked by object at 5-4`. A quadrant-change move is ALSO a path through the
current quadrant: from 8-6 sector 4-4 heading west, `Blocked by object at 4-3`.
Stepping to a clear row first and repeating the same move works.

This port teleports in both cases.

### The planet chain, captured end to end

`ORBIT` (0.0 stardates):

    NAVIGATION: Entering standard orbit.
    Planet Sigma-7, Type O.
    SCIENCE: Scanners indicate the presence of energium on planet.

So planets are named `<Greek letter>-<digit>` and carry a TYPE letter, and the
orbit scan is what reveals energium.

`LAND` opens `LANDING PARTY`:

    How do you wish to get to the planet?
      1) Shuttle Craft
      2) Transporter
      3) Abort landing

Transporter costs 0.0 stardates and runs a four-beat sequence: party to
transporter, party on planet, `energium successfully mined`, party beaming up.

`USE` opens `USE AN ITEM` with a numbered inventory carrying quantities:

    Which item do you wish to use?
      1. Raw energium (1)

Refused outside the gate, in the First Officer's voice: regulations do not
allow such a dangerous procedure except under extreme low energy conditions.
Inside the gate (energy 500 of 5000, shields 300 of 2500) it asks first --
Engineering warns it is an extremely hazardous procedure and asks Y/N -- and
then:

    Attempting to load energium...
    Crystal loaded...it appears good!
    Energy levels increasing...

**Energy went 500 to 7435 and shields 300 to 2500.** The energy figure is the
result worth staring at: 7435 is well ABOVE the 5000 maximum every other
mechanic clamps to. Energium overcharges the ship.

`it appears good!` is doing obvious work -- there is a bad crystal branch, and
it was not sampled.

### The ENGINEERING REPORT, which E opens

    1) Main Energy:      3749.5   75%
    2) Impulse Engines:   493.8   99%
    3) Shields:          2500.0  100%

    Divert energy (Y/N)? _

    Systems marked in red are damaged

Three pools, absolute value and **percentage of that pool's maximum** -- which
independently confirms the note in trek.h that this percentage is CHARGE and
not state of repair. 3749.5 of 5000 is 75%.

### The attack deadline is real and it is enforced

`The StarBase in 4-2 reports that it is under attack. They can last until
3512.7.` was followed, after 3512.7 passed, by `The StarBase in 4-2 has been
destroyed`, and the galaxy chart's base digit for 4-2 went from 1 to 0.

So base-under-attack is a timed event with a stardate deadline and a permanent
consequence for ignoring it. Same shape as the distress signal captured
earlier: `Planet Xevious-8, quad 8-8, requests evacuation. They can only hold
out until 3508.7.`

### The galaxy chart is E-B-S, coloured per digit

Three digits per quadrant: enemies, bases, stars -- enemies red when non-zero,
bases orange, stars green. Quadrant 5-5 read `016` and its scanner showed no
enemies, one base and six stars.

### The shield routine: the 0.8, RESOLVED -- and the law CONFIRMED

`fn 0x16844` is the enemy-fire and damage routine (2,707 bytes; strings
"Shields absorb ", " unit hit from ", "Shields failing. Now at ", "Lexington
destroyed.", "StarBase shields protect Lexington").

At 0x17044 and again at 0x17085 it multiplies a real by a constant whose six
bytes are `80 CD CC CC CC 4C`, which decodes to **exactly 0.8**. At 0x17085
the product is compared against 1.0 and, if greater, printed as the amount in
"Shields absorb N unit hit from ...". At 0x17044 the same product is combined
with the shield real at [0x1D60].

Reading the routine from its ENTRY instead of from its message sites resolved
it, and the answer confirms the law this port already had.

**The absorption formula, at 0x16A51..0x16A96:**

    [bp-8] = damage * (shields / 2500) * ([0x235C] / 100)

with 2500 and 100 decoded from the real constants at 0x16A5A and 0x16A73 --
both exact round numbers, which is itself a check on the decoding. **2500 is
SHIELD_MAX**, and **[0x235C] is element ONE of a twelve-word array**, which is
the index `SYS_SHIELDS` has in this port. So the three-factor shape is
CONFIRMED, including the system term that this file has called "a hypothesis
that fits rather than a measurement" since the law was written.

**The 0.8 applies to the POOL DRAIN and the PRINTED FIGURE, not to the
protection.** It appears at 0x17044, where the shield real is updated, and at
0x17085, where the reported amount is built -- never on the value that decides
how much gets through.

That placement is the only one under which both surviving measurements hold:

    charge 1000   0.4 * 0.8 = 0.32   sweep read 0.33
    charge 1500   0.6 * 0.8 = 0.48   sweep read 0.47
    charge 2000   0.8 * 0.8 = 0.64   sweep read 0.65
    full shields  energy untouched, as six clean hits showed

and the fitted `charge / 3100` was approximating **2500 / 0.8 = 3125**.

**A retraction goes with it.** The absorption sweep was recorded as INVALIDATED
by the shield-write corruption, and the law was left "unresolved rather than
measured" on that basis. The binary now predicts that sweep's three readings to
within one percent, so the sweep was measuring the POOL DRAIN correctly all
along. What was wrong was reading it as the protection.

**And a test in this port was over-reading its own measurement.** It asserted
the pool loses the WHOLE hit at full shields. The note it came from says "the
PRINTED FIGURE comes out of the shield pool entirely and main energy is
untouched" -- the printed figure, which is 0.8 x the protection. Energy
untouched was the measured part and still holds; "the pool loses 409 of 409"
was inference.

### RETRACTED: fn 0x15C07 is the SPY, not the distress signal

**The 2026-08-26 entry that read this routine as the distress signal was
wrong, and the settled-planet conclusion drawn from it was wrong with it.**
Retracted in full; what the routine really does is below.

How it went wrong, because the shape is worth recognising. The routine has
only TWO `mov di, imm16`, so its segment base solved on ONE match instead of
the usual dozen -- and that one match was the anchor itself, which is
circular. The entry recorded that caveat at the time, which is the only reason
it was catchable. **A base solved on one string is not solved.**

What broke it open was an unrelated read: the shield law uses `[0x235C]` as a
percentage, and `0x235C` is `0x235A + 1*2`. Counting references to `0x235A`
across the binary then names the array outright -- `fn 0x0F1D4` uses it six
times and is the STATE OF REPAIR display ("System / Repair Time / Docked
Undocked"), and `fn 0x22773` is the viewer's system page.

**So DS:0x235A is the TWELVE SYSTEM REPAIR PERCENTAGES**, and DS:0x1188,
stride 16, is the twelve SYSTEM NAMES -- "EnergyConverter" is fifteen
characters and fits exactly. Not planets, and not a population.

### The distress signal is LOCATION-TRIGGERED, not scheduled (2026-08-26)

`fn 0x15105`, 80 bytes, and it is not an event at all:

    if ([0x1E1C] != ship.quad_y) return
    if ([0x1E1E] != ship.quad_x) return
    if (deadline [0x1DA2] <= stardate [0x1D42]) return
    print "COMMUNICATIONS: A distress signal is being received from the planet."

**You hear the distress signal by FLYING THERE.** It fires whenever the ship
is in the settled planet's quadrant with time still on the clock -- a
different mechanic from the base-under-attack warning, which is scheduled and
arrives wherever you are. Worth knowing before building one as the other.

This also fixes the string attribution: the distress text belongs to fn
0x15105 at segment base 86208, not to fn 0x15C07 (the spy). The text was right
all along; the routine was wrong.

**AND NOTHING EVER MARKS A SETTLEMENT DESTROYED.** ORBIT computes it live --
`if stardate > deadline` insert "destroyed " -- and the only writes to the
deadline are the one in fn 0x151D0 and an initialiser. There is no flag, so
this port's stored PF_RUINED was the wrong shape and is gone; a derived
`planet_settlement_lost()` replaces it.

### A PLANET'S NAME IS ITS QUADRANT (2026-08-26) -- EXACT

`fn 0x151D0`, the evacuation message, builds the planet's name like this:

    mov ax, [0x1E1E]      ; the settled planet's quadrant COLUMN
    mov dx, 13
    mul dx                ; x 13 -- the name table's stride
    add di, 0x1075        ; -> planet_name[x]

and appends the ROW separately with Str(). So:

    name  = planet_name[quad_x]        digit = quad_y

**Nothing about a planet's name is stored.** That is why the table has exactly
EIGHT entries: one per column.

Eleven for eleven against the PLANET LIST photograph -- 5-1 Andromeda-5, 5-4
Gallista-5, 5-5 Gamma Regula-5, 5-6 Sigma-5, 6-2 Ceti Alpha-6, 6-5 Gamma
Regula-6, 6-7 Vega-6, 7-2 Ceti Alpha-7, 7-4 Gallista-7, 7-8 Xevious-7, 8-1
Andromeda-8.

The ROW half was already measured seven times. The COLUMN half is new, and it
explains what seven samples could not: why Gallista appears twice (both in
column 4), and why the name table has eight entries and not seven or twelve.

This port had a random name index in the Planet record, which could put
Xevious in column 1. The field is gone.

### THE EVACUATION DEADLINE (2026-08-26)

Same routine, at 0x15212:

    if (stardate <= [0x1D9C]) return       ; a "next event" gate
    [0x1D9C] = 9999.0                      ; and NEVER AGAIN
    [0x1DA2] = stardate + 3.0*Random + 1.0 ; ONE TO FOUR stardates of warning

Both constants decode exactly (9999.0 and 3.0). The 9999 is this game's
"never": stardates run around 3500, so setting the gate there means **a galaxy
gets ONE evacuation**, which fits a single settled planet.

The message assembles as "Planet <name>-<row>, quad <row>-<col>, requests
evacuation. They can only hold out until <deadline>." with the deadline
printed 6:1 -- matching the captured "Planet Gallista-8, quad 8-4, requests
evacuation. They can only hold out until 3516.5."

### THE SETTLED PLANET: there is exactly ONE (2026-08-26)

Found where it should have been looked for -- in ORBIT, which prints the
settlement line. `fn 0x0E092` at 0x0E195, before it decodes the per-quadrant
byte at all:

    if ([0x1E1C] != ship.quad_y) skip
    if ([0x1E1E] != ship.quad_x) skip
    "SCIENCE: Scanners show a "
    if ([0x1D42] > [0x1DA2]) "destroyed "      ; stardate past a deadline
    "settlement on the planet."

**A SINGLE PAIR OF GLOBALS holds the settled planet's quadrant**, so a galaxy
has exactly one. And generation writes that pair INSIDE THE ENERGIUM BRANCH of
the planet loop (0x05363), so every energium world overwrites it and the last
one wins -- which finally explains the "remembered quadrant" recorded as
unexplained when that loop was decoded. **The settled planet is always an
energium planet.**

The test runs BEFORE the byte is decoded and falls through afterwards, so the
settled world gets BOTH scan lines: the settlement and then the energium.

The same globals are read by `fn 0x15105` ("A distress signal is being
received...") and `fn 0x151D0` (", requests evacuation. They can only hold out
until "). That is the whole evacuation mechanic and the source of the +200
rescue line, and it hangs off one planet.

This retires two wrong readings recorded earlier the same day: settlers as a
fourth per-planet find (this port's invention) and a twelve-entry table at
DS:0x1188 (which holds the SYSTEM names). Both are struck.

Not modelled yet: the DEADLINE at [0x1DA2], so a settlement in this port is
never "destroyed". That wants fn 0x151D0.

### THE SPY, read out of the binary (2026-08-26)

`fn 0x15C07`, and the strings just before `fn 0x15D6E` name it: "SECURITY:
A spy has been captured aboard the Lexington, but he has damaged the ..."

    Random(150); if != 0 return              ; ONE TURN IN 150
    if [0x1DF0] <= 7 return                  ; high V only
    idx = f(Random(12))                      ; a SYSTEM
    print "...he has damaged the " + system_name[idx]
    [0x235A + idx*2] -= 10 + Random(90)      ; 10..99 points off, floored at 0

A mechanic this port has no part of: a spy is caught roughly once in 150
turns and takes ten to ninety-nine percent off one random system on the way
out. The `[0x1DF0] > 7` gate still supports V = level + 4 -- a spy only at the
top command levels is coherent -- but it says nothing about rescues.

**Where settled planets live is OPEN AGAIN.** The per-quadrant byte at
DS:0x24A9 holds only three find values, and the second structure has NOT been
found. `PFIND_SETTLERS` in core/planet.h remains this port's invention.

### The shield routine: the 0.8, RESOLVED -- and the law CONFIRMED

`fn 0x16844` is the enemy-fire and damage routine (2,707 bytes; strings
"Shields absorb ", " unit hit from ", "Shields failing. Now at ", "Lexington
destroyed.", "StarBase shields protect Lexington").

At 0x17044 and again at 0x17085 it multiplies a real by a constant whose six
bytes are `80 CD CC CC CC 4C`, which decodes to **exactly 0.8**. At 0x17085
the product is compared against 1.0 and, if greater, printed as the amount in
"Shields absorb N unit hit from ...". At 0x17044 the same product is combined
with the shield real at [0x1D60].

Reading the routine from its ENTRY instead of from its message sites resolved
it, and the answer confirms the law this port already had.

**The absorption formula, at 0x16A51..0x16A96:**

    [bp-8] = damage * (shields / 2500) * ([0x235C] / 100)

with 2500 and 100 decoded from the real constants at 0x16A5A and 0x16A73 --
both exact round numbers, which is itself a check on the decoding. **2500 is
SHIELD_MAX**, and **[0x235C] is element ONE of a twelve-word array**, which is
the index `SYS_SHIELDS` has in this port. So the three-factor shape is
CONFIRMED, including the system term that this file has called "a hypothesis
that fits rather than a measurement" since the law was written.

**The 0.8 applies to the POOL DRAIN and the PRINTED FIGURE, not to the
protection.** It appears at 0x17044, where the shield real is updated, and at
0x17085, where the reported amount is built -- never on the value that decides
how much gets through.

That placement is the only one under which both surviving measurements hold:

    charge 1000   0.4 * 0.8 = 0.32   sweep read 0.33
    charge 1500   0.6 * 0.8 = 0.48   sweep read 0.47
    charge 2000   0.8 * 0.8 = 0.64   sweep read 0.65
    full shields  energy untouched, as six clean hits showed

and the fitted `charge / 3100` was approximating **2500 / 0.8 = 3125**.

**A retraction goes with it.** The absorption sweep was recorded as INVALIDATED
by the shield-write corruption, and the law was left "unresolved rather than
measured" on that basis. The binary now predicts that sweep's three readings to
within one percent, so the sweep was measuring the POOL DRAIN correctly all
along. What was wrong was reading it as the protection.

**And a test in this port was over-reading its own measurement.** It asserted
the pool loses the WHOLE hit at full shields. The note it came from says "the
PRINTED FIGURE comes out of the shield pool entirely and main energy is
untouched" -- the printed figure, which is 0.8 x the protection. Energy
untouched was the measured part and still holds; "the pool loses 409 of 409"
was inference.

### THE DISTRESS SIGNAL, and where settled planets live (2026-08-26)

`fn 0x15C07`, 359 bytes, decoded from raw bytes:

    Random(150); if != 0 return                 ; fires ONE TURN IN 150
    if [0x1DF0] <= 7 return                     ; and only at high V
    idx = f(Random(12))            -> [0x1CF4]
    message := "COMMUNICATIONS: A distress signal is being received
                from the planet" + NAME + suffix
    NAME comes from a table at DS:0x1188, STRIDE 16, indexed by idx
    [DS:0x235A + idx*2] -= 10 + Random(90)      ; clamped at zero

~~**SETTLED PLANETS ARE A SEPARATE TWELVE-ENTRY TABLE.**~~ **RETRACTED
2026-08-27** -- and it took until 2026-09-02 for the retraction to reach this
paragraph, which is the whole reason the rule about grepping for the CLAIM
rather than the file exists. **DS:0x1188 is the SYSTEM NAMES table**, not
settlements. There is exactly ONE settled planet in a galaxy and it lives in a
pair of globals at DS:0x1E1C/0x1E1E, written inside the ENERGIUM branch of the
planet loop so that the last energium planet wins. See `core/planet.h`, which
has carried the correct reading since the day it was made.

What survives from this paragraph, and is still unread: the word at
`[DS:0x235A + idx*2]` that the distress signal DRAINS by 10..99 and floors at
zero. Population or countdown is not read.

~~So the port's `PFIND_SETTLERS` is modelling the right thing in the wrong
place... The faithful model is a second, small table of named inhabited
worlds~~ -- **also retracted with the paragraph above.** There is no such
table. The port models one settled planet, which is what the binary has.

**A THIRD SUPPORT FOR V = level + 4.** The gate here is `[0x1DF0] > 7`, so
distress signals -- and therefore rescues, and therefore the +200 scoring line
-- happen ONLY at the top command levels. With V = level + 4 that is levels 4
and 5, which is a coherent difficulty rule. The three constraints now are:

    11 - V          StarBase count, must stay small and positive
    cmp V, 9        in generation at 0x05482
    V > 7           this gate, which wants to select the top levels

all satisfied by V = level + 4 and none by V = level. Still a reading, and
still one emulator run from being a measurement.

CAVEAT on the strings: this routine has only two `mov di, imm16`, so the
segment base solved on ONE match rather than the usual dozen. The message text
is certainly right -- it is the only distress string in the binary and this is
the only routine that builds it -- but the suffix at di=0x0B45 did not resolve
and the exact wording of the assembled sentence is not confirmed.

### BLACK HOLES, and THE SECTOR MAP'S FORMAT (2026-08-26)

Both out of the MOVE routine, `fn 0x0C609`, which also owns the tractor beam
and the warp-damage message. Decoded from raw bytes at 0x0D000.

**A black hole is a SPACE in the sector map.** The move code tests
`cmp byte [di+0x2622], 0x20` and branches into the black hole handling when
the destination cell is a blank. Empty space is `'.'` (0x2E) and the ship is
`'E'` (0x45).

**So the sector map is at DS:0x2622, ASCII, STRIDE TEN** -- the index is
`sec_y * 10 + sec_x`, not `* 8`. That is the structure MEASURED.md has called
"separate from the enemy table" since the RAY session and never located. It
also explains the display directly: the original draws blank for a black hole
and a dot for vacuum, which is exactly what the bytes say.

**What entering one does** (0x0D0B6):

    Random(5)
      == 0   "NAVIGATION: The ship has entered a black hole..."
             ending code 7  ->  SHIP DESTROYED
      != 0   "...and has been thrown free in quadrant Y-X."
             qy,qx = Random(8)+1, retried while chart[q] >= 999
             sec_y,sec_x = Random(8)+1
             sector map[sec_y*10 + sec_x] = 'E'

So a black hole **destroys the ship one time in five** and otherwise throws it
to a uniformly random quadrant AND sector anywhere in the galaxy. Both
outcomes are why it confounds any sampled measurement taken by playing: the
throw silently changes the quadrant, and the destruction is attributed to
whatever the player last did.

Ending code 7 is the "pulled into black hole & destroyed" line, alongside 9
for the death ray.

The `chart[q] >= 999` retry looks like a sentinel rather than a real
threshold: the chart word packs `enemies*100 + type*10 + stars` and enemies
cap at four, so a real quadrant never reaches 999.

~~**Still open: where black holes are PLACED.**~~ **FOUND 2026-08-28, while
reading the death pod, four instructions further down the same routine.** At
0x016525, immediately after the pod's own roll in the new-game quadrant build
`fn 0x15F51`:

    if (Random(64) < 16)                          ; one quadrant in four
        ' ' into a free sector                    ; 0x016548

No galaxy-level record is consulted, so the DS:0x23E9 candidate this file
named is NOT it. **ONE QUADRANT IN FOUR, ON EVERY ENTRY** -- and the "once, in
the starting quadrant" that stood here for about an hour was the death pod's
mistake borrowed wholesale: the same routine, the same one-caller search, the
same refutation. A sweep in the same session read `holes=1` in quadrant 4-7,
flown to mid-game. See NOTES.md, 2026-08-28 (later).

**AND 0x007142 IS THE OTHER HALF OF THE DEATH RAY.** It is not a placement at
all. `fn 0x70C0` is reached from the RAY's outcome dispatch at 0x0074F5,
which rolls `Random(6)`:

    0  -> fn 0x7026   the ray WORKS
    1  -> fn 0x70C0(0)   "Death ray misfires."  message only
    2  -> fn 0x71F5   the mutants
    3  -> fn 0x70C0(1)   "Death ray misfires."  AND every empty cell in the
                         quadrant becomes a black hole on a coin flip
    4,5 -> fn 0x7247  fatal

so the misfire has TWO forms, and this port has both of them as one. See the
RAY note below: `trek.h` calls rolls 1 and 3 "cosmetic", and roll 3 is the
single most destructive thing the command can do short of killing you.

### Bases, read out of the binary (2026-08-26)

**TWO LOOPS, and the base TYPE is not in a separate array at all** -- it is
the middle digit of the chart word, exactly as the manual's long-range scanner
description says (1 StarBase, 2 research station, 3 supply depot). The open
item "Supply and Research docking quantities... wants the base-type array
located in memory" was looking for something that does not exist.

**StarBases** (fn 0x04FD1 at 0x053BB):

    count = 11 - V
    qy,qx = Random(6) + 2                  ; 2..7 -- never on the edge
    retry while (chart[q] % 100) >= 10     ; already has a base
    retry while |qy - last_y| <= 2 AND |qx - last_x| <= 2
    chart[q] += 10                         ; type 1

That third rule is the interesting one: **a StarBase is rejected if it lands
within two quadrants of the previously placed one**, so they are deliberately
spread across the map. Running for repairs is meant to be a decision.

**Research stations and supply depots** (0x0553D):

    count = 2 + Random(3)                  ; 2..4
    qy,qx = Random(8) + 1                  ; anywhere, edges included
    retry while (chart[q] % 100) >= 10
    chart[q] += (i odd) ? 20 : 30          ; ALTERNATING by loop parity

So the two rarer types alternate strictly rather than being rolled. This
port placed two to four bases TOTAL and rolled the type per base at
6/2/2 -- wrong in every particular.

**V is the open value**, the same [0x1DF0] the enemy total scales by. Two
things in this routine argue V = level + 4: `11 - V` has to stay positive and
small, and a `cmp V, 9` sits at 0x05482, which is meaningless if V is a
command level of 1..5. That reading gives 7 - level StarBases -- six at level
1, two at level 5, a sensible curve. FLAGGED, not confirmed. One emulator run
settles V and with it both this and the enemy count.

### THE PLANET MODEL, read out of the binary (2026-08-26) -- EXACT

`fn 0x04FD1` at 0x052B3, decoded from raw bytes:

    count = 10 + Random(10)                         ; 10..19 planets
    qy,qx = Random(8)+1;  retry while ARRAY[q] != 0 ; ONE PER QUADRANT
    ARRAY[q] = Random(3) + 1                        ; class: 1=M 2=N 3=O
    if (class <= Random(5))   ARRAY[q] += 10        ; energium
    else if (Random(2) == 0)  ARRAY[q] += 20        ; Mongol supply station

The store is **one byte per quadrant at DS:0x24A9 holding `find*10 + class`**,
and ORBIT decodes it by dividing by ten -- `cmp byte [q], 20 / jbe` then
`cmp byte [q], 10 / jbe`, mapping to the Mongol, energium and nothing scan
lines, with the remainder printed as "M." / "N." / "O."

**THE FIND DEPENDS ON THE CLASS**, which this port's notes had recorded as
unmeasured:

    class M   energium 4/5   Mongol 1/10   nothing 1/10
    class N   energium 3/5   Mongol 2/10   nothing 2/10
    class O   energium 2/5   Mongol 3/10   nothing 3/10

A class-M world is worth landing on four times in five; a class-O world twice
in five. Overall, class being flat: energium 60%, Mongol 20%, nothing 20% --
against this port's shipped one-in-three for energium and an invented split
for the rest.

Three other things fall out:

  * **Ten to nineteen planets.** The ancestor's five-to-ten was the wrong
    population; the provisional twelve-to-twenty-two taken from the PLANET
    LIST photograph was close and is now exact. Eleven on page 602 with the
    rest on 601 is consistent.
  * **One planet per quadrant.** The generator retries an occupied quadrant.
    The port modelled a LIST because a 2026-08-21 note transcribed the page as
    three columns showing 6-4 twice; the photograph shows one column and all
    quadrants distinct.
  * **The class is flat**, `Random(3)+1`. The photograph's six N to three M
    and two O was noise, as eleven samples can be.

**SETTLERS ARE NOT IN THIS BYTE.** Only three values fit one decimal digit,
and the original plainly has settled planets -- ORBIT prints "[destroyed
]settlement on the planet" and LAND prints "Planet settlers found...". That
comes from a second structure this pass did not locate, and finding it is the
next job on this routine.

The generator also REMEMBERS the last energium planet's quadrant in
[0x1E1C]/[0x1E1E], which is unexplained and probably feeds an event.

### The landing party's casualties, read out of the binary (2026-08-26)

`fn 0x0E3A1`, decoded from raw bytes at 0x0E435:

    mov ax,5; Random(5); or ax,ax; jnz  ->  attacked only on a ZERO roll
    "Landing party attacked..."
    mov ax,5; Random(5); add ax,2       ->  casualties = 2..6
    " casualties."

So a raided landing party **always loses at least two**, and never more than
six. This port shipped 1..10, invented.

The attack itself is **one in five**, which changes the feel of the Mongol
supply station considerably: most landings on one come back empty rather than
short. WHICH case that gate guards is not yet read -- the block sits between
the settlers case and the energium case in a switch over the find -- so it is
either the Mongol station specifically or a risk on landings generally. The
port keeps it on the Mongol find, where the SCIENCE line puts it.

The routine also resolved all 29 of its strings cleanly, which is what
confirms the segment base of 37648 shared with the crystal handlers.

### The energium crystal, read out of the binary (2026-08-26) -- EXACT

`fn 0x0ED3B`, decoded from raw bytes at 0x0EE80. **A crystal is one roll of
six and nothing else:**

    roll 0        DEFECTIVE   energy -= Random(1000), floored at zero
    roll 1, 2     DUD         "The crystal appears to be damaged."
    roll 3, 4, 5  GOOD        calls fn 0x0934E

So it **works half the time**, duds a third of the time, and hurts one time
in six. Everything this port shipped here was invented and all of it was
wrong: 5% escalating (the ancestor's `cryprob`, doubled per use) and a 10%
dud. **The escalation does not exist** -- there is no state, each crystal is
an independent roll -- and the saved byte that carried it is gone.

The defective branch **subtracts energy**. It does not wreck the converter,
which is what the port did on the strength of the message "Damage to main
energy systems."

`fn 0x0934E`, the success handler, is two lines:

    energy  += V * (700 + Random(700))
    shields += V * (300 + Random(300))

Shields are **topped up by an amount, not set to full**. The measured
300-to-2500 was that amount hitting the ceiling, which is a different mechanic
from assignment: a badly damaged shield pool does NOT come back full from one
crystal.

**V = 5, and the single measurement pins it.** The gain was 6935, and
`6935 = V * (700 + r)` with `r` in 0..699 has exactly one solution in V --
5 x 1387. No other divisor lands in the window.

V was assumed to be the command level for about an hour. The evidence is
against it: run 1's tooling starts games at level 3 and V is 5. One emulator
run would settle it beyond doubt -- load a crystal at a KNOWN level and divide
-- because a second sample at a different level separates "constant 5" from
"the level" immediately.

### Galaxy generation, read out of the binary (2026-08-26)

`fn 0x04FD1` is galaxy generation: it references the galaxy array at DS:0x2560
fifteen times and the ship's quadrant eight times each, and it holds
TWENTY-SEVEN Random() calls -- the enemy count, the placements, and by every
indication the planets, stars, bases and black holes as well.

**CONFIRMED, the enemy placement loop** (0x51CD..0x5255), decoded from raw
bytes rather than trusted to one disassembly pass:

    qy = Random(8) + 1
    qx = Random(8) + 1
    if GALAXY[qy*16 + qx*2] >= 100: retry      ; already holds an enemy
    n = Random(4) + 1                          ; 1..4 for this quadrant
    if placed + n > total: n = total - placed  ; clamp to what is left
    GALAXY[q] += n * 100

So a quadrant gets ALL of its enemies in one placement of one to four, and is
never picked twice. This port increments one at a time to a cap of four, which
reaches the same cap by a different distribution -- worth knowing, not
obviously worth changing.

It also confirms the packing outright: **enemies are stored x100** in the same
word as bases (x10) and stars (x1), which is the chart's three digits.

**NOT CONFIRMED, the total.** The arithmetic at 0x5181 is

    r1 = Random(10);  r2 = Random(10);  V = [0x1DF0]
    total = r1 + ((V - 3) * 8 * (100 - r2)) / 100

and the `* 8` matches `ENEMY_PER_LEVEL` literally. But if V were the command
level, level 3 would yield 0..9 enemies against nineteen measured samples of
34..42, so **V is DERIVED from the level and the relation is not yet pinned**.
[0x1DF0] has two writers: one at 0x08384 fed by a string-to-number conversion
(the setup screen), and one at 0x15064 that reads it, compares against 6, and
does `add ax, 4`. V = level + 4 fits the level-1 and level-5 samples and is
marginally tight at level 3, which is not good enough to assert.

Resolving it is one emulator run -- start a game at a known level and read
[0x1DF0] -- which is exactly the division of labour this method is for: read
the shape statically, confirm one value dynamically. Until then the port keeps
its fitted `10 + 8*level + rand(0..8)`, which matches all nineteen samples.

### RAY's ODDS, read out of the binary (2026-08-26) -- EXACT

Not sampled. Read from the code, so these are Anderson's own constants and
carry no confidence interval. Four DOSBox sessions failed to estimate this;
one disassembly settled it.

The routine is at file offset 0x7375, immediately after its own strings, and
its segment base is 24512 -- solved by taking each `mov di, imm16` in the
routine as though it addressed "Preparing death ray..." and keeping the base
under which the other eight also resolve. All nine do:

    0x12AD  SCIENCE: Scanners show no enemy ships in this quadrant.
    0x12E5  WEAPONS CONTROL
    0x12F5  Captain, I wish to remind you that
    0x1318  the death ray is experimental in
    0x1339  nature and has been highly prone
    0x135A  to failures. Are you sure that you
    0x137D  wish to continue <Y/N>?
    0x1396  Preparing death ray...
    0x13AD  Firing!

Then, at 0x750E:

    mov ax, 6
    lcall 0x2692:0x1150        ; Random(6)
    cmp ax,0  -> call 0x7026
    cmp ax,1  -> push 0; call 0x70c0
    cmp ax,2  -> call 0x71f5
    cmp ax,3  -> push 1; call 0x70c0
              -> call 0x7247   ; ax = 4 or 5

**Random(6), and SIX rolls over FOUR handlers.** Two handlers take two rolls
each, so the outcomes are not equiprobable even though the roll is:

| roll | handler | outcome                                            | odds |
|------|---------|----------------------------------------------------|------|
| 0    | 0x7026  | `It worked!` -- every enemy in the quadrant dies    | 1/6  |
| 1, 3 | 0x70c0  | `Death ray misfires.` (two variants)               | 2/6  |
| 2    | 0x71f5  | `Nothing happens...` half the crew become mutants  | 1/6  |
| 4, 5 | 0x7247  | `The apparatus is going unstable!` -- SHIP LOST    | 2/6  |

So a death ray **works one time in six and destroys the ship one time in
three**. The note that called it "four outcomes" was counting handlers.

The fatal one is certain rather than inferred: 0x7247 is the ONLY handler that
writes the ending-code byte, `mov byte ptr [0x26c6], 9`, and the caller tests
`cmp byte ptr [0x26c6], 0` immediately after the roll. Code 9 is the death ray
explosion, which matches the loss report captured the same day.

The two misfire variants really are different -- 0x70c0 branches on its
argument at `cmp byte ptr [bp+4], 1` and only the arg-1 path runs a nested
loop over a grid. What each does is NOT yet read.

#### Addresses this fixed, for free

  * **The galaxy array is at DS:0x2560**, indexed `qy*16 + qx*2`, and each
    word packs **enemies*100 + bases*10 + stars** -- the chart's three digits
    are one number, and RAY tests for enemies with `idiv 100`.
  * **The ship's quadrant is [0x1DE4] (y) and [0x1DE6] (x)**, which is what
    the RAY routine indexes that array with. The address used for this in
    earlier sessions disagreed with the screen and was never resolved.
  * **[0x26C6] is the ending code**, 9 = destroyed by death ray.
  * **[0x1CC8] is the sound flag**, confirming what the music extraction found.

### RAY's fatal outcome, and its loss report (2026-08-26)

Captured at last, on the fourth attempt at sampling RAY. The death ray failed
and destroyed the ship, and the Top Secret memo it prints is the frame item 8
has been waiting for:

    From:   Commander, Earth Sector
    To:     Headquarters
    Date:   3500.0
    Re:     Loss of U.S.S. Lexington, RCB-92

    U.S.S. Lexington destroyed by death ray explosion this stardate, with
    loss of all aboard.  Results of operations prior to loss follow:

      Stardays in action:  0.0
      Mongol ships destroyed per stardate: 0.00
      Score: -730

So the fatal outcome's one-sentence cause line names the death ray explicitly,
which is what the frame varies between endings.

**AND THE SCORE CONTRADICTS A SETTLED CONSTANT.** This ship was destroyed with
nothing killed and no time elapsed, and the memo totals **-730**: -300
incomplete plus -430 casualties, with NO -200 ship-loss line. The Detailed
Evaluation read on 2026-08-24 for a combat death totalled -930 with the line
printed, which is what restored `SCORE_SHIP_LOST`.

Three readings that cannot all be simple:

    2026-08-20  -730  loss report      no ship-loss line   -> constant deleted
    2026-08-24  -930  detailed sheet   line printed        -> constant restored
    2026-08-26  -730  loss report      no ship-loss line

The pattern that fits all three is that **the MEMO and the DETAILED EVALUATION
are different documents with different totals** -- the memo omits the
ship-loss penalty and the sheet carries it -- and that the 2026-08-20 reading
was of a memo while the 2026-08-24 one was of a sheet. That is a hypothesis
from three samples, not a measurement. `SCORE_SHIP_LOST` stays as it is,
because the sheet is what this port renders; but the next loss report captured
should be read for this line specifically. See NOTES.md.

### The MAIN VIEWER is a PAGED INSTRUMENT DISPLAY (2026-08-26)

Found in `reference/strings.txt` while building the planet UI, not in a run.
The binary carries a run of viewer page titles, each with a numeric page code:

    SPACE COMM NET 411        SHIP STATUS 501
    STANDARD ORBIT 301        GRAV FIELD 502
    POWER DISTRIB 509         ARRAY MONITOR 504
    PLANET LIST 601           STRUC INTEGRITY 505
    PLANET LIST 602

Several carry their layout with them. POWER DISTRIB 509 is followed by
`PMAX   PAVL   PPCT` and `1:5000` `2:0500` `3:2500` -- the three energy pools
at their maxima, so that page is LIVE DATA and not decoration. ARRAY MONITOR
504 has ONLINE/OFFLINE rows, STRUC INTEGRITY 505 a four-row table.

Three things follow. The viewer is a multi-page instrument, not one display.
~~STANDARD ORBIT is the page it shows while orbiting a planet~~ -- **WRONG,
retracted 2026-09-02**: page 8 is reached at random like every other and tests
the orbit flag itself, falling through to page 6 when there is no orbit. See
"The MAIN VIEWER, read end to end" at the end of this file. And PLANET LIST needs TWO pages, so whatever the
viewer's capacity is, the planet list exceeds it.

The port draws STANDARD ORBIT 301 and nothing else of this; there is no
paging mechanism, and no measurement of how the original cycles them.

### The planet name's digit is the QUADRANT ROW (2026-08-26)

Not a new run -- a re-reading of what three sessions already captured, prompted
by building the model. Every planet name we have, against the quadrant it was
in:

    Gallista-5   quad 5-4        Cygnus-6     quad 6-4
    Gallista-6   quad 6-4        Andromeda-7  quad 7-1
    Sigma-7      quad 7-6        Gallista-8   quad 8-4
    Xevious-8    quad 8-8

Seven for seven. The digit is the 1-based quadrant ROW, so it is not part of
the name at all, and the seven names in the binary (Andromeda, Ceti Alpha,
Cygnus, Gallista, Gamma Regula, Sigma, Xevious) are the whole table. It is also
what makes the duplicates legible: Gallista-5 and Gallista-6 are two planets
sharing a name, disambiguated by row.

NOTES.md carried one of them as "Gallisto" from a screen capture. The binary
and the distress message both say **Gallista**.

### Not reached, and now the only run-1 item left

**Supply and Research docking quantities.** Every base this run turned out to
be a StarBase -- 5-5, 4-2 and the one hailed from two quadrants away all
announce themselves as StarBases. A StarBase restores energy and shields to
full in one 0.1-stardate turn. The other two types were never found, and
finding them wants either a base-type array located in memory or a longer
survey than run 1 had left in it.

## Run 2 (2026-08-24): laser heat IS in memory, and it does not do what we thought

### The settled negative was wrong, and the reason is instructive

`182124` is a **16-bit word that drives the Temp gauge**. Writing it moves the
bar; firing raises it. Two earlier sessions concluded laser heat was "not
stored as a Turbo Pascal real or a 16-bit integer of the fired amount anywhere
in the 338KB", and this memory was recorded as a settled negative.

Both halves of that search were wrong in the same way: it is not a real, and it
is **not the fired amount**. It is a small integer on its own scale, and the
game **caps it at 100**. Searching for 400 after firing 400 was never going to
find a 6.

The lesson stands as recorded -- a negative search result must say what
encoding was searched for -- but the conclusion drawn from it does not.

### The Temp gauge draws value x 10 against a 0..1500 scale

Written value 100 fills 80 of the bar's 121 pixels, which is 66% of a scale
labelled 0 / 1000 / 1500 -- so about 1000. Values above 100 saturate the bar.
Since the game itself never lets the value exceed 100, **the Temp bar can never
pass its own 1000 tick in normal play.**

### Heat does not degrade laser effectiveness. At all.

A fixed 400-unit volley at a fixed distance (4.1231 sectors, the enemy's
position rewritten before every shot so nothing else varied), with the heat
word written to a different value each time:

    heat written    0   20   40   60   80  100  120  140  160  200  300
    damage        263  263  263  263  263  263  263  263  263  263  263

Expected at full effectiveness is `400 x (1 - 4.1231/12)` = 262.56. Every
sample matched to the unit. Firing added +6 to the word each time up to the
cap.

So within everything the game can reach, **heat is a gauge and not a
mechanic**. `trek.h`'s PROVISIONAL 1240..1500 overheat band has no support:
the displayed value cannot get there.

Not proven: that no threshold exists at all. What is proven is that no value of
that word between 0 and 300 changes the damage by one point, and that the game
caps the word at 100.

### Effectiveness IS exactly the laser system's repair percentage

Same rig, heat zero, varying only the Lasers entry in the systems array:

    lasers    100%    75%    50%    25%
    damage     263    197    131     66
    ratio    1.0017 0.7503 0.4989 0.2514

Linear to within the rounding of an integer hit point. `effectiveness =
laser_pct / 100`, which is what this port already implements.

Taken with the section above, the manual's "laser effectiveness goes down due
to excess heat and due to damage from enemy fire" (l.330-331) is half
observable: the damage half is exact, the heat half never fires.

### The laser law, reconfirmed twice

`damage = energy x (1 - distance/12)` matched to the unit at two distances in
two different games: 500 units at 4.2426 gave 323 against 323.2 predicted, and
400 units at 4.1231 gave 263 against 262.56.

### Ship classes seen

A quadrant holding five enemies read 355, 355, 355, 355 and 320 hit points, and
the MAIN VIEWER named the one being fired at a **MONGOL BATTLESHIP** at 355.
The 320 matches the commander figure already on file.

### Two traps this run paid for twice

**Firing at five enemies kills you in one turn even from full shields.** The
return volley is per ship. The rig has to reduce the quadrant to one target, or
restore between every shot and check for game over.

**A destroyed ship restarts the program, and the restart eats your input.** Two
sweeps ran to completion against the setup screen, typing volley amounts into
"enter your command level" and reading plausible, entirely stale numbers out of
the old addresses. Every loop needs a liveness check -- here, that the enemy
record still holds the coordinates it was written with.

Also worth knowing: **the S.R. Scanner below 50% draws an empty quadrant**,
which looks exactly like a quadrant with nothing in it. Confirms SRSCAN_BLIND.

## Run 3 (2026-08-24): shields do not subtract, and a hit usually kills a system outright

Rig: one enemy, its coordinates and hit points rewritten before every turn, our
sector pinned, all twelve system percentages written to 100, energy and the
shield pool written to chosen values, then a turn taken by firing ZERO at the
enemy -- which costs no stardate time and still draws return fire.

### Raised shields are a different mechanic from a shield pool that subtracts

Three states, and they behave qualitatively differently:

**Shields DOWN.** The hit comes out of MAIN ENERGY. The shield pool is not
touched at all. Systems get wrecked.

    shields 2500 (down)  hit 472.7  shields unchanged  energy -472.7  EnergyConverter -> 0%
    shields 2500 (down)  hit 514.2  shields unchanged  energy -514.2  Life Support -> 0%, Transporter -> 0%
    shields 1200 (down)  hit 511.4  shields unchanged  energy -511.4  nothing
    shields  400 (down)  hit 500.0  shields unchanged  energy -500.0  Lasers -> 38%

**Shields UP and full.** The hit comes out of the SHIELD pool, energy is
untouched, and NO system is damaged. Six landed hits of 385 to 518 across two
blocks, every one absorbed whole, not one system touched.

    hit 409.7  shields 2500 -> 2090.3  energy unchanged  nothing
    hit 518.0  shields 2500 -> 1982.0  energy unchanged  nothing
    hit 385.2  shields 2500 -> 2114.8  energy unchanged  nothing

**Shields UP but nearly flat (200 of 2500).** The shields absorb only a small
part and the rest reaches energy, and systems start dying:

    hit 495.5  shields absorbed 32.2  energy -463.3  nothing
    hit 465.5  shields absorbed 30.3  energy -435.2  L.R. Scanner -> 0%
    hit 520.6  shields absorbed 33.9  energy -486.7  nothing
    hit 475.2  shields absorbed 30.9  energy -444.3  EnergyConverter -> 0%, Warp Engines -> 0%
    hit 482.1  shields absorbed 31.4  energy -450.7  Life Support -> 0%

The absorbed fraction across those five is 0.0650, 0.0651, 0.0651, 0.0650,
0.0651 -- tight enough to be a formula, not a roll. At a charge of 800 two
hits absorbed 136.4 and 212.3, which is NOT a constant fraction, so whatever
the law is it is not simply proportional to charge. ~~**Unresolved.**~~
**RESOLVED -- see the retraction above ("A retraction goes with it") and the
S/E ratio table below: this sweep was reading the POOL DRAIN correctly all
along, and what was wrong was reading it as the protection. The binary
predicts these readings to within one percent.**

What is settled is the shape, and it is not ours: `through = amount - shields`
is wrong. The shields are a proportional absorber whose share falls with
charge, not a bucket that subtracts.

### A hit that gets through usually destroys a system outright

Eleven system hits observed. The resulting percentage was **0** eight times.
The non-zero results were 5%, 22%, 38% and 42%. And **two systems can go in a
single turn** -- seen twice, Life Support with Transporter, and
EnergyConverter with Warp Engines.

`trek.c` takes 20 to 59 points off exactly one random system. Both halves are
wrong: the count is not always one, and the severity is not a modest bite --
it is usually annihilation.

**[RETRACTED 2026-08-26. It is NOT that shape -- the original never tests
what got through. The reading below of WHAT HAPPENS is right and the reading
of WHY is wrong.]**

The trigger is consistent with our `through >= SYSTEM_DAMAGE_THRESHOLD` shape:
no system was ever damaged while the shields absorbed the whole hit, and
systems were damaged on roughly three hits in five once a few hundred units
were reaching energy. The threshold itself is still not pinned.

### Class hit points

`INFO` on a 120-hit-point ship reads:

    Mongol Supply
      Sector:   3-1
      Range:    3.00
      Bearing:  180.00
      Shields:  80%

120 shown as 80% makes the **Mongol Supply class maximum 150**. Combined with
run 2, where the viewer named a 355-hit-point ship a MONGOL BATTLESHIP: the
enemy table holds current hit points and `INFO` renders them against the class
maximum. `HP_SCOUT` still wants a scout.

Reconfirmed again: **enemies hold fire on about half their turns.** Fourteen of
roughly thirty pinned turns produced no hit at all.

### Instrument trap: writing the shield pair low poisons it

`probe_original.restore()` writes BOTH words of the shield pair. After a sample
that wrote 150.0 there, every later write of a larger value came back as 150 --
three sweep rows read "absorbed 2350 of a 2350 hit" with the pool landing
exactly on 150 from three different starting values, which is a clamp and not a
hit. The earlier blocks, taken before any low write, show no such behaviour.

So a session that writes the shield pair must not write a small value into it
and then expect large ones to stick. This invalidated the absorption sweep and
is why the law above is unresolved rather than measured.

## Runs 4 and 5 (2026-08-24): the loss report, the scoring sheet, and RAY

### All four enemy classes read at 100%

`INFO` steps through every enemy in the quadrant and prints its hit points as a
percentage of the class maximum. One quadrant held all four:

    Mongol Scout        255   100%
    Mongol Battleship   355   100%
    Mongol Commander    695   100%
    Mongol Supply       150   (seen as 120 at 80%, run 3)

`trek.h` already carries 255, 355 and 695 and all three are confirmed. **The
plan's claim that `HP_SCOUT` was "still 100 and still unmeasured" was false** --
it was measured on 2026-08-21 and the note saying otherwise was carried forward
into the measurement plan and then into the open list twice. Third time this
class of error has cost a session; see [[negative-claims-about-egatrek]].

`HP_SUPPLY 120` is a spawn value, not the class maximum, which is **150**.

### RAY

The confirmation, in WEAPONS CONTROL: the death ray is experimental in nature
and has been highly prone to failures, are you sure you wish to continue (Y/N).

On success the dialog prints three beats -- preparing, firing, and it worked --
then waits on enter. **Every enemy in the quadrant died**: two ships gone from
the table, the Mongol counter 40 to 38, ALERT back to Green.

With no enemies present it refuses: `SCIENCE: Scanners show no enemy ships in
this quadrant.`

**The odds are still one sample.** Repeat sampling is blocked by something
worth knowing: a successful RAY clears the SECTOR MAP, and writing ships back
into the enemy table does not put them back on the map -- the two are separate
structures, and the game asks the map. Sampling RAY needs a fresh quadrant or a
restored save per shot, not a memory poke.

### Self-destruct, and it takes nothing with it

Three stages: a RED confirmation box ("this is a desperate measure", Y/N), then
`Enter self-destruct password:`, then `Hit ESC to abort` and a pause before it
fires.

**With four enemies in the quadrant, the nearest at range 1.41, it destroyed
NONE of them.** The Mongol counter did not move and the report printed
`Mongol ships destroyed per stardate: 0.00`. So `SELFDESTRUCT_FACTOR` looks
like another ancestor rule Anderson dropped. One sample, but an unambiguous
one.

### The Top Secret loss report -- the screen RAY's fatal outcome needs

A memo, and the frame the other loss endings reuse:

    Dept. of Space / EARTH HEADQUARTERS / Top Secret
    From: Commander, Earth Sector
    To:   Headquarters
    Date: <stardate>
    Re:   Loss of U.S.S. Lexington, RCB-92

    <one sentence naming how the ship was lost>

    Stardays in action:                  1.5
    Mongol ships destroyed per stardate:  0.00
    Score:                               -890

For self-destruct the sentence is that the ship was destroyed per order of the
captain this stardate, with loss of all aboard.

### The Detailed Evaluation, with every weight on screen

    ITEM                                      SCORE
        Penalty for loss of ship..........    -200
        Penalty for incomplete mission....    -300
      2 Mongols killed @ 10 each.........       20
      1 Commanders killed @ 20 each......       20
      0 Enemy bases destroyed @ 50 each..        0
   0.00 Kill/day ratio @ 500 per day.....        0
    430 Casualties on board Lexington....     -430
      0 Stars destroyed @ -5 each........        0
      0 Bases hit @ -200 each............        0
        TOTAL.............................    -890

Every weight matches `trek.h` -- 10, 20, 50, 500, -200, -300 -- and the
casualties are one point each with 430 being the whole complement, so
self-destruct kills all hands.

**But there IS a "Penalty for loss of ship" line, at -200, and this port
deleted it.** `trek.h` says "There is no ship-loss line on the original's
sheet", measured on 2026-08-20 from a sheet that totalled -730 with no kills.
Today's sheet prints the line and the arithmetic closes exactly:
-200 -300 +20 +20 -430 = -890.

Either the 2026-08-20 reading missed a line, or the penalty applies to
self-destruction and not to being shot down. **One sample of dying in combat
settles it** and it is cheap. Until then the constant stays out, because
putting it back on one screenshot would be making the same mistake in the
other direction.

Note the two kill lines are NOT exclusive: two ships died, one of them a
commander, and the sheet scored 2 x 10 AND 1 x 20.

### Shields: more data, and the confound that spoiled it

Fifteen pinned turns at six charge levels, with the shield pool written through
its CURRENT word only (writing the pair is what poisoned run 3):

    charge 2500   absorbed 212/212, 795/795, 786/1057
    charge 1800   absorbed 470/860, 457/898, 468/919
    charge 1200   absorbed 429/969, 183/889, 209/1004
    charge  800   absorbed 231/816, 184/886, 188/868
    charge  500   absorbed 158/918,  82/942, 665/764

The three 1800 rows are tight -- 470, 457, 468, all about 0.26 of the charge --
and the rest are not. **The confound is that TWO enemies were firing**, so each
"hit" is the sum of up to two hits resolved in sequence, and the first one
damages the shield SYSTEM before the second arrives. The law needs a quadrant
with exactly one enemy.

Two things did come out of it:

- **The shield SYSTEM takes damage when the pool absorbs a big hit** -- to 71%
  on an 795-unit hit that was otherwise fully absorbed, and to 0% twice. That
  is a mechanic separate from the pool draining, and this port has none of it.
- **System damage does not need penetration.** One turn damaged the shield
  system with `to_energy` at zero. So the trigger is not simply "something got
  through".

## Session 6 (2026-08-24): the survivor's sheet, and a rescue line we never knew about

### Two endings, two different memos

The Top Secret memo has at least two forms, and they differ in the header as
well as the body.

**Ship lost** (self-destruct, captured earlier):

    To:  Headquarters
    Re:  Loss of U.S.S. Lexington, RCB-92

**Ship survives** (QUIT):

    To:  Captain, U.S.S. Lexington, RCB-92
    Re:  Battle Results

    Captain: The results of your operations against the Mongol Empire have
    been evaluated. We have found the following:

Both then print the same three lines -- stardays in action, Mongol ships
destroyed per stardate, score.

`Q` itself asks inline in the COMMAND panel (`Quit (Y/N)?`), not in a dialog
box.

### The evaluation sheet's LINES vary by ending -- and there is a rescue line

The surviving sheet, after quitting immediately:

    ITEM                                      SCORE
      0 Rescues @ 200 each..............         0
        Penalty for incomplete mission..      -300
      0 Mongols killed @ 10 each........         0
      0 Commanders killed @ 20 each.....         0
      0 Enemy bases destroyed @ 50 each.         0
   0.00 Kill/day ratio @ 500 per day....         0
      0 Casualties on board Lexington...         0
      0 Stars destroyed @ -5 each.......         0
      0 Bases hit @ -200 each...........         0
        TOTAL...........................      -300

Two things this port does not have:

- **`Rescues @ 200 each`.** That is the scoring end of the distress-signal
  mechanic -- the manual's feature list says "Successful rescues increase
  score" and this is the weight. It does NOT appear on the self-destruct
  sheet, so the line set is per-ending.
- **No `Penalty for loss of ship` line here**, where the self-destruct sheet
  had one at -200. So that line is conditional on losing the ship, which
  supports it being real rather than a misreading. **Still not settled**: a
  COMBAT death sheet is the discriminator and it was lost three times to the
  capture problem below.

Quitting alive with nothing done scores exactly -300, the incomplete-mission
penalty and nothing else.

### Shields, with exactly one enemy at last

The condition runs 3 and 4 both lacked. Enemy pinned at (2,8) with 900 hit
points, our sector fixed at (4,4), systems written to 100 and the pools set
before every turn, shields raised:

    charge 2500   absorbed 464/464, 824/927, 189/554
    charge 2000   absorbed 743/1149, 769/1194, 743/1147
    charge 1500   absorbed 561/1206, 562/1206, 551/1178
    charge 1000   absorbed 242/739

**At a given charge the absorbed AMOUNT is repeatable** -- 742.6, 768.7, 742.6
at 2000, and 561.3, 561.6, 551.0 at 1500. That is the first repeatable shield
reading this project has produced.

The absorbed FRACTION rises with charge: 0.327 at 1000, 0.466 at 1500, 0.647 at
2000. Those three fit `fraction = charge / C` with C near 3100 to within a few
percent, and 2500 does not (0.890 observed against 0.806 predicted). **The
shape is settled and the constant is not**: shields absorb a share of the hit
that grows with their charge, and the rest reaches energy.

Whatever it is, it is NOT `amount - shields`.

### System damage reports casualties, and zeroes the system

    Transporter damaged. Now at 0%. There are 7 casualties reported in
    Engineering.
    Shields damaged. Now at 0%. There are 1 casualties reported in Engineering.
    EnergyConverter damaged. Now at 0%.

Three systems in ONE turn, all to zero, with per-system casualty counts. It
also confirms energy reaching 0 does NOT destroy the ship -- the Lexington sat
at 0.0 energy and kept playing.

### Save and restore is a working checkpoint

`SAVE` takes `<Enter>` for a default filename; on restart, `Restore a saved
game (Y/N)? Y` then `File name, <Enter> for default, <ESC> to abort`. Restoring
put the ship back in quadrant 8-7 with the same four battleships and the same
stardate. **That is the rig for sampling RAY**, which needs a fresh quadrant
per shot.

### The repair-time estimate is NOT a function of points remaining

Fresh sweep, one system, both columns:

    pct    0    1    2    3    4    5   40   41   42   43   44   45   46   47
    dock  2.1  2.1  2.0  2.0  2.0  2.0  1.3  1.3  1.2  1.2  1.2  1.2  1.2  1.1
    adrft 5.1  5.0  5.0  4.9  4.9  4.8  3.0  3.0  3.0  2.9  2.8  2.8  2.8  2.7

The undocked column fits `points / 19.5` truncated to a tenth for the first
eight rows, and then breaks: the 2.8/2.9 boundary and the 2.9/3.0 boundary are
ONE point apart while the 2.7/2.8 boundary is three points away. A single rate
cannot produce that. **Abandoned as cosmetic** -- it is a display estimate with
no gameplay effect, and it has now cost parts of two sessions.

### The capture problem, three times in one session

**Death is asynchronous to your input, and any loop that sends a key after the
action will destroy the screen you are trying to capture.** Three separate
combat deaths were lost this way -- the trailing keystrokes walked through the
loss report, the evaluation and the hall of fame into a new game before the
screenshot. Checking `live()` at the top of the loop is not enough, because the
ship dies in the middle of the volley you just committed to.

The technique that works: send the action, then send NOTHING and poll `live()`,
and screenshot the moment it goes false.

## Session 7 (2026-08-24): the -200 ship-loss penalty is REAL

A combat death, captured intact this time by sending the fatal volley and then
sending NOTHING while polling for the game to end. Jamie called it from the
screen before I did.

The memo, on a starfield rather than a plain ground:

    Re: Loss of U.S.S. Lexington, RCB-92
    U.S.S. Lexington lost in battle with Mongols this stardate, with all
    aboard.  Results of operations previous to loss follow:
      Stardays in action:  0.0
      Mongol ships destroyed per stardate: 0.00
      Score: -930

And its sheet:

    Penalty for loss of ship.........  -200
    Penalty for incomplete mission...  -300
    430 Casualties on board Lexington  -430
    TOTAL............................  -930

**Settled.** Nothing killed, no time elapsed, and the total is -930 rather than
the -730 this project recorded on 2026-08-20 and used to DELETE the constant.
The line is on the sheet, in its own row, above the casualties. The earlier
reading missed it.

Three sheets now, and the line set differs by ending:

    ending           ship-loss line   rescues line
    combat death     -200             absent
    self-destruct    -200             absent
    quit, alive      absent           present, 0 @ 200 each

So the two are alternatives, not independent rows. `trek.h` gets
`SCORE_SHIP_LOST` back, `ScoreSheet` gets `ship_lost_pts`, `test_trek.c`
asserts -930 on the case it used to assert -730, and `ui.c` prints whichever
row the ending calls for.

The combat-loss sentence -- "lost in battle with Mongols this stardate, with
all aboard" -- is the second of the eight endings captured verbatim in shape,
after self-destruction's "destroyed per order of captain".

### The apparatus goes unstable under long automated runs (2026-08-24)

Recorded because it ended the session and will end the next one the same way.

After a long block of scripted input -- the shield sweeps, the death loops, and
especially anything that keeps driving after the game has ended and restarted
-- the emulator stops responding to input correctly. The tell is a screenshot
crop that comes back BLANK where a dialog should be, while the API still
answers and memory still reads. Jamie spotted it twice from the screen before
any of my checks did.

Restarting dosbox clears it. Restoring the save puts the ship back exactly, so
the cost is small -- but it means **long unattended loops are the wrong shape
for this instrument.** Work in short blocks, restart between them, and check a
screenshot rather than trusting that the API answering means the guest is
healthy.

This is why `RAY`'s outcome odds are STILL one sample after three attempts.
They need many samples, each needing a restore, which is exactly the pattern
that destabilises the thing. The next attempt should restart the emulator per
sample rather than per block.

## The movement model, entire (2026-08-25)

Taken for the correction pass, because "movement is a straight line and objects
block it" was not enough to implement from: it says nothing about where the
ship ENDS UP, and nothing about what the clock is charged. Both turned out to
be measurable in one short session, and one of them contradicted a fitted
constant this port has been carrying.

Level 3 game, quadrant 7-4, warp 1.0 unless stated. Stars at sectors 1-6, 2-5,
2-6, 6-5, 7-6 -- read off the short range scan and used as the obstacles.

### The path: round half AWAY FROM ZERO, over max(|dy|,|dx|) steps

`n = max(|dy|, |dx|)`. Step `i` sits at the real position
`(y0 + dy*i/n, x0 + dx*i/n)`, and the cell tested is that position with each
coordinate ROUNDED -- Turbo Pascal's `Round`, which breaks a half away from
zero.

The rounding rule is not assumed; it was DISCRIMINATED. From 5-2 to 7-6 the
two candidate rules predict different outcomes:

    round half up    step 3 = (6.5, 5.0) -> 7-5 clear, step 4 -> 7-6 STAR
    round half down  step 3 = (6.5, 5.0) -> 6-5 STAR

The ship finished at 7-5 and the log read `Move blocked by object at 7-6`, so
it is half-up. Reproduced twice from the same start.

Three earlier observations fit the same model and were what suggested it: the
4-4 to 6-3 block at 5-4, the 8-6 westward block at 4-3, and RAY passing from
5-3 through a star at 4-4 on the way to 2-7 -- a different command walking the
same path.

### A blocked move is a PARTIAL move, not a refusal

This is the part that could not be guessed, and it decides the implementation.

    from 1-3, m18   -> ship ends 1-5, log "blocked by object at 1-6"
    from 5-2, m76   -> ship ends 7-5, log "blocked by object at 7-6"
    from 8-3, m47   -> ship ends 7-4, log "blocked by object at 6-5"

The ship travels the path and stops in the last clear cell. The message names
the OCCUPIED cell, which is one step further on than where the ship is.

**Quadrant changes behave the same way, and the ship stays in the quadrant it
started in.** From quad 7-4 sector 7-1, a move to quad 7-5 sector 7-1 walks
east along row 7, hits the star at 7-6, and leaves the ship at quad 7-4 sector
7-5. The departure path is checked in the quadrant being LEFT. A move blocked
on its first step goes nowhere at all: from 2-4, a move to quad 7-5 gave
`blocked by object at 2-5` with the stardate unmoved.

### Impulse time: distance / 24, on the TRUNCATED endpoint

Time for an in-quadrant move is the Euclidean distance actually travelled,
divided by 24 stardates -- and it is **warp-independent** (4 sectors cost
0.1667 at warp 1.0 and 0.1666 at warp 3.0).

Ten samples, every one landing on d/24:

    d 2.000  (1-3 -> 1-5, blocked)   0.0833
    d 5.000  (1-5 -> 5-2)            0.2084
    d 4.000  (7-5 -> 7-1)            0.1667
    d 4.000  (7-1 -> 7-5, blocked)   0.1667
    d 4.123  (7-5 -> 8-1)            0.1718
    d 2.000  (8-1 -> 8-3)            0.0833
    d 1.414  (8-3 -> 7-4, blocked)   0.0589
    d 2.828  (7-4 -> 5-2)            0.1179
    d 4.000  (7-5 -> 7-1, warp 3)    0.1666

**The endpoint is TRUNCATED, not rounded**, and that is a real distinction on
a blocked move whose stopping step has a fractional coordinate. Two samples
show it, and both were reproduced:

    5-2 -> blocked at 7-6, ship at 7-5.  Step 3's real position is (6.5, 5.0).
      charged 3.1608   trunc (6,5) -> sqrt(10) = 3.1623   MATCH
                       round (7,5) -> sqrt(13) = 3.6056
                       real  (6.5,5) -> sqrt(11.25) = 3.3541

    8-1 -> blocked at 2-5, ship at 3-5.  Step 5's real position is (3, 4.571).
      charged 5.8320   trunc (3,4) -> sqrt(34) = 5.8310   MATCH
                       round (3,5) -> sqrt(41) = 6.4031
                       real  (3,4.571) -> sqrt(37.75) = 6.1441

The second was designed as a discriminator -- three hypotheses, three distinct
predictions -- rather than fitted after the fact. So the ship is PLACED at the
rounded cell and BILLED for the truncated one. On a clear move the two are the
same, because it ends on its integer target.

### Warp time: 11 * distance_in_quadrants / warp squared

**The port's fitted constant is about 9.98 and it is wrong by 10%.** The
correct constant is 11, and it is exact.

The new sample is the clean one: a quadrant-change move blocked after four
sectors -- half a quadrant -- at warp 1.0 cost **5.5000** stardates. That is
`11 * 0.5 / 1`, and it also confirms that a blocked move bills for the distance
TRAVELLED and not the distance asked for.

Then the five warp-3 samples already recorded in run 1 were re-read against it,
and every one resolves to an exact lattice distance:

    0.8227  ->  5.3849 sectors  =  sqrt(29)
    0.9663  ->  6.3249 sectors  =  sqrt(40)
    1.2222  ->  8.0000 sectors  =  8
    2.5972  -> 16.9999 sectors  =  17
    2.7330  -> 17.8887 sectors  =  sqrt(320)

Five arbitrary fractions collapsing onto integer-difference distances is not
something a wrong constant does. With the warp-2 reading (4 quadrants, 11.0)
that is **seven samples across four warp factors**, against the two that the
9.98 was fitted from.

### What this leaves open

- Whether the destination quadrant's contents can block an ARRIVING ship. The
  original does not generate a quadrant until entry, so probably not, but it
  was not tested.
- The warp ENERGY model is untouched by this and is still fitted from two
  points.
- The 0.2 energy charged by the move that went nowhere.

### The corrected time law also fixes the warp ENERGY fit

Not looked for, and worth more than the thing it was found under. All three
warp-energy readings were computed as `spent + 400 * elapsed`, with the
elapsed time read off the Date display -- which shows one decimal. Recomputing
each against the measured law instead of the displayed figure:

| reading | elapsed used | cost then | true elapsed | cost now | model | error |
|---|---|---|---|---|---|---|
| warp 5, 1 quad | 0.5 | 194.3 | 0.4400 | 170.3 | 171.9 | +0.9% |
| warp 8, 1 quad | 0.2 | 709.7 | 0.1719 | 698.5 | 704.0 | +0.8% |
| warp 8, 2 quad | 0.4 | 1174.5 | 0.3438 | 1152.0 | 1152.0 | 0.0% |

The energy model was called "fitted from two points, the warp-5 point the
weakest of the three at -12%". It is not: it was right, and the TIME it was
being judged against was 10% short. Nothing in `warp_energy()` changed.

**And it settles a test this port had asserting the wrong thing.** The warp-5
line reads `+5.7 = 400 * 0.5 - cost`: main energy went UP by 5.7 over an
interval containing the jump. So the original does NOT make a net energy loss
at warp 5, and `core/test/test_trek.c` asserted that it must. Break-even sits
just above warp 5 -- cost goes as warp^3 and the refill as 1/warp^2 -- so the
manual's "faster than you can regenerate it" starts at cruising speed. Below
it, energy is cheap and TIME is the price: one quadrant at warp 1.0 costs 11
stardates out of a 30-stardate mission.

---

## Everything the damage report knows, read out of the binary (2026-08-26)

Pointed at `fn 0x020DCE`, seven `Random()` sites, to close
`SYSTEM_DAMAGE_THRESHOLD`. There was no threshold to close. The routine and
its two callers gave the whole of combat system damage, two mechanics this
port does not have, and the value of `[0x1DF0]`, which three earlier sessions
left open.

Segment base **0x1BD60**, solved on thirteen `mov di, imm16` and scoring
twelve disjoint Pascal strings:

    DAMAGE REPORT:\n        %. There are            on F Deck.
     damaged. Now at         casualties reported     in Engineering.
     failing. Now at        %.                       on K Deck.
                                                     by Weapons Officer.
                                                     on Q Deck.
                                                     on T and U Decks.
                                                     on R Deck.

### The tool work that made the rest readable

**The Turbo Pascal real operators, identified from their stubs** rather than
guessed. Every one of them is a five-byte thunk in segment 0x2692, and each
gives itself away in its first instruction:

| offset | file | what it is | the tell |
|---|---|---|---|
| 0x0C93 | 0x2C3B3 | real ADD | falls into 0x2C0AF |
| 0x0C99 | 0x2C3B9 | real SUBTRACT | `xor di, 0x8000` first, then the same |
| 0x0CA5 | 0x2C3C5 | real MULTIPLY | |
| 0x0CAB | 0x2C3CB | real DIVIDE | `or cl,cl / je` -- a zero-divisor check |
| 0x0CB5 | 0x2C3D5 | real COMPARE | sets flags, callers use `ja`/`jae` |
| 0x0CB9 | 0x2C3D9 | longint -> real | |
| 0x0CC5 | 0x2C3E5 | real -> longint, ROUND | `mov ch,1`, then 0x2C357 |

The SUBTRACT stub is the one that pays twice: it flips **bit 15 of `di`**,
which is bit 7 of the sixth byte, and that is the sign bit. So it also
CONFIRMS the `cx:si:di` byte order the constant decoding depends on.

Two small helpers, both worth naming because they look like something else:

  * **`fn 0x01C015` is `max(n - 1, 0)`** -- a Pascal `for i := 1 to n-1 do
    inc(c)` the compiler did not fold. Every `f(Random(k) + 1)` in the damage
    routine is just `Random(k)`.
  * **`fn 0x01F4B8` is `Sign(x)`** -- returns 1.0, 0.0 or -1.0.

And **DS maps to file offset by adding 0x2D820**, from "EnergyConverter" at
DS:0x1188 landing on file 0x2E9A8. That converts every data address in the
back catalogue into something readable without an emulator.

### `[0x1DF0]` IS THE COMMAND LEVEL PLUS FOUR. Settled.

    0x015050  if ([0x1DF0] <= 0) ask again
    0x015057  if ([0x1DF0] >= 6) ask again
    0x015061  [0x1DF0] = [0x1DF0] + 4

The setup prompt takes 1..5 and adds four. Fourteen lines later it indexes a
rank-name table at DS:0x102E by `(V - 4) * 14`, and that table reads

    Lt. Commander  Commander  Captain  Commodore  Admiral

so V = 5 is level 1. Two independent confirmations in one routine.

Everything that was pending on V falls out:

  * `11 - V` StarBases at 0x0053C5 -- **six at level 1, two at level 5**.
    `STARBASES_AT_LEVEL` moves from FITTED to BINARY.
  * `cmp V, 9` at 0x005482 is a level-5 test.
  * `V > 7` gating the spy is "level 4 and up".
  * `V >= 6` twice below is "level 2 and up".

The note in trek.h said "one emulator run settles V and both constants with
it". No emulator run was needed. The prompt that READS the value was thirty
lines from a `mov di, 0x1DF0` that a reference scan finds in a second.

**It does NOT touch `CRYSTAL_V`.** That one is `[0x1DCC]`, a different word --
and a reference scan shows `[0x1DCC]` is written by thirty-eight sites all
over the binary as a shared `for` counter, including inside `fn 0x01C015`.
Nothing in the USE path sets it, so **the crystal's multiplier is whatever the
last loop left behind**. That is a bug in the original, not a constant, and it
is why the measurement could not be reconciled with the level. planet.h's
caveat stands and gets sharper: reproducing the one reading is the right call,
and a second reading would probably disagree with it.

### System damage is decided ONCE A TURN, on the turn's total

`fn 0x0213AD`, called from the main loop at 0x005993, inside this:

    n = Round([0x1DC8] / 350.0) + 1        ; 0x00595E
    for r = 1 to n: CombatDamageGate()
    WearAndTear()                          ; fn 0x0213F3

and the gate itself:

    if ([0x26DE] != 0)              goto roll     ; enemies fired this turn
    if ([0x1DF0] < 6)               return        ; level < 2
    if ([0x1DC6] > 700)             goto roll     ; absorbed this turn
    if (Random([0x1DC8]) <= 175)    return        ; raw hits this turn
  roll:
    if (Random(3) == 0)             return
    DamageReport(2)

`[0x1DC8]` and `[0x1DC6]` are the turn's raw hits and the turn's absorbed,
both zeroed on entry to the fire routine at 0x0165B5. `[0x26DE]` is cleared at
the top of every turn at 0x0058F5 and set at 0x01671B by the fire routine
whenever enemies engaged -- **the same routine that writes the other two.** So
the level and threshold branches below it can never be reached with anything
in `[0x1DC8]` to test, and what actually happens is:

> **If enemies fired at you, roll `Round(hits/350) + 1` times, and each roll
> damages one system two times in three.**

860 units across three hits -- the run-3 measurement -- is three rounds. Two
systems went that turn. The port's `through >= SYSTEM_DAMAGE_THRESHOLD` was
invented, its "about three in five" was the shadow of this loop, and its
"a second system, rarely" was the shadow of the third round.

### How hard, and it is in HIT UNITS

From 0x020E37..0x020FA7. `rnd` is Turbo Pascal's argument-less `Random`, a
real in [0,1):

    shields up:    sys -= Round(hits * (1.25 - charge/2500) / (2 + 3*rnd))
    shields down:  sys -= Round(hits * 0.5                  / (2 + 3*rnd))
    sys -= Random(5)
    if (sys > 90) sys -= 10 + Random(10)
    if (sys < 0)  sys = 0

3.0, 2.0, 2500.0, 1.25 and 0.5 all decode as exact round numbers, which is the
check on the decoding.

**The subtraction is in hit units against a 0..100 scale.** That is the whole
explanation of "zero eight times in eleven" -- there is no annihilation case,
only a number that usually clears 100. At 860 units and shields down the
amount runs 86..215 and clears 100 for about three quarters of the divisor's
range: 8 in 11 to the accuracy eleven observations support.

And the first line says something the port would never have guessed:
**raised but empty shields are the worst place to be.** The factor is 0.25 at
a full 2500 and 1.25 at zero charge -- two and a half times worse than
dropping them. There is now a test for it, because it is exactly the kind of
sign a later edit would "fix".

Casualties, at 0x021006, are `Round(Sign(hits - 500)) * Random(10)`: a turn
whose hits never passed 500 costs nobody, and one that did costs 0..9. The
total accumulates at `[0x1DDE]`, which is what the Top Secret report prints.

### Which systems each kind can touch

`DamageReport` takes one argument, and the three call sites pass 1, 2 and 3:

| kind | system chosen | reads |
|---|---|---|
| 1 | `Random(6)` -- Converter..Warp Engines | "X failing. Now at N%." |
| 2 | `Random(11)` -- everything but the Shuttlecraft | "X damaged. Now at N%." |
| 3 | index 0, the EnergyConverter | "X failing. Now at N%." |

Combat is kind 2, so **the Shuttlecraft can never be damaged by enemy fire**,
and the "on R Deck." string that names it is unreachable. Kinds 1 and 3 are
wear and tear, below.

If the chosen system is already at 0%, the routine returns without a word.

### The shield SYSTEM wears from what the POOL stops

`fn 0x016844` at 0x0171CC, once per turn on the turn's total absorbed:

    if (absorbed > 800 && shield_sys > 0)
        shield_sys -= Round((absorbed - 700) / 10.0)      ; floored at 0

800 absorbed costs 10 points, 1500 costs 80. `SHIELD_SYS_HIT_MIN 600` was
fitted from one reading and wrecked the system outright; it is now three
BINARY constants and a graded reduction.

### ~~NOT BUILT:~~ BUILT 2026-08-29 -- wear and tear

`fn 0x0213F3`, the second call from the turn loop, and a mechanic this port
does not have at all. `[0x1D36]` is the stardate of the last wear event:

    if (stardate <= 3503.0)   return          ; nothing breaks in three days
    if (level + 4 < 6)        return          ; nothing breaks at level 1
    e = Round((level + 3) * (stardate - last_event))

    if (Random(100) > 98 - e):
        if (average of all twelve systems > 95):
            DamageReport(1)                   ; a random one of the first six
            last_event = stardate
    else if (Random(100) > 97 - e):
        DamageReport(3)                       ; the EnergyConverter
        last_event = stardate

So roughly `(1 + e)/100` per turn, rising with elapsed time since the last
one and with the command level. Note what the 95% test does: **the random
breakdown only fires on a ship that is in near-perfect repair.** Let something
else be broken and the game stops inventing faults, and drops through to the
EnergyConverter check instead.

Not built here because it needs `last_event` in the save record, which is a
version bump. Fully specified when it is wanted.

### ~~NOT BUILT:~~ BUILT 2026-08-29 -- a damaged Computer eats the star chart

The tail of `fn 0x020DCE`, at 0x02134D, and only when the damaged system is
index 9. It walks all sixty-four cells of an 8x8 word array at DS:0x2372 --
which is NOT the galaxy at DS:0x2560, it is the RECORDED chart -- and for
each one:

    if (computer% < 30)                        erase it
    else if (Random(10) < 5 && computer% < 70) erase it
    else                                       leave it

So above 70% nothing is lost, below 30% the chart goes entirely, and in
between **each cell is an independent coin flip** -- you get a chart with
holes in it, not a blank one. That is a better feature than anything this port
would have invented, and it wants the chart to be a separate array from the
galaxy, which it already is.

---

## The torpedo, and it does not roll for anything (2026-08-26)

Went looking for `TORP_BASE`, `TORP_SPREAD` and `TORP_MISS_PCT_PER_UNIT`,
three FITTED constants in a mechanic the player uses every combat turn. None
of the three exists. **THE ORIGINAL RAY-MARCHES**, and the accuracy curve and
the damage spread the port had fitted are both shadows of that march.

Segment base **0x9310** for the whole weapons unit, 15 of 20 immediates
resolving to disjoint strings. Two routines: `fn 0x00B5FD` is ENERGY TORPEDO
CONTROL and `fn 0x00B1CE` is what happens when one arrives.

### One correction to the routine map first

`0x09563` was filed as "torpedo firing". It is not; it is the **PLASMA BOLT
item** -- "Sector to fire at:", "Tracking...", `Random(3) == 0` fails to
detonate, otherwise `fn 0x01EB48(y, x, 1000)` damages the quadrant and it
prints "N Mongol(s) destroyed." then hits us back with " unit hit from plasma
bolt.". That matches the "639 unit hit from plasma bolt" seen on screen and
recorded above under "Also seen". The real torpedo code is at 0x0B5FD.

### The flight

    y = ship_y;  x = ship_x                       (1-based, as reals)
    if shields up:
        y += (charge/25000) * Random
        x += (charge/25000) * Random
    dy = target_y - ship_y;  dx = target_x - ship_x
    step = 1/|dy|, or 1/|dx| when |dx| > |dy|
    loop:
        y += step*dy + Random*0.1
        x += step*dx + Random*0.1
        if y or x leaves 0.5 .. 8.5   ->  "Clean miss, sir."
        d = Sqrt(Frac(x)^2 + Frac(y)^2)
        case sector[Round(y)*10 + Round(x)] of
            '.'      keep flying
            ' '      BLACK HOLE, below
            '*'      fn 0x00A8C8, the star
            'P'      "Torpedo hit a planet."
            'B'      "Are you mad? You damaged a base!"
            'A'      "Careful! That ship is one of ours!"
            'R'      "No damage reported."
            C K S    fn 0x00B1CE, the hit

25000.0, 0.1, 0.5, 8.5 and the 0.3/0.6 below all decode as exact round
numbers. 0.1 is `7D CD CC CC CC 4C`, the same mantissa as the shield law's
0.8 with a different exponent, which is a check on both.

**BOTH WOBBLES ARE ONE-SIDED.** `fn 0x02C886` is Turbo Pascal's argument-less
`Random` and it ends `and dh, 0x7f` -- the sign bit is cleared, so it is
[0, 1) and never negative. Every torpedo therefore drifts toward higher y and
x, and the further it flies the further it drifts. That is not an
interpretation of a distribution; it is what the code does.

The consequence is that **accuracy is an ANGLE, not a distance**. A shot along
the 45-degree diagonal drifts along its own line of travel and barely errs; the
same range off the diagonal drifts sideways and misses. The port's old model
had accuracy fall with range alone and could not tell those two apart.

### The hit

    if (d >= 0.6)          nothing at all -- it passed through the cell
    base = (level + 4) * 15 + 250
    if (Random(50) == 0)   "EnTorp fails to detonate."
    if (d < 0.3)           hp -= base                       a direct hit
    else                   hp -= Round((1 - d) * base)      a graze

**This settles the nineteen-shot measurement of 2026-08-21.** At level 3,
`base` is 7*15 + 250 = **355 exactly**. The figure recorded as "a CAP, not the
damage" was the damage all along, and it binds "every time up close" because
a short flight leaves no room to wander.

Reproducing the measurement in the port's own implementation, 2000 shots a row:

| geometry | direct | damage | measured |
|---|---|---|---|
| range 1.41 and 2.24 | 100% | 355 always | 6/6 at 355 |
| range 5.00, straight | 20% | 157..248, or 355 | 210 355 355 247 209 296 |
| range 5.00, 3-4 | 60% | 194..248, or 355 | |
| range 7.62, off-diagonal | 1% | 141..248, or 355; 6% no damage | 176 209 229 247 + 3 misses |

### THE HOLE, and the one reading that does not fit

A direct hit is `base`. A graze is `(1 - d) * base` with `d >= 0.3`, so it can
never exceed `0.7 * base` = 248.5, which rounds to **248**. There is nothing
in between:

    ... 245  247  248        355 ...
                  ^^^^^^^^^^^^^^ nothing here, ever

A histogram of 20,000 shots at range 5 shows exactly that: a run up to 248 and
then 355. **No level produces a value in 249..354.** One of the nineteen
measured shots read **296**, which sits in the hole. Recorded as an open
discrepancy rather than smoothed over. The likeliest explanations are a volley
of more than one torpedo (the original asks "Number to fire:" first) or a
laser in the same turn; the cheapest check is one emulator run firing many
torpedoes at range 5 and looking for anything between 249 and 354.

The long-range miss rate is the other soft spot. The model gives about 6% "no
damage" at range 7.62 with shields down and **28% with them up**, against 3 of
7 measured. Whether the shields were raised for that block was not recorded,
and neither was the firing angle, which the model says matters more than the
range. Both belong in the next measurement.

### The black hole, exactly

`fn 0x00AFE7`, four instructions of substance:

    if (d < 0.3)  "Torpedo sucked into black hole."   and it stops
    else          "Torpedo deflected by black hole."  and the caller
                  SWAPS dy AND dx

A 90-degree mirror about the diagonal. **BUILT 2026-08-28**, and it reuses
`TORP_DIRECT_Q` -- the 0.3 here is the same threshold, and the same constant,
as a direct hit's.

### ~~NOT BUILT:~~ BUILT 2026-08-29 -- what a star really does

`fn 0x00A8C8`, which the march calls on '*', is three outcomes:

    Random(100) > 95        4.0%   the star goes SUPERNOVA: "Star at Y-X goes
                                   supernova!", "Lexington blown to quad N.",
                                   "N Mongols destroyed.", and a hit on us
                                   that can read "Lexington destroyed."
    then Random(100) < 40  38.4%   "Torpedo absorbed by star."
    otherwise              57.6%   the star is DESTROYED -- the cell becomes
                                   'N' and [0x1DF6] increments

`[0x1DF6]` is where the **"Stars destroyed @ -5"** scoring line comes from.
~~The port answers TORP_ABSORBED to all three.~~ **All three are built**: the
supernova on 2026-08-27 and SEC_NOVA -- the 57.6% case -- on 2026-08-29, with
the per-quadrant count that makes a destroyed star stay destroyed.

### ALL FOUR ENEMY STRENGTHS, and the level dependence

Falls straight out of the same `[0x1DF0]`. Six sites, four formulas:

| class | site | formula | lvl 1 | lvl 3 | lvl 5 |
|---|---|---|---|---|---|
| Commander | 0x016173 | `(level+4)*35 + 450` | 625 | **695** | 765 |
| Battleship | 0x016119 | `(level+4)*15 + 250` | **325** | **355** | 385 |
| Scout | 0x0161C7, 0x01627C | `(level+4)*15 + 150` | 225 | **255** | 285 |
| Supply | 0x016232, 0x0162B6 | `(level+4)*10 + 50` | 100 | **120** | 140 |

**Every bold figure is a separate sighting taken on a different day, and all
six land exactly.** The fitted `310 + 15*level` for a battleship was right;
the level-5 game that "would settle it" is not needed. Scout and supply each
appear twice, which is the galaxy generator and the reinforcement path writing
the same expression.

And the battleship line is the SAME EXPRESSION as the torpedo's `base`. One
torpedo kills one standard Mongol at every command level, by construction. It
was never a coincidence and never a cap.

---

## The event scheduler exists, and it is the shape we guessed (2026-08-26)

This was the biggest open question in the port and the only one about the
SHAPE of the core rather than a number. Three decoded events in a row had
turned out not to be scheduled -- the spy is a flat `Random(150)` per turn,
the distress signal is location-triggered, wear and tear is an elapsed-time
roll -- and NOTES.md had recorded the worry plainly: "the port may have the
right behaviour on top of a structure the original does not have."

**It does not. EGA Trek schedules events, and it does it exactly the way this
port does.**

Eight Turbo Pascal reals in a fixed array at **DS:0x1D78**, one slot per event
type, each an absolute stardate, each tested `if (stardate > deadline)` once a
turn. No queue, no list, no allocation. The save and restore routines at
0x0078xx and 0x0082xx read and write all eight, so the deadlines are game
state -- which is also how the array was found: every slot is six bytes apart
and every one is compared against `[0x1D42]`.

    DS:0x1D78   the hail response            fn 0x02066B
    DS:0x1D7E   REINFORCEMENTS               fn 0x015A4C
    DS:0x1D84   the boarding party           fn 0x015D6E
    DS:0x1D8A   a Union ship's distress      fn 0x0158EC
    DS:0x1D90   a StarBase comes under attack   fn 0x01F9D5
    DS:0x1D96   ... and falls                   fn 0x01F9D5
    DS:0x1D9C   the supernova                galaxy generation sets it;
                                             both nova routines clear it
    DS:0x1DA2   the evacuation deadline      fn 0x0151D0

**9999.0 is never** -- written literally as `[slot] = 9999.0` -- which is what
`SCHED_NEVER` already meant.

### But the DEVIATE is uniform, not exponential

Every reschedule in the binary is the same expression:

    slot = stardate + base + spread * Random

with Turbo Pascal's argument-less `Random`, flat on [0,1). The port used the
ancestor's `expran()` -- `-mean * ln(u)` -- which has a tail reaching 4.16x
its mean. That is a visibly different game: an exponential schedule clusters
events and then goes quiet for a long stretch, a uniform one does not.

`trek_expran` and its 32-entry -ln table are gone. The two slots that are
genuinely the original's now carry read constants:

    first attack   stardate + 2 + Random(4)    0x0054F4  -- the INTEGER
                                                            Random, so whole
                                                            stardates
    next attack    stardate + 2 + Random*4     0x01FA5A
    base falls     stardate + 2 + Random*2     0x01FB75

That third line is the number the COMMUNICATIONS panel prints. "The StarBase
in 6-6 reports that it is under attack. They can last until 3517.8" is
`stardate + 2..4`, which is what was captured twice on screen. The port had
`10 + Random(30)` tenths from the ancestor -- 1.0 to 3.9 stardates -- so it
was a stardate short at the bottom.

### And two of the port's four slots are not events at all

**The tractor beam has no slot.** It lives inside MOVE, `fn 0x0C609`, which
makes it action-triggered like the distress signal.

**The death pod has no slot either.** `fn 0x1DD4F` does not reference any of
the eight addresses; the only stardate comparison in it is against an
immediate, which is the mission-elapsed test in the scoring.

~~Both are kept in the port for now... their intervals are the port's own
invention and are tagged PROVISIONAL~~ -- **SUPERSEDED. BOTH TRIGGERS WERE
READ.** The tractor is `TRACTOR_OF_N` 10 / `TRACTOR_ABOVE` 7 and the pod is
`POD_FIRE_OF_N` 33, one in 20 in quadrant columns 7-8, capped at five ships --
every one of them BINARY in `core/trek.h`. Nothing in this port is
PROVISIONAL any more; `make tiers` reports 0.

~~**Four slots this core does not have at all:** the hail response, the
boarding party, a Union ship's distress call, and the supernova.~~ **ALL FOUR
ARE BUILT.** The distress call and the supernova came earlier; the hail's
delayed reply (SCHED_HAIL) and reinforcements (SCHED_REINFORCE) went in on
2026-08-29, taking this core to five schedule slots. The boarding party is a
per-turn roll here rather than a slot, which is a shape difference and not a
missing mechanic.

### Reinforcements, in full, and a surprise

`fn 0x015A4C`, segment base 0x150C0:

    if (stardate <= [0x1D7E]) return
    find a quadrant with NO enemies
    if there is one:
        add (Random(4) + 2) * 100 to its galaxy word     -- 2 to 5 ships
        "COMMUNICATIONS: Mongol reinforcements are reported in quadrant N-1."
    [0x1D7E] = stardate + 5 + Random*4                   -- rescheduled either way

**Reinforcements only happen at command level 5.** The setup code at 0x005830
reads `if ([0x1DF0] < 9) [0x1D7E] = 9999.0` -- level+4 below nine, so below
level five, and the slot is set to never for the whole game. At level 5 the
first check is scheduled for `3512 + Random*5`.

**And they always arrive in COLUMN 1.** The scan walks `0x2562 + 16*i` for
i = 1..8, which is column one of the galaxy array; the write goes to the same
address; `[0x1E08]` -- the column -- is set to the literal 1; and the message
itself ends in a separate string that reads `-1.`, so the text can only ever
say "quadrant N-1". Four independent places agree. Whether Anderson meant it
is not knowable from here, but it is unambiguously what the program does.

---

## The lasers: heat is a mechanic, and the measurement was blind to it (2026-08-26)

`fn 0x09CC1`, segment base 0x9310. It owns "Amount to fire at ", "Laser
efficiency reduced by damage.", "Lasers overheat. Now running at N%
efficiency." and the target names. Went in for `HEAT_PER_UNIT`, which trek.h
called "the weakest number in this file"; it was, and it was wrong, and it
turned out not to be a gauge scale at all.

### The damage law, and LASER_RANGE_ZERO confirmed

At 0x009F6C:

    r      = Sqrt(((ey-sy)/8)^2 + ((ex-sx)/8)^2)          -- distance/8
    damage = Round(amount * (1.5 - r) * 6.67 * lasers% / 1000)

8.0, 1.5 and 1000.0 decode exact. The fourth decodes **6.6700**, which is a
typed decimal and not 20/3. Fold them and `(1.5 - d/8) * 6.67 * pct/1000` is
`(1 - d/12) * pct/100` times 1.0005.

**So the linear falloff to zero at TWELVE sectors is the original's, exactly**,
and `LASER_RANGE_ZERO 12` moves from FITTED to BINARY. This port is five
hundredths of a percent light because Anderson's constant is not quite 20/3 --
under one unit on any shot, which is why all five measured readings reproduced
without anyone noticing.

### Heat: three constants wrong, and a mechanic missing

    heat += amount div 15          0x009EEF   -- an INTEGER divide
    if (heat > 120) heat = 120     0x009EFF

`HEAT_PER_UNIT` was 18, fitted from one eyeballed bar position. `LASER_HEAT_CAP`
was 100, and that was MEASURED -- but 100 was a floor on the observation, not
the cap: nothing in that run fired enough in a turn to reach 120.

Then, straight after the damage is computed, at 0x00A20D:

    if (heat > 90) {
        "Lasers overheat. Now running at N% efficiency."
        lasers% -= damage div 120 + Random(5)          -- floored at 0
    }

**Heat does not weaken the shot. It DAMAGES THE LASERS SYSTEM**, which weakens
every shot after it.

### Why run 2 concluded the opposite, which is the useful part

MEASURED.md has recorded since 2026-08-24: *"No value of it changes the damage.
A fixed 400-unit volley at a fixed distance, with the word written to 0, 20,
40, 60, 80, 100, 120, 140, 160, 200 and 300 in turn, dealt 263 every single
time."* Every one of those readings is correct, and the conclusion drawn from
them is wrong. Two reasons, both in the rig:

  1. **The penalty lands downstream of the number that was read.** The overheat
     branch runs AFTER the damage is computed, so the shot you are measuring is
     always at full strength. The cost falls on the next shot.
  2. **The rig repaired all twelve systems on both sides of every turn** -- a
     fix adopted for an unrelated life-support problem, recorded in this same
     file. That erased the penalty before it could be seen.

So the measurement was not wrong about what it saw. It was blind to where the
effect lands, and the instrument was actively undoing it. **A test can be
biased, not just the code** -- and this one was biased by a change made for a
good reason somewhere else.

### And two cooldowns, which explain the other observation

    heat -= 20                     once per command, 0x0059BC, in the main loop
    heat -= Round(elapsed * 360)   per stardate, 0x0201B3, with the repair

Together they are why heat looked like it "cleared on leaving the quadrant":
a warp jump elapses far more than enough time. A 300-unit volley adds 20 and
the turn sheds 20, so sustained fire breaks even around 300 units a turn and
climbs above it -- which is exactly the level at which the 90 threshold starts
to bite.

The port now has all five constants, the overheat penalty, both cooldowns, and
a `trek_turn_end()` hook for the per-command one, because a core whose clock
only moves on movement has nowhere else to put it.

### A note on the size this cost

The heat mechanic took the C128 image to 158 bytes free. `trek_score_sheet` --
1,332 bytes, reached only from the evaluation screen, which is already an
overlay -- moved into that overlay with a one-line annotation, and the build
is back to 1,490 bytes free with the eval overlay at 3,058 of 3,840. Worth
recording as a pattern: **core code called from exactly one overlay belongs in
that overlay**, and `OVL_CODE` is already a no-op off-target so core/ may use
it without learning anything about the platform.

---

## The enemy count, and a fit that was a coincidence of its own sample (2026-08-26)

`fn 0x04FD1` is not "galaxy generation" -- it is the whole main program, with
generation as its first few hundred bytes and the turn loop at 0x0058xx. The
enemy total is computed at 0x005181:

    total = Random(10) + ((level + 1) * 8 * (100 - Random(10))) div 100

`[0x1DF0] - 3` is `level + 1`, `shl 3` is the times eight, and 0x279/0x294 are
the runtime's 32-bit multiply and divide.

**Plus THREE for the StarBase that starts the game under attack.** At 0x0054D0,
inside the StarBase loop: base number one always, and EVERY base at level five
(`[0x1DCA] == 1 || [0x1DF0] == 9`), and only when that base's quadrant has no
enemies already -- then 300 goes into the galaxy word and 3 onto the total.
That was the term nineteen readings could not see, because it is added after
the count and looks like part of it.

### The fitted model reproduced every reading and was still wrong

`10 + 8*level + rand(0..8)` was adopted on 2026-08-24 with this reasoning,
recorded above: *"Level 3's ten samples span exactly 34..42, both endpoints...
This REPLACES the earlier fit of `level*10 + rand(0..12)`, which the same data
also satisfies but which predicts level 3 could roll 30..33. Ten samples never
did, and missing four of thirteen values ten times running is about one in
forty."*

The real range at level 3 is **32..44**. Ten draws landing on 34..42 inside it
is entirely ordinary -- and the argument that rejected the previous fit was the
same argument, applied to a band that was also too narrow. **A span that ten
samples exactly fill is evidence about the samples, not about the band.**

Three constants (`ENEMY_BASE`, `ENEMY_PER_LEVEL`, `ENEMY_SPREAD`) came out of
that fit. One of them survives.

| level | binary can produce | measured |
|---|---|---|
| 1 | 14..28 | 18, 21 |
| 2 | 21..36 | 30, 32 |
| 3 | 32..44 | 34 37 37 38 38 38 40 42 42 42 |
| 4 | 36..52 | 42, 47 |
| 5 | 43..63 | 53, 55 |

All nineteen fit, and the test asserts that each one is REACHABLE rather than
that the band happens to contain it.

The `(100 - Random(10))/100` term is a nought-to-nine percent shave, so the
count is biased DOWNWARD from `(level+1)*8` rather than spread evenly about
it. No number of readings could have shown that shape.

### And they arrive in clumps

The placement loop at 0x0051CD picks a quadrant, **skips it if it already has
enemies**, and drops `Random(4) + 1` in, clamped to what is left of the total.
One visit per quadrant. So a galaxy holds a dozen or so busy quadrants and
fifty empty ones, not a thin even scatter -- the port had been adding them one
at a time with a cap of four, which is a different distribution and a
different game to fly through.

### BLACK HOLE PLACEMENT, at last

Not in generation at all. It is in the QUADRANT FILL, `fn 0x0160xx..0x016562`,
which is also where the four enemy-strength formulas live. At 0x016525:

    if (Random(64) < 16)  put a black hole in a free sector

**One quadrant in four gets exactly one black hole**, and the sector is chosen
by the same free-sector helper (fn 0x01857E) everything else uses. **BUILT
2026-08-28** -- the placement, the MOVE interaction (1 in 5 fatal, otherwise
thrown across the galaxy), the torpedo interaction (swallowed under 0.3,
otherwise dy and dx SWAPPED) and the DEATH RAY's roll 3, which makes them.

### Stars, and why a destroyed one stays destroyed

The same routine draws stars at 0x016490, and just before it:

    if (novas[quadrant] >= k)  draw 'N'      -- a star already destroyed
    else                       draw '*'

`DS:0x24E9` is a per-quadrant count of destroyed stars, incremented by the
torpedo-into-a-star path at 0x00AD3E. So novas persist for the rest of the
game, and the quadrant fill redraws them every time you come back.

---

## The score sheet, and what actually gated the kill rate (2026-08-26)

`fn 0x1DD4F`, segment base 0x1BD60, 3,058 bytes. It owns the whole Top Secret
report -- every loss message ("destroyed by Vandal death pod", "pulled into
black hole & destroyed", "The cowardly captain... surrendered his ship"), the
Detailed Evaluation and the promotion line.

### The gate, which has been open since 2026-08-21

trek.h has carried this since the score sheet was first built: *"The kill/day
term is deliberately absent -- it printed 0.00 against two kills in 1.2
stardates, so something gates it that we do not understand, and implementing
a guess would be worse than leaving it out."*

At 0x01E270:

    stardays in action = stardate - 3500.0
    if (stardate < 3503.0)  rate = 0
    else                    rate = (total - remaining) / (stardate - 3500.0)
    score += Round(500.0 * rate)

**The gate is three stardates on the clock, and nothing else.** 3501.2 is under
3503, so the sheet printed 0.00; the unfinished mission it was attributed to
had nothing to do with it. 3500.0, 3503.0 and 500.0 all decode exact.

Two things the port had wrong follow:

  1. **It is not gated on finishing.** This port paired the rate term with the
     -300 incomplete penalty as "two halves of one condition", so a captain
     who ran out of time scored nothing for the ships he killed. The original
     pays him. Only the -300 depends on enemies remaining.
  2. **There is no five-stardate floor.** The port carried the ancestor's
     clamp and applied it always, which paid out for a win at 0.1 stardates.
     The original pays nothing at all below 3.0 and divides by the real
     elapsed time above it.

`SCORE_PER_KILL_DAY 500` was FITTED and is confirmed exactly.

### The rest of the sheet, and one rule nobody had

    if (shields [0x1D60] > 0.0)   score += rescues * 200
    else                          score -= 200
    score -= casualties [0x1DDE]
    score -= stars_destroyed [0x1DF6] * 5
    score -= bases_hit [0x1DF8] * 200

**The ship-survived test is the SHIELD REAL being above zero** -- the game
zeroes it when the ship dies (0x00A6FF, 0x016FA3) and reuses it as the flag.
And the rescue credit is in the *same branch*: **lose the ship and the people
you rescued earn you nothing.** The port credited them unconditionally.

`[0x1DF6]` is the stars-destroyed counter the torpedo-into-a-star path
increments, and `[0x1DF8]` counts OUR bases lost to any cause -- including a
torpedo of our own, at 0x00B10C, which is where "Are you mad? You damaged a
base!" leads. Four separate sites increment the rescue counter; this port has
one of them.

### THE DEATH POD IS NOT AN EVENT

`fn 0x1DD4F` was on the routine map as "death pod + score" because it prints
the pod's loss message. The pod itself is in the QUADRANT FILL, at 0x01649D,
and it is an object:

    if (ships already here >= 5)     no pod
    if (quadrant COLUMN != 8)        no pod
    if (Random(10) <= 5)             no pod        -- so four times in ten
    place 'R' in a free sector, enemy table type 6

That explains why a torpedo aimed at it answers "No damage reported." rather
than destroying it: it is an enemy-table entry with its own rules.

So `SCHED_DEATH_POD` is not a mistuned interval, it is a stand-in for a
mechanic of a completely different shape. Kept -- rebuilding it needs a sector
code and its own turn behaviour -- but tagged for what it is.

**The COLUMN 8 condition is extraordinary and is recorded as read.** The
instruction is `cmp word ptr [0x1DE6], 8`, reached from a verified jump target,
and `[0x1DE6]` is the quadrant column everywhere else in the binary. It is the
**second** such quirk here: reinforcements only ever arrive in column 1, and
that one the message text itself confirms by ending in a separate `-1.`
string. Two independent one-column rules make each other more credible, not
less. One emulator run flying column 8 repeatedly would settle it.

### Where the tiers stand after this

**FITTED: 0. DERIVED: 0.** Every constant in `core/trek.h` and
`core/planet.h` is now BINARY, CONFIRMED, MEASURED or ID, except seven
PROVISIONAL entries -- and all seven are the tractor-beam and death-pod
stand-ins, whose real shapes are now known and unbuilt rather than unknown.
(Both were built later, and the audit has read PROVISIONAL 0 since.)

---

## The tractor beam, and the last PROVISIONAL but three (2026-08-26)

### It catches you for flying PAST a Commander

`fn 0x0C609` is MOVE. At 0x00D83F, after a warp jump completes:

    for qy = min(old_y, new_y) .. max(old_y, new_y)
      for qx = min(old_x, new_x) .. max(old_x, new_x)
        if not caught yet
          if galaxy[qy,qx] >= 100                 -- enemies present
            if commanders[qy,qx] > 0
              if Random(10) > 7                   -- two times in ten
                pull the ship to (qy, qx) and stop
                "Lexington caught in long range tractor beam.
                 Pulled to quadrant N-N."

`fn 0x02203F` and `fn 0x022012` are max and min, which is what makes the loop
bounds the **bounding rectangle of the trip**. So a long jump across a
defended stretch of the galaxy is genuinely more dangerous than a short one,
and the Commander that catches you is the one you tried to fly past. A
scheduled event cannot express any of that -- which is why this port's version
dragged the ship to a random quadrant holding enemies, out of the blue.

`SCHED_TRACTOR` and its two invented intervals are gone from the enum.

### Which needed Commanders to be state, not a roll

`DS:0x23E9` is an 8x8 byte array of Commanders per quadrant. The quadrant fill
at 0x016133 makes the **first `commanders[q]` ships in a quadrant Commanders**
and rolls only the rest -- so which quadrant holds one is stable across
visits, where this port re-rolled every class on every arrival. The array is
written during generation (0x0052A2: a quadrant that got more than one ship
gets one three times in seven; 0x0054E3: the besieged StarBase quadrant always
does), added to by reinforcements, and moved between quadrants by the enemy
movement code, which decrements one entry and increments another.

**And there are none below command level 3** -- `cmp [0x1DF0], 7 / jl` guards
the whole commander branch of the fill.

The port now carries `gal_commander[]`, saves it (format version 4), and uses
it for both the fill and the tractor. Two more per-quadrant arrays -- for
battleships and scouts, at DS:0x2429 and the one after it -- are what the
original uses to fix the whole class composition per quadrant; this core still
rolls those two.

### SHIELD_RAISE_COST was right

The last PROVISIONAL that was a plain number. `fn` at 0x00EA55 sets the
shields-up flag and subtracts the real `86 00 00 00 00 48` from main energy,
which decodes to exactly **50.0**. The POWER DISTRIB reading that put main at
4950 of 5000 after a single SHUP was correct, and one observation had been
enough. Lowering shields is free -- there is no matching subtraction anywhere.

### Where this leaves the audit

    BINARY 92   CONFIRMED 18   MEASURED 19   FITTED 0   DERIVED 0
    PROVISIONAL 3   ID 113

**The three are the death pod's**, and they are a stand-in for a mechanic
whose real shape is known -- an object placed by the quadrant fill -- and
unbuilt at the time. (Built 2026-08-28.) Nothing in the port is now a guess
about a number.

### A note on the C128 image

The commander array and the save-format change pushed the `front` overlay five
bytes past its window. The window went from 0x0F00 to 0x1000 -- which costs
resident RAM one for one -- leaving 1,037 bytes free with the largest overlay
at 3,845 of 4,096. Growing the window is the right lever when ONE overlay is
tight and the resident region is not; moving code out is the right lever when
the resident region is.

---

## The enemy's shot, and a conflict I am not resolving by fiat (2026-08-26)

The last mechanic in the port whose arithmetic had not been read. `fn 0x16844`,
segment base **0x150C0** (the same unit as reinforcements), at 0x01696F:

    dy  = (ey - ship_y) / 8.0
    dx  = (ex - ship_x) / 8.0
    hit = hp * (0.6 + Random*0.1) * (1.5 - Sqrt(dx^2 + dy^2))
    absorbed = hit * (charge/2500) * (shield_sys%/100)      [shields up only]
    through  = hit - absorbed                                -- 0x016CE6
    energy  -= through                                       -- 0x016FCF
    shields -= absorbed * 0.8                                -- 0x01703C
    printed  = absorbed * 0.8    "Shields absorb N unit hit from ..."  0x017077

0.6, 0.1, 1.5 and 8.0 all decode exact.

### What this confirms

`1.5 - d/8` is identically `1.5 * (1 - d/12)`. **The falloff is linear in
Euclidean distance and reaches zero at twelve sectors, exactly as measured** --
the hardest part of the thirty-six-turn session, and it was right. The port's
`through = hit - absorbed` shape is right too, read directly at 0x016CE6.

### What it adds

**There is a random component, and the port had none.** The shot is uniform
across a band 15.4% wide. The measurement saw it and named it without
identifying it: *"k came out at 0.782 with a standard deviation of 0.038 over
all thirty-six, so the residual scatter is about 5% -- the random component,
and small."* A uniform band of that width has a standard deviation of **4.4%**
of its mean against the **4.9%** observed. That is the same distribution.

The port now rolls the band instead of a flat percentage.

### What it conflicts with, and what I did about it

The binary's mean factor is `0.65 * 1.5 = 0.975` of hit points at point-blank.
The measurement gives **0.782**. The ratio is **0.8022** -- and 0.8 is a
constant in this very routine, applied to the shield pool drain and to the
printed "Shields absorb N" figure. The likeliest reading is that the
thirty-six samples measured a quantity that carries that 0.8.

**I have not flipped the constant.** Thirty-six samples at eleven ranges is a
serious measurement, and this is one instruction's worth of doubt against it;
the rule this project has learned twice is to contest a conflicting
measurement, not to overwrite it. The band in trek.h keeps the MEASURED centre
of 78% and takes the BINARY's relative width, so the port is now right about
the shape either way.

**The experiment that settles it is one shot.** Shields DOWN, read main energy
before and after a single Commander's shot at a known range. The binary
predicts 0.90 to 1.05 of hit points times the falloff; the port predicts 0.72
to 0.84. There is no overlap, so one reading decides it.

### Two things that are not where they were assumed to be

**There is no hold-fire roll in this routine.** The firing loop skips an enemy
only when its hit points are zero (0x0168CE) or when its cell holds `'R'`
(0x016903) -- the death pod, which never shoots. Every other ship in the
quadrant fires every time the routine runs. `ENEMY_FIRE_ONE_IN` is a solid
observation -- 5/10, 5/10, 7/10, 4/8 -- of a mechanism that lives somewhere
else. Most likely the routine is not called on every turn, or a ship that
moved does not also shoot. Kept and flagged.

**The death pod does not fire lasers**, which is a second reason it needs
building as an object rather than an event: it is skipped by the fire loop and
has its own area effect.

## Docking, and the last routine on the read list (2026-08-27)

`fn 0x0F022`, segment base **0x9310** (scored 4/5 on disjoint spans; the fifth
candidate offset was a real, not a string). This was the one item NOTES carried
as "still to READ" -- what Supply depots and Research stations actually give,
because every base in every measurement session was a StarBase.

### The base type is the chart's tens digit, and it is a TYPE not a COUNT

    ax = galaxy[qy*16 + qx*2]          ; DS:0x2560
    type = (ax mod 100) div 10         ; 0x00F04D..0x00F064

and `fn 0x01F8D1` is a plain string table on that number:

    1  'StarBase'      2  'research station'      3  'supply depot'
    otherwise 'something'

The manual says the same thing in the L.R. SCAN section -- *"the number
indicates base type (1 is a StarBase, 2 a research station and 3 a supply
depot)"*. **So the middle digit of `enemies*100 + bases*10 + stars` is not a
count of bases; a quadrant holds at most one, and the digit says which kind.**
NOTES.md has called that digit `bases` since the RAY session; it is `basetype`.

### What each type gives

    ALL THREE      life support reserve [0x1D30] = 2.0        0x00F072
                   reserve-panel flag   [0x26D6] = 0          0x00F084
    if type > 1    docked flag          [0x26DA] = 0          0x00F06D
    StarBase       impulse [0x1D4E] = 500.0                   0x00F08F
                   if (energy  < 5000) energy  = 5000         0x00F0A1
                   if (shields < 2500) shields = 2500         0x00F0CD
    if type != 2   torpedoes [0x1DBE] = 9                     0x00F0FF

then "NAVIGATION: Docked.\nCOMMUNICATIONS: The administrator of <type> <column>
greets us." and 0.1 stardates (`7D CD CC CC CC 4C`, 0x00F160), which confirms
the measured docking turn cost. The refusal is "NAVIGATION: Not adjacent to
base".

500.0, 5000.0, 2500.0, 2.0, 9 and 0.1 all decode exact.

### This read CONFIRMED the port instead of correcting it

Worth saying plainly, because every mechanic read out of the binary before this
one corrected something already shipped, and it would be easy to carry that as
a rule. `trek_dock()` had the energy top-up, the shield maximum, nine
torpedoes, impulse 500, the StarBase-only safety and the 0.1 stardates all
right, from the manual. Three constants moved CONFIRMED/MEASURED -> BINARY on
the strength of it (`IMPULSE_START`, `IMPULSE_MAX`, `TORPS_START`), taking the
audit to **BINARY 97**.

Two details it settles that the manual could not:

**"energy torpedoes" is one noun.** *"Supply stations can provide life support
supplies and energy torpedoes"* parses as two gifts, and trek.h read it that
way. The game names the weapon "energy torpedoes (EnTorps)", and the binary
gives a supply depot no main energy -- `[0x1D54]` is touched only under
`type == 1`. One noun.

**Energy is a top-up because it can legitimately be over its maximum.** A
measured reading once put energy at 7435 against a 5000 ceiling. Shields cannot
do that -- every path that adds to them clamps at 2500 (0x00FB4D) -- so this
core's `ship.shields = SHIELD_MAX` and the binary's `if (< 2500)` are the same
function, and the difference is not worth a branch at 3% resident free.

### The Research Station is not pointless -- this core is just missing its gift

`[0x1D30]` is a **life support reserve measured in stardates, capped at 2.0**,
and it is a separate quantity from life support's repair percentage. Docking at
ANY base refills it. The reserve life support ITEM adds 1.0 and clamps to the
same 2.0 (0x009861, 0x009888), prints "Replenishing reserve life support.",
decrements an inventory byte at [0x234F], and answers "Not on reserve life
support." when used off-reserve. `[0x26D6]` is the flag for the swapped console
panel, set at 0x01FF4B when life support fails and cleared by both the dock and
the item; the panel repaint is the box (160,250)-(319,349) and it guards itself
by reading a pixel back.

That is the two-day countdown already on the feature list, arriving with its
constants attached, and it is what makes a Research Station worth flying to.
`LIFE_RESERVE_MAX_TENTHS` and `LIFE_RESERVE_ITEM_TENTHS` are in trek.h now,
ahead of the feature.

### One thing this did NOT settle, recorded as open

Whether `REPAIR_PER_STARDATE_DOCKED` is gated on the StarBase-only flag. This
core gives the 2.5x docked rate at all three base types. `[0x26DA]` is read at
nine sites and none of them is the repair routine -- 0x016887 is the enemy
skipping its firing turn (the manual's "its shields will protect your ship",
and what `trek_docked_safe()` already models), 0x00FEAA picks between two
status strings, 0x007FD3 is inside SAVE. A search for the rate reals in the
`mov cx,exp / xor si,si / mov di,hi` form finds 50.0 and 100.0 at nine
addresses but neither 20.0 nor 60.0, so the undocked rows are not loaded that
way and the repair routine has to be found directly. **Not guessed at, and the
code is unchanged.** Next step is to locate the repair routine from the STATE
OF REPAIR dialog's strings, which print both columns.

## The repair routine, and the +0.08 that made 50 look like 46.6 (2026-08-27)

Found from the STATE OF REPAIR dialog's own strings -- "STATE OF REPAIR" at
file 0xF190 resolves under the docking segment's base 0x9310, referenced at
0x00F20D -- and then from every writer of the twelve system percentages at
DS:0x235A. The repair routine is `fn` at **0x02024E**, and all three
[0x26DA] reads that the docking write-up could not place are inside it.

### The four rates are confirmed, and the original stores none of them

    focused:   sys[f] += Round(t * (docked ? 100.0 : 60.0))     0x02026E/0x0203B8
    the rest:  n = Round(t * 100.0)
               sys[i] += docked ? n div 2 : n div 5             0x020389/0x0203D3

100.0 and 60.0 decode exact. **That is why a search for the reals 20.0 and
50.0 found nothing in this routine** -- the undocked rows are an integer
division of a single rounded product, not constants. The earlier search that
came back empty was right about the bytes and wrong about the conclusion.

At tenths granularity `n` is always a multiple of ten, so `div 2` and `div 5`
are exact and this core's `rate * tenths / 10` is the same function.

### The docked rate IS StarBase-only -- the open question from the dock read

`cmp byte ptr [0x26DA], 0` at 0x02025F and 0x020367 selects the row, and
[0x26DA] is the flag docking CLEARS for types 2 and 3. So a research station
and a supply depot repair at the UNDOCKED rate. **This core applied the 2.5x
docked rate at any base**; `trek_advance_time` now tests
`ship.docked == BASE_STARBASE`. Fixed, with a test that fails when the gate is
put back.

### The focus is a claim on the CLOCK, not a rate row

This is the structural half, and this core had it as a rate.

    sys[f] += Round(t * focus_rate)
    if (sys[f] <= 100)  t = 0                       ; the day is spent
    else {
        [0x1DBA] = 0                                ; the focus clears itself
        t -= (sys[f] - 100) * 0.01                  ; overshoot back into time
        sys[f] = 100
    }
    ; then the loop over ALL TWELVE with whatever t is left

So a focus that does not finish starves everything, which is what eleven turns
of pinned shields measured. A focus that DOES finish hands the rest of the day
back to every system at the ordinary rate and unsets itself. The 0.01 scale is
the DOCKED focus rate and is used whether or not the ship is docked -- read at
0x02030E, left as it reads.

### The +0.08, and a measurement finally reconciled

The dialog's estimate at 0x00F37B is a different function from the mechanic:

    docked   = (100 - pct) * 0.02 * f + 0.08        f = 0.5  focused, else 1.0
    undocked = (100 - pct) * 0.05 * f + 0.08        f = 0.33 focused, else 1.0

printed `Str(x:4:1)`. **A constant offset of 0.08 stardates**, and it
reproduces the 2026-08-21 table exactly -- 2.08/5.08, 1.88/4.58, 1.26/3.03,
0.98/2.33, 0.78/1.83 against measured 2.1/5.1, 1.9/4.6, 1.3/3.0, 1.0/2.3,
0.8/1.8 -- once the row recorded as 60 points is read as 59, which its docked
column already implied. Solving a rate across those readings gave 46.6 because
the offset was being absorbed into the slope.

Note the two focus factors are NOT the same number: 0.5 halves the docked
estimate (100 against 50) and 0.33 thirds the undocked one (60 against 20).
Both were right; they belong to different columns. `REPAIR_FOCUS_FACTOR` being
"3, not the DERIVED 2" was true of the column it was read from.

### Two things found in passing

**The twelve system names, in index order**, from DS:0x1188 stride 16:
EnergyConverter, Shields, Life Support, Lasers, EnTorp Tubes, Warp Engines,
Impulse Engine, S.R. Scanner, L.R. Scanner, Computer, Transporter,
Shuttlecraft. This core's SYS_* order matches.

**The life support reserve drains here too**, at 0x02042E, immediately after
the repair loop:

    if (sys[2] < 90 && !docked) {
        [0x1D30] -= t
        if ([0x1D30] < 0) { [0x1DDE] = 430; "ENGINEERING:  Auxiliary life
                                             support depleted..." }
    }

So the countdown starts at **Life Support below 90%, not at zero**, it runs
only while undocked, and it kills at less than zero rather than at zero. With
the 2.0 cap and the item from the docking read, the whole mechanic is now
specified.

### Cost

Resident code grew 193 bytes (free 1,029 -> 836) for the corrected law. Four
constants moved MEASURED -> BINARY; the audit is **BINARY 101**.

## Life support is built, and the panel identified itself (2026-08-27)

The largest unbuilt feature, built from the three reads that specified it: the
docking refill (0x00F072), the canister (0x009861), and the drain that sits
immediately after the repair loop (0x02042E).

### Two details that were verified rather than assumed, and both mattered

**The drain uses the ORIGINAL elapsed time.** It subtracts `[bp-8]`, copied
from the parameter at 0x020151 -- *before* the focus block writes a remainder
back over `[bp+6]` at 0x02032F. So concentrating repairs does not slow the
countdown. Taking the drain off the same variable the repair loop uses would
have been the natural thing to write and would have been wrong.

**There are TWO thresholds, not one.** The console draw routine opens
`cmp word ptr [0x235E], 0x64` / `je` at 0x01FDC9, so the panel swaps as soon
as Life Support is not PERFECT -- below 100. The reserve only drains below 90.
There is a band, 90..99, where the console has already changed and the
countdown has not started, and one threshold would have lost it.

### The panel identified itself

The original repaints (160,250)-(319,349) of its 640x350 screen. In this
port's cells that is columns 20..39, rows 17..24 -- and `panels[P_SYSTEMS]` is
`{ 20, 17, 20, 8 }`. An exact match on all four numbers, so the panel the
original replaces was READ, not chosen. Its labels are "LIFE SUPPORT" and
"RESERVE, DAYS" at cs:0x403A and cs:0x4047, with a "0" tick at cs:0x4055 that
implies a gauge whose geometry is not read -- the bar in `draw_reserve()` is
this port's own and says so.

### A test that passed for the wrong reason

The 90..99 band test first read: set Life Support to 95, advance ten tenths,
expect no drain. **It passed under both thresholds and proved nothing** --
repair runs before the drain check, and at 20 points a stardate a full day
lifts 95 clean past 100, so neither rule drained. Caught by breaking the
constant, which is the only reason it was caught at all. It is now 92 and ONE
tenth, which leaves 94 inside the band, and it fails under the wrong rule.

Two other things the rig nearly hid. `make` did not rebuild between four rapid
break-tests, so three of them reported the FIRST break's failure -- the giveaway
was that the "restored" run still showed a failure. Every break test now forces
a clean build. And `make verify` refused the death message: the original's own
line is already elided in the binary ("Auxiliary life support deplete...") and
at 34 characters it overran the 26 the message panel fits.

### Cost

405 resident bytes, leaving 2,172 free -- the overlay pass earlier today paid
for this and left change. Save format goes to version 5, 679 bytes. Audit
BINARY 103.

## The boarding party, and a routine the map had wrong (2026-08-27)

`fn 0x15D6E`, segment base **0x150C0** (5/5 on disjoint spans). The routine map
listed it as "department damage" and it is nothing of the kind -- **the third
label in that map to be wrong**, after the plasma bolt (0x09563) and the main
program (0x04FD1). Its strings name it in one read:

    0x0C11  'SECURITY:\nThe Mongol boarding party has been eliminated.'
    0x0C4A  'SECURITY:\nA Mongol boarding party has transported into '
    0x0C82  'Engineering.'   0x0C8F 'Laser control.'   0x0C9E 'EnTorp control.'

### The whole mechanic, called from the turn loop at 0x005954

    if (aboard && stardate > deadline)   eliminated; RETURN without rolling
    if ([0x1DF0] <= 7)         return      ; level + 4, so level 4 and 5 only
    if (aboard)                return      ; one party at a time
    if ([0x26DC])              return      ; the shields-up flag: they BEAM in
    if (galaxy[quad] <= 99)    return      ; at least one enemy in the quadrant
    if (Random(100) <= 95)     return      ; four in a hundred
    aboard   = Random(3) + 1               ; 1 Engineering 2 Lasers 3 EnTorps
    deadline = stardate + 0.5 + Random*0.5

0.5 decodes exact, and the Random is the argument-less one -- ONE-SIDED, so
the stay is 0.5 to 1.0 stardates and never shorter.

### What each department costs, in the original's own words

    Engineering    0x00EA12  "Cannot raise shields; Mongol boarding party
                              controls engineering."
    Laser control  0x009CD0  "Mongol boarding party controls the lasers."
    EnTorp control 0x00B60B  "EnTorp control is held by the Mongol boarding
                              party."

All three are tested at the TOP of their command, before it asks for anything,
which is where this port now tests them too.

### A redundancy recorded rather than built

The Engineering branch at 0x015E61 clears the shields-up flag [0x26DC]. **It
can never do anything**: the gate five instructions earlier already required
that flag to be clear. Read as "taking Engineering drops your shields" it
would have been a striking mechanic, and it would have been invented -- the
original cannot reach it. Recorded in trek.h beside the constants.

### [0x26DC] identified in passing

It is the SHIELDS-UP flag, from SHIELDS UP at 0x00EA08: `cmp [0x26DC],0 / je`
picks between "ENGINEERING: Shields already up." and "ENGINEERING: Raising
shields", and the 50.0 subtraction follows. Nine other sites read it.

### Cost, and a warning

777 resident bytes, leaving 1,395. **The `front` overlay is now 3,994 of
4,096 -- 102 bytes of headroom.** The save record grew again (v6, 682 bytes)
and the string index with it, and both live in `front`. The next save-format
change overflows it. That overlay wants splitting, or the window wants
growing, BEFORE the next feature that touches serialisation.

Audit BINARY 112.

## The spontaneous supernova, which is not the torpedo one (2026-08-27)

`fn 0x1ED00`, segment base **0x1BD60**. The read list called this "the
supernova" as though there were one; there are two routes and they behave
completely differently.

    if (Random(100) != 0)  return              ; one in a hundred, per turn
    repeat
        row = Random(8)+1;  col = Random(8)+1
    until row != [0x1DE4]                      ; NOT the ship's quadrant ROW
      and (galaxy[q] mod 10) > 0               ; the quadrant holds a star
      and galaxy[q] != 999                     ; and is not already burnt

    n = galaxy[q] div 100
    [0x1DC2] -= n                              ; enemies remaining
    if ([0x1DC2] == 0) [0x26C6] = 1            ; the WON flag
    galaxy[q] = 999                                            0x01EEEB
    chart[q]  = 2                                              0x01EEFF

### Three things worth pulling out

**IT CANNOT TOUCH THE SHIP.** The exclusion is on the ROW alone -- eight
quadrants, not one -- so a supernova never occurs anywhere along the ship's
own rank. **The emulator observation of being blown to quadrant 8-4 belongs to
the TORPEDO route** (the 4% branch of 0x0A8C8) and not to this one. Filing
both under "the supernova" would have produced a feature that threw the ship
about at random.

That row exclusion is the third quirk of its shape in this program, after the
death pod's column 8 and reinforcements' column 1. Recorded as read.

**IT CAN WIN THE GAME.** `[0x1DC2]` is the enemies-remaining count and the
routine sets the won flag directly when it reaches zero. A mission can end
because a star two quadrants over happened to go up.

**THE PLAYER LEARNS OF IT WITHOUT SCANNING.** `chart[q] = 2` writes the
RECORDED chart at DS:0x2360, not the galaxy -- so a burnt quadrant appears on
the L.R. chart having never been visited. The manual's line about scanners
overloading and displaying all 9's is the same fact from the other side.

### What it cancels, and the one thing not identified

The base-under-attack pair at [0x1E0A]/[0x1E0C] is cleared when it names the
destroyed quadrant -- [0x1E0A] is written at game setup (0x0054EB) and read by
the base-attack scheduler `fn 0x1F9D5`, which is what identifies it. A second
pair at [0x1E1C]/[0x1E1E] is cleared too, along with schedule slot [0x1D9C],
which is set to the real 9999 -- never, and the same sentinel SCHED_NEVER
already models. **That second pair is NOT identified.** This core maps the
behaviour onto the base state it has and says so at the call site rather than
guessing.

### Cost

349 resident bytes, leaving 1,028. gal_nova[] is 64 bytes of galaxy state --
the original needs none, because its galaxy is one number per cell and 999
overwrites the lot; this core keeps the three digits apart and the sentinel
needs somewhere to live. Save v7, 746 bytes. `front` went 3,353 -> 3,471 with
the bigger record, which is exactly the room this morning's split bought.
Audit BINARY 114.

## The torpedo's supernova -- the route that CAN kill you (2026-08-27)

`fn 0x0A8C8`, segment base **0x9310** at 10/10. Everything the 2026-08-26
emulator session saw and could not explain is in this one routine, and its
strings are that screenshot verbatim: 'Star at ', ' goes supernova!',
'Lexington blown to quad ', ' Mongols destroyed.', ' unit hit absorbed by
shields.', ' units of damage to Lexington.', 'Lexington destroyed.'

### Where the ship goes is NOT random, and the rule is dimensionally odd

    nqy = qy;  nqx = qx
    if      (star_row < qy && qy < 8)  nqy = qy + 1
    else if (star_row > qy && qy > 1)  nqy = qy - 1
    else if (star_col < qx && qx < 8)  nqx = qx + 1
    else if (qy < 8)                   nqy = qy + 1
    else                               nqy = qy - 1

**It compares the star's SECTOR coordinate against the ship's QUADRANT
coordinate.** Both happen to be 1..8 so it compiles and runs; it means
nothing physically. Recorded as read.

It reproduces the measured throw exactly: star at sector 4-4 with the ship in
quadrant 7-4 gives 4 < 7 and 7 < 8, so the ship goes to row 8 -- "Lexington
blown to quad 8-4", which is what the screen said. That is the observation
that had been filed for a day as needing an emulator session.

### The rest

    every Mongol in the quadrant dies -- the four hp slots are zeroed
        outright at 0x00AAC8, with no survival roll; reaching zero WINS
    damage = Random(600)
        shields up    shields -= damage, the WHOLE hit
        shields down  energy  -= damage, floored at 0, AND the damage is
                      added to the turn's hit tally at 0x00ABFE -- so a
                      supernova can wreck systems through the ordinary
                      Round(hits/350)+1 path
    galaxy[here] = 999                                       0x00AC84
    if (galaxy[destination] == 999) "Lexington destroyed."   0x00AC2F

**Thrown into a quadrant that is already burnt, the ship is destroyed.** That
is also why the "blown to quad" line is suppressed when the destination is
999 -- you never arrive. Unlike the spontaneous route this one does NOT write
the recorded chart; it does not need to, you were there.

### A test that could not fail, again

The throw test first used a star ABOVE the ship, and inverting the first
condition changed nothing -- because branch one and the fallback both give
`qy + 1`. Only a star BELOW the ship exercises branch two, which is the one
that differs. Caught by breaking the condition and watching nothing fail; the
second case is in the suite now. Same shape as the life-support band test
earlier today.

### Cost, and the budget is now the constraint

850 resident bytes as first written, which left **178 free**. Moving the
four-line report into OVL_CMDS -- it is 4% of a star hit, while
`fire_one_torpedo` around it is the hottest code in the game -- brought that
back to **484**. Net 544 bytes for the feature.

**484 is not enough for another feature.** The next one needs an overlay pass
first. The remaining resident candidates are the second wave of `do_*`
handlers: `do_repair`, `do_chart`, `do_planets`, `do_hail`, `do_sound`,
`do_max_energy`, `do_shields_up/down` -- all small individually, and
`do_repair`/`do_planets` call INTO other overlays so they cannot move without
merging. Audit BINARY 116.

### ~~Still unbuilt from this routine~~ BUILT 2026-08-29

The third star outcome, and it turned out to be galaxy state rather than a
cell change -- see "what a star really does" above. `fn 0x0A8C8` splits the other 96% as 38.4% "Torpedo
absorbed by star." and 57.6% the star DESTROYED, cell -> 'N'. This core still
answers TORP_ABSORBED for all of it, because the destroyed case wants a nova
SECTOR code it does not have.

## The Vandal Death Pod, and a guess the audit could not see (2026-08-27)

`fn 0x20B38`, found from its own strings under base 0x1BD60 --
"DAMAGE REPORT:\nVandal Death Pod enters quadrant: ", " unit hit on
Lexington.", " Mongol ship(s) destroyed.", and "…Lexington destroyed."

    if (![0x26E0])            nothing        ; a pod is in this quadrant
    n = ([0x1DE6] > 6) ? 20 : 33             ; 0x020B4C, the quadrant COLUMN
    if (Random(n) != 0)       nothing
    hit = Random(50) + 50                    ; 0x020B76
    SHIELD CHARGE [0x1D60] -= hit            ; 0x020B9F, whole, no split
    every ship whose table type is NOT 6 loses the same   ; 0x020BDE
    if (shield charge <= 0)   "Lexington destroyed."      ; 0x020C64

### Four things this port had wrong

**The damage was invented.** `40 + trek_rand_n(40)` -- a band made up around
the single measured reading of 59, which sits inside both. It is 50..99.

**The frequency was a scheduled interval**, with two PROVISIONAL constants
holding it up. It is a per-turn roll, and it is COLUMN-DEPENDENT: half as
likely again in quadrant columns 7 and 8. That is the FOURTH column/row quirk
in this program, after the pod's own placement in column 8, reinforcements in
column 1, and the spontaneous supernova's row exclusion.

**It takes the shield CHARGE, whole**, with no absorption arithmetic and no
reference to whether the shields are raised.

**It kills you outright if that takes the charge to nothing.** A captain
flying on a flat charge dies to the first detonation. That is what the code
says, and it is why the thing is feared.

### THE AUDIT COULD NOT SEE THE GUESS

`make tiers` had reported `FITTED 0, DERIVED 0` for days and was right about
every constant it could see. It reads `#define`s in two headers. The invented
band was a bare literal inline in `core/trek.c` and had never been near a
header, so it was not merely untagged -- it was **invisible**.

`tools/tiers.py` now audits randomness literals as well: any number >= 20
appearing as a `trek_rand_n` argument, or added to one, must be a named
constant. The bar is 20 because below it the numbers are dimensionless -- a
coin, a d6, +1 for a 1-based index -- while above it a literal in a random
roll is a claim about a DISTRIBUTION, which is exactly the kind of claim this
project sources. It found three sites: the pod's two, and `trek_rand_n(100)`
in the supernova's star roll, now `NOVA_STAR_OF_N`.

### The audit is clean

**BINARY 126, CONFIRMED 16, MEASURED 15, FITTED 0, DERIVED 0, PROVISIONAL 0.**
"0 constants NOT measured or read." The three surviving PROVISIONALs were the
pod's stand-in and they are deleted rather than measured, which is what was
always going to close them.

Resident free went UP, 1,452 -> 1,726: deleting the scheduled stand-in cost
more than the real roll. Save v9, 553 bytes.

### ~~What is still unbuilt here~~ BUILT 2026-08-28

The pod as a SECTOR OBJECT: `SEC_POD`, 900 hit points, killed by lasers only
and never counted as a Mongol kill, with a torpedo answering the cloaking
device. And the detonation's gate moved from "the pod is HERE" to "the pod is
ALIVE", galaxy-wide, which is what the object was built to make survivable.

### A test retired rather than faked

"Two events due in the same window both fire, oldest first" used
SCHED_DEATH_POD as an independent second slot. With the pod gone the two
remaining slots RESCHEDULE EACH OTHER -- whichever fires first pushes the
other past the end of the window -- so the property cannot be expressed with
them. Retired with the reason and a note to restore it when a third
independent slot arrives; the hail response, the boarding party's own slot,
the Union distress call and the supernova are all real slots in the
original's eight.

## HAIL, and a string pool that had quietly overflowed (2026-08-27)

`fn 0x207FD`, base 0x1BD60 at 5/5.

    best = 8.0
    for row in max(qy-4,1)..min(qy+4,8)
      for col in max(qx-4,1)..min(qx+4,8)
        if ((galaxy[q] mod 100) div 10) != 1  skip      ; a StarBase ONLY
        d = Sqrt((col-qx)^2 + (row-qy)^2)               ; 0x020906..0x020922
        if (d < best) { best = d; found = row,col }

    if (Random(10) < 2)   "...blocked by subspace interference."
    else if (best <= 2.0) "...open...  StarBase in R-C responds."
    else                  "...open...  No response."

Three things worth having: the search box is **±4 quadrants**, only a
**StarBase** answers -- research stations and supply depots never do, which
follows from the base-type digit read at the dock -- and the reply threshold
is **2.0 quadrants**, Euclidean. This core compares SQUARED distances
throughout; the order is identical and the 6502 does not need the sqrt.

**The empty COMMUNICATIONS box the 2026-08-23 session saw was "No response."**
There was no StarBase in range, which is exactly the case this prints it for.
That reading was right and its note said "nothing is invented here" -- which
is why there was nothing to unpick when the routine turned up.

NOT built: the routine ends by writing schedule slot [0x1D78] with
`stardate + something * 0.5` (0x020A65). Hailing schedules SOMETHING and this
core has two of the original's eight slots. Not invented; recorded.

### THE STRING POOL HAD OVERFLOWED, and nothing said so

Adding HAIL's replies took the pool past 256, and `S()` took a **uint8_t**.
Every id from 256 up silently WRAPPED -- `S_262` fetched string 6 -- so a set
of messages would have shown the wrong text on a disk that built, linked and
passed `make verify` clean.

**The only complaint was a compiler warning, in a wall of twenty-three of
them.** `make verify` checks the key table, the panel widths, the overlays and
now lowram; it had nothing to say about this because the failure is a type,
not a layout.

`StrId` is a named type now and `c128/src/strpool.c` fails to COMPILE if
STR_COUNT outgrows it -- verified by narrowing it back to uint8_t and watching
the build refuse. The lesson is the same one lowram taught this morning from
the other direction: **the thing that reports is the thing that gets managed,
and a warning nobody reads is not a report.**

### Cost

HAIL and the wider ids together: 266 resident bytes, leaving 1,460. Its prose
is in OVL_CMDS. Four break-tests -- any base type answering, the range test
removed, the block roll removed, the turn cost removed -- each failed its own
assertion. Docking's identical 0.1-stardate cost had only INDIRECT cover and
now has a direct assertion, which that same break found.

## The enemy's turn is gated once, not per ship, and only if you MOVED (2026-08-27)

Found by asking who CALLS `fn 0x016844`. There is exactly one caller, in the
turn loop at 0x005917:

    if ([0x1F31] == 'Y')       skip the enemy turn entirely
    if ([0x26E3]) {                          ; the ship MOVED this turn
        if (Random(100) >= 60) skip the enemy turn entirely
    }
    call 0x01658F              ; enemy MOVEMENT
    call 0x016844              ; enemy FIRE
    [0x26E3] = 0                             ; 0x00594A, either way

### What this port had, and why it looked right

`ENEMY_FIRE_ONE_IN 2` was a MEASURED observation -- 5/10, 5/10, 7/10, 4/8 --
and it was a good one. It was implemented as a **per-ship hold-fire roll**,
which is a mechanism the original does not have anywhere: the firing loop
skips a ship only when its hit points are zero or its cell holds the death
pod, as the 2026-08-26 read of that routine already said. Every ship in the
quadrant fires, or none does.

**And the gate only applies if you moved.** `[0x26E3]` is set by `fn 0x0C609`
(MOVE) and by the supernova throw at 0x00AC9B, and cleared the instant the
enemy turn resolves. Stand still and the enemy always acts; move and there is
a 40% chance of getting away with it -- and it skips their MOVEMENT too, not
just their fire.

That is a real tactical rule the port did not have, and it explains the
measured "about half" exactly: those samples were taken while flying about.

### The one thing left unbuilt -- and it was settled the next day

`[0x1F31]` is one of the setup screen's two Y/N answers -- `fn 0x14CB9` asks
"Will you require a briefing <Y/N>?" and "Restore a saved game <Y/N>?", and
0x014DFA upper-cases the reply into it. A 'Y' suppresses the enemy's FIRST
turn and the loop forces it to 'N' at 0x0059CE. ~~**Which of the two questions
it is has not been read**~~

**SETTLED 2026-08-28 ON THE RIG: it is the RESTORE question.** Two games with
the restore answer held at N and the briefing answered N then Y; the byte read
'N' both times, so it does not track the briefing. The briefing was
screenshotted actually appearing in the second game, because a test whose
input never registered cannot fail and proves nothing.

It agrees with a second reading taken the same day for another purpose: the
guard at 0x0058A6 skips the initial quadrant build when `[0x1F31] == 'Y'`,
which is restore semantics -- a loaded game must not have its quadrant rebuilt
underneath it -- and suppressing the enemy's first turn on load is the same
courtesy. Two independent lines, one answer.

### A fourth test that could not fail

Breaking the gate three ways, the third -- deleting `ship.moved = 1` from
every movement routine -- failed NOTHING. Both new cases set the flag by hand,
so neither could see whether movement sets it. The link is tested now:
`trek_move_impulse` and `trek_move_warp` each assert the flag afterwards, and
deleting the assignment fails both.

That is the fourth time in one day a fixture bypassed the mechanism it was
meant to exercise, and the fourth time only breaking the code found it.

### Cost

**128 resident bytes**, free 1,460 -> 1,332. The turn gate plus the `moved`
flag and its two assignments cost more than the per-ship roll they replaced.
Audit BINARY 133, and `ENEMY_FIRE_ONE_IN` is gone.

## Supply capture, and the item table it settles (2026-08-27)

`fn 0x181E8`, base 0x150C0. The routine map called it "capture,
boarding-party frequency"; it is neither.

    print "Mongol supplies captured..."
    if (Random(4) == 0) { print "none."; return }      ; one in four, nothing
    for item in 1..3:                                  ; ONE roll each
        if (Random(item + 2) <= 1) { inventory[item]++; print its name }
    inventory[4] += 2                                  ; ALWAYS, on a success
    print "Life support supplies"

So the odds per item are **2 in 3, 2 in 4, 2 in 5** -- diminishing by
position, one roll apiece, not a loop that can give two of the same thing.

### It settles the item table

The names are at **DS:0x10D4, stride 22**, and the inventory is bytes at
**DS:0x234B**:

    1 Mongol energium      2 Plasma bolts        3 Plasma bolt shield
    4 Life support supplies                      5 Raw energium

That is this core's `item_name[]` exactly, in order, one index apart (the port
is 0-based). It also confirms the reserve life support counter: `[0x234F]` is
`0x234B + 4`, which is the byte the USE item decrements at 0x0098A1 -- read
days ago from the other end and matching here without being made to.

**Only items 1..3 are rolled.** Raw energium is never captured; life support
supplies are never rolled, they are simply +2 on any success.

### Where it comes from, and the one thing NOT pinned

The caller is `fn 0x0E3A1`, which base.py names 6/6 as the **LANDING PARTY**:
"Planet settlers found...", "Evacuating settlers...", "Landing party
attacked...", " casualties.", "energium successfully mined.", "Nothing
found." So supplies are captured by LANDING, not by destroying a supply ship.

The condition is `[0x24A9 + quadrant] > 20`, a byte-per-quadrant array
(stride 8). ~~**What raises it past 20 is not read**~~

**IT WAS ALREADY READ, ON 2026-08-26, AND THIS FILE DID NOT KNOW.**
`core/planet.h` has carried the generation law -- the +10/+20 branches and
their exclusivity -- since the planet model was built. The open item here was
a stale duplicate of a question already answered two days earlier, and it sat
on the read list being counted as work. Grep for the CLAIM, not the file.

What IS new from 2026-08-28 is the second half: **nothing raises it at
runtime, and the only runtime writes LOWER it.** For the record, generation at
0x0052FD:

    if ([0x24A9+q] != 0)      skip            ; one planet per quadrant
    class = Random(3) + 1                     ; 1..3, stored as-is
    if (class <= Random(5))   += 10           ; 0x005356
    else if (Random(2) == 0)  += 20           ; 0x00539A, one in two

The two branches are EXCLUSIVE -- `ja` at 0x005341 skips the +10 and lands on
the Random(2) test. So the byte is `class + 0`, `class + 10` or `class + 20`,
which is why the code tests `> 20` and `> 10` rather than equality: the units
digit is the planet's CLASS and the tens digit is what is on it.

**And the only runtime writes LOWER it.** 0x00E88F and 0x00E905, both in the
landing code, are the same instruction: `[0x24A9+q] = value mod 10`. That
strips the tens digit and keeps the class -- which is exactly `PF_TAKEN` in
this port's planet.h, arrived at independently.

So the trigger needed no invented predicate and never did -- and the port has
been right about this since 2026-08-26. The find is rolled when the galaxy is
made, and collecting it clears the tens digit.

## `fn 0x23FD2` is not the INFO display

The map had it as "INFO/scan display -- MONGOL BASE". It has NO strings, a
0x40A-byte frame, and blits an image through a far pointer at [0x26C8] to a
fixed screen position; the turn loop calls it at 0x00590B when [0x26D4] is
set. It is graphics, and this port renders its own console.

The "MONGOL BASE" the map attached to it is a string at 0x23F90 -- BEFORE the
prologue, belonging to whatever precedes it. A label taken from the nearest
string in the file rather than from the strings the routine PUSHES. That is
the fifth wrong entry in that map.

## The planet byte, and a correction to this morning's supernova (2026-08-27)

Chasing what raises `[0x24A9 + q]` past 20 settled three things at once, and
corrected one I had got wrong the same day.

### The per-quadrant planet byte, from setup at 0x0052FE

    planet[q] = Random(3) + 1                  ; 1..3, the CLASS
    d = Random(5)
    if (planet[q] <= d) {
        planet[q] += 10                        ; -> 11..13, ENERGIUM
        [0x1E1C] = row;  [0x1E1E] = col        ; and it is THE settled planet
    } else if (Random(2) == 0) {
        planet[q] += 20                        ; -> 21..23, MONGOL SUPPLIES
    }

**This core already had it.** `PFIND_ENERGIUM_OF_N 5` and
`PFIND_MONGOL_OF_N 2` in planet.h are these two branches, read days ago from
the ORBIT side. The supply-capture trigger recorded this morning as "unread"
is `find == PFIND_MONGOL`, and it has been in the port all along -- **found by
not sweeping planet.h before writing the note.** The same failure this session
has been recording all day, committed by me, in the same session.

### The landing party's three outcomes, fn 0x0E3A1

    if (planet[q] > 20)  supply capture, fn 0x181E8, and RETURN
    if (planet[q] > 10)  "energium successfully mined.", [0x2350]++
    else                 "Nothing found."

`[0x2350]` is `DS:0x234B + 5` -- **item 5, Raw energium**. The inventory
indices line up across three independent reads now.

**READ IN FULL 2026-09-02 -- see "The landing party's attack, read from the
branches" at the end of this file.** What follows was written before the
control flow was traced and gets two things wrong.

Note the FIRST branch returns: landing on a Mongol-supply world runs the
capture and nothing else. This core rolls a 1-in-5 attack there and otherwise
answers LAND_NOTHING, which belongs to the settlement branch above it, not
this one. Recorded, not yet corrected -- the attack roll's real home is the
part of fn 0x0E3A1 before 0x00E49C and has not been read.

### [0x1E1C]/[0x1E1E] IS THE SETTLED PLANET, and this morning's write-up was wrong

The spontaneous-supernova entry says of that pair: *"That second pair is NOT
identified; this core maps the behaviour onto its own base state."* It is the
settled planet's quadrant, and `core/planet.h` had said so for days.

`fn 0x0E3A1` opens by testing the ship's quadrant against it, and if
`[0x1DA2] > stardate` prints "Planet settlers found..." / "Evacuating
settlers..." and sets `[0x1D9C]` to the real 9999. **So `[0x1DA2]` is the
EVACUATION DEADLINE and `[0x1D9C]` is its schedule slot** -- planet.h's "the
DEADLINE is not modelled yet, that wants fn 0x151D0 read" now has its address
without that read.

Both supernova routes do the same two writes. They are not cancelling a base:
**a settlement inside a supernova is destroyed and nobody is coming for it.**
`planet_quadrant_lost()` now clears PF_SETTLED in a burnt quadrant, called
from both routes, and removing it fails its test.

### Cost

94 resident bytes, free 1,332 -> **1,238**.

## Supply capture built, and the landing attack was in the wrong place (2026-08-27)

`fn 0x181E8`, reached from the landing party when the planet's byte exceeds
20 -- which is `find == PFIND_MONGOL`, a thing this core has had all along.

    if (Random(4) == 0) { "none."; return }
    for item in 1..3:   if (Random(item + 2) <= 1) inventory[item]++
    inventory[4] += 2                    ; life support, always, on a success

Two in three, two in four, two in five for Mongol energium, plasma bolts and
the plasma bolt shield. **Raw energium is never captured and life support is
never rolled.**

### The attack roll was in the wrong branch

`fn 0x0E3A1` rolls `Random(5) == 0` at 0x00E435 -- once per landing, for EVERY
landing that is not the settlement rescue, BEFORE the find is looked at -- and
then **falls straight through**. A mauled landing party still comes back with
whatever was there.

This core rolled it only inside the Mongol branch and RETURNED, which made it
both far too rare and mutually exclusive with a find. `LANDING_ATTACK_OF_N`
and the 2..6 casualty band were right; only the placement was wrong. The
casualties now come back through the out-param on any outcome and the UI
prints them ahead of the find.

### Two harness failures, and the second was the dangerous one

**`timeout` does not exist on macOS.** A break-test helper written as
`timeout 120 make test | grep FAIL` produced NO OUTPUT for all four breaks,
which reads exactly like "no test failed". Four non-discriminating tests would
have been recorded as verified. Every break has to be run with the command
that actually exists.

**And the capture test was reseeding the RNG every iteration.** 2,000 calls to
`trek_new_game(3, 5000 + i)` measure the SEEDING, not the distribution: the
first draw after consecutive seeds is not uniform, and `none` came out well
outside the 1-in-4 band. Seeding once and looping fixed it. That is the fifth
"test that could not do its job" today and the first that was failing rather
than falsely passing -- which is the only reason it was caught before the
harness bug was.

### Cost

4 resident bytes net, free 1,238 -> 1,234: the capture and its prose went into
the planet overlay, and removing the attack branch paid for most of the rest.
Audit BINARY 138.

## fn 0x15F51 is the initial quadrant build, and the death pod does not add up (2026-08-27)

The last entry on the read list. The map called it "Union wreckage -- unbuilt
screen"; it is neither. **Sixth wrong label in that map.**

It clears the sector map at DS:0x2622 to '.' over a 10x10 grid, puts 'E' at
the ship's sector from [0x1DE8]/[0x1DEA], and fills the quadrant. It has
**exactly one caller**, at 0x0058AD in the game-start sequence, guarded by
`[0x1F31] != 'Y'` -- so it does not run when you restore a save, which needs
no fresh quadrant.

### The death pod does not add up, and I am not resolving it by fiat

The pod placement at 0x1649D..0x164E8 is INSIDE this once-only routine, and

    the ONLY write of 'R' to the sector map in the whole binary is 0x164D8

so on a static reading the pod is placed once, at game start, in the STARTING
quadrant, and only when that quadrant is column 8 with fewer than five ships
and a 4-in-10 roll. This core places it on EVERY quadrant entry.

> **REFUTED THE SAME DAY, on the rig.** Flying to quadrant 5-8 mid-game put an
> `'R'` on the scan and set `[0x26DF]` -- a quadrant that was not the start.
> `fn 0x15F51` IS reached on quadrant entry by a path an absolute-address
> caller search cannot see, exactly as the `[0x26E0]` initialiser was. The
> pod is placed on EVERY entry. See NOTES.md, 2026-08-28 (later).

Worse, the flags do not meet:

    placement    sets [0x26DF] = 1               0x0164BE
    detonation   tests [0x26E0] != 0             0x020B58
    destroying the pod clears BOTH               0x01E9BE, 0x01E9C3

`[0x26E0]` is written **only with zero**, at one site, and tested at one site.
An exhaustive capstone decode of every `e0 26` byte pair in the game region
finds those two instructions and nothing else. Both bytes lie past the end of
the load image (file is 0x2F4E0; DS:0x26E0 maps to 0x2FF00), so they are BSS
and start at zero.

Read literally, that says the detonation can never fire -- **and a screen
observation says otherwise**: "Vandal Death Pod enters quadrant: 59 unit hit
on Lexington", with both Mongols in the quadrant dropping by the same 59.

So the read is INCOMPLETE, not the game. The likeliest gap is a write through
a pointer, which no absolute-address search can see. **No code was changed on
the strength of this.** The port keeps its per-quadrant placement, which
reproduces what was actually observed, and the discrepancy is recorded with
the experiment that settles it.

**THE EXPERIMENT, and it is a small one:** watch DS:0x26E0 under
dosbox-automation across a few dozen turns and note when it becomes non-zero.
One byte, one poll per turn. If it never does, the observation belongs to some
other routine and the pod needs re-reading from the message end; if it does,
the setter is a pointer write and the state that precedes it says which.

## Two findings from one DOSBox session (2026-08-27)

### HAIL COSTS NO TIME, and this is the second time that claim has been wrong

Jamie, mid-run: *"hail does not advance the stardate."* The session agrees --
**twenty-five consecutive HAILs left the stardate at 3500.00 exactly**, with
energy and shields untouched.

The claim came from reading the message log's `3500:34` stamp as a stardate on
2026-08-23. **Jamie caught it on 2026-08-24**, and the retraction went into
MEASURED.md and the memory. It did NOT go into `core/trek.h`, where the
sentence "MEASURED 2026-08-23: it costs a turn -- the stardate moved 3500.1 to
3500.2" survived above `do_hail`. Building HAIL earlier the same day, that
file was the source consulted, the stale line was believed, and
`trek_advance_time(1)` went into `trek_hail()` with a test asserting it.

**A retraction that does not reach every copy of the claim has not retracted
it.** The other copies were struck; this one was the one that got read.
Removed, with a test that now fails if the call comes back.

### [0x26E0] IS SET -- and it means "the pod is alive", not "a pod is here"

The static read said that byte was written only with zero and concluded the
death pod's detonation could never fire. **One reading settles it: it is 1
immediately after a new game**, with `[0x26DF]` at 0, no 'R' anywhere on the
sector map, and the ship in quadrant 7-7 -- nowhere near the column-8
placement rule.

    energy at 181956, DS base 174448
    start quadrant 7-7
    [0x26DF] pod-here  = 0
    [0x26E0] pod-armed = 1        <-- and 1 for all 25 turns following

So the flag is initialised true and, per the disassembly, only ever CLEARED --
at 0x01E9C3, in the branch that runs when the destroyed thing is the pod. Its
meaning is **"the Vandal Death Pod has not been destroyed"**, galaxy-wide.

That is why the message reads "Vandal Death Pod ENTERS quadrant": the
detonation is gated on the pod being ALIVE, not on it being in the quadrant
you are standing in. `[0x26DF]` is the per-quadrant "there is an 'R' here"
flag and is a different thing.

**The exhaustive decode was right about the instructions and wrong about the
conclusion.** No absolute-address write of 1 exists; the initialisation is a
pointer write or a block fill that no such search can see. This is exactly the
shape [[negative-claims-about-egatrek]] warns about, and the one-byte
experiment named for it cost about four minutes.

**NOT CHANGED YET, deliberately.** This core gates the detonation on
`pod_here`. Flipping it to "alive" makes the pod a galaxy-wide hazard from
turn one -- which is what the original does, but the original also lets you
DESTROY it, and this core has no 'R' sector object to shoot at. Shipping the
hazard without its counter would be worse than the current approximation. The
gate flips when the pod becomes a sector object, and that is now the reason to
build it.

## THE 0.8 CONFLICT IS SETTLED, and the binary won (2026-08-27)

Eighteen shots under dosbox-automation, one enemy of 1,000 hit points poked
into the table and onto the sector map, shields DOWN, energy and shields
pinned before every turn -- **and the twelve system percentages written back
to 100 before every turn, which is what made it work.**

Dividing each energy drop by `hp * (1.5 - d/8)` recovers the binary's first
factor directly:

    n = 18   min 0.6042   max 0.6965   mean 0.6538

    BINARY   0.6 + Random*0.1  ->  0.600 .. 0.700   18 of 18 inside
    PORT     72..84 (MEASURED) ->  0.720 .. 0.840    0 of 18 inside

### Pinning the systems is the whole story

Jamie called it mid-run: *"repair the systems."* Before that the same loop
produced 0.6387, 0.6838, 0.6973 **and 1.1964** -- and 1.1964 is not a shot at
all. The board was degrading under its own fire and the readings drifted
upward and scattered.

**That is what the 2026-08-26 session was measuring.** Thirty-six turns at
eleven ranges gave 0.782 with sd 0.038 and produced the "unresolved factor of
0.8"; it was a decaying board, not a scale error in the binary. The rig fix
that settles it is one line -- `P.write(DS+0x235A, (100).to_bytes(2,'little')*12)`
-- and it is the same lesson as the shield-write poisoning and the pinned
pools that made the 2026-08-20 motion result clean: **pin every input the
game's decision reads, not just the ones the answer is about.**

### What changed in the port

`ENEMY_FIRE_PCT_MIN 72 / SPAN 13` becomes **90 / 16**. This core's falloff is
`1 - d/12`, which is the binary's `1.5 - d/8` with the 1.5 divided out, so the
percentage has to carry it: 0.6 * 1.5 = 90, 0.7 * 1.5 = 105. The old 72..84
was 0.78 carrying the same 1.5 -- low by exactly the 0.8 the printed "Shields
absorb N" line also carries. **One instrument fault, two symptoms, both now
closed.** Two constants move MEASURED -> BINARY.

The band test asserted 661..771. Nine of these eighteen shots were at exactly
its range and came back 851.4 to 953.0 -- every one of them outside what the
suite was asserting. It now asserts 826..964, which brackets them.


## The death pod, read out completely (2026-08-28)

The full read is in NOTES.md. Three things in this file depended on the old
model and are corrected there and above:

1. **"Pods roam."** They do not. A pod is rolled fresh on each entry to a
   column-8 quadrant and never moves once placed; the
   detonation is a galaxy-wide roll whose damage lands on the quadrant you are
   in. Every reading in this file that CORRECTED for pod damage stays valid --
   a pod hitting us and every enemy for one figure is exactly what the binary
   does, and it is why the enemy's hit points measure the pod for us.
2. **"A pod had been through before we arrived."** Withdrawn as an inference.
3. **It can be killed, and killing it is worth nothing.** Lasers only, 900
   points, and 0x01E9BE jumps clear of the scoring. A torpedo aimed at it is
   answered with a cloaking device and spent. That asymmetry is worth knowing
   before the next fire run: **a torpedo run in a quadrant holding an 'R' will
   lose torpedoes to a target that cannot be damaged**, and nothing in the
   port's own message log would have explained it before today.


## THE ABSORPTION HALF OF THE 0.8, SETTLED (2026-08-28)

The shot half was settled on 2026-08-27 and the binary won. This is the other
half, and the binary wins it too -- to four decimal places.

**The discriminator is a RATIO**, so it needs no knowledge of hit points,
range, or the random band, all three of which defeated earlier attempts:

    S = shield-charge drop, E = energy drop, f = charge / 2500

    0.8 on the POOL DRAIN (binary)    S/E = 0.8f/(1-f)
    0.8 on the PROTECTION             S/E =     f/(1-f)

| charge | f | binary predicts | alternative | MEASURED |
|---|---|---|---|---|
| 1000 | 0.4 | **0.5333** | 0.6667 | **0.5333** (n=5) |
| 2000 | 0.8 | **3.2000** | 4.0000 | **3.1995..3.2003** (n=19) |

Twenty-four clean turns, every one within 0.0005 of the prediction, and the
alternative excluded by 25% at both charges. `SHIELD_ABSORB_NUM/DEN` were
already BINARY; they are now measured as well, and `core/test/test_trek.c`
asserts the two ratios by cross-multiplication. Three breaks confirm the test
can fail: moving the 0.8 onto the protection, removing it, and changing it to
3/4.

### The rig, and what it had to hold still

Null volley -- fire ZERO, which costs no energy and still gives the enemy its
turn -- so E is the enemy's shot and nothing else. Pinned every trial: the
twelve systems, both pools, the enemy's hit points, **the death pod DISARMED
at [0x26E0]** (a detonation takes 50..99 straight off the charge and would
have landed in S as noise), and **the number of firers**.

That last one was Jamie's, twice. Mongols wander in from neighbouring
quadrants -- four had gathered within a few turns, each pinned by the rig to a
full 355 hit points, so the board was getting stronger every turn. It is the
same fault as the decaying board that cost the shot half, running the other
way. Every slot but the first is zeroed now, so exactly one enemy fires.

### The outliers were a different WEAPON, and I nearly filed them as a timing bug

Three trials came back at 4.07, 8.50 and 8.62. Every one of them followed a
turn that read as "no shot", so they looked exactly like the documented settle
trap -- damage arriving late and landing on the next reading. That explanation
fits three samples and is wrong.

Jamie, watching the screen: *"mongol fires a plasma bolt"*.

Predicted before looking, and photographed: an outlier turn shows the plasma
bolt, a 3.20 turn does not. The captured outlier reads

    639 unit hit from plasma bolt.
    87 unit hit from Mongol at 8-8.

with E = 87.50 -- the ordinary shot's through figure exactly. **The plasma
bolt drained NO energy at all**; the shields took all of it, ~442 off the
charge for a printed 639. It is a separate weapon with a separate damage path,
already on record as 619 and 639 unit hits. BUILT 2026-08-28, later the
same day this was written.
One capture is not a law; what it establishes is that the ordinary-shot
measurement above is clean once plasma turns are excluded, and that the bolt
needs its own designed run.

### AND THE PRINTED FIGURE IS THE THROUGH VALUE, NOT absorbed x 0.8

This was the reason the run was scheduled: if the printed "N unit hit" carried
the 0.8, every screen-sourced absorption figure in this file's back catalogue
took a correction. **It does not.** With shields UP at charge 2000, the damage
report read "87 unit hit from Mongol at 8-8" and the energy drop was 87.50 --
the same number. The charge dropped 280 in that turn, and 0.8 x 280 is 224,
which is not what was printed.

So the back catalogue needs no correction, and a conflict is recorded rather
than resolved: the static read of 0x017077 in this file says
`printed = absorbed * 0.8` with the string "Shields absorb N unit hit from ".
No message of that wording appeared in twenty-four turns with shields up and
absorbing. Either that string belongs to a path these turns did not take, or
the site was mis-attributed. **The experiment that separates them:** capture a
turn whose printed figure is NOT equal to the energy drop, and read its
wording. Nothing in the port depends on the answer -- the port prints its own
messages -- so this is about the instrument, not the game.

### Confirmed in passing

**Life support at 0% swaps the console panel.** The captured outlier shows
"Life Support damaged. Now at 0%" and the SYSTEMS STATUS panel replaced by
"LIFE SUPPORT / RESERVE, DAYS" with a bar -- which is what `draw_reserve()`
does in this port, built from the reading rather than from a screenshot.


## THE PLASMA BOLT, read out completely (2026-08-28)

Jamie, reading the screen while an absorption run was producing numbers:
*"mongol fires a plasma bolt"*. It had been on the "seen but unmeasured" list
since 2026-08-20 as "a distinct enemy weapon, 619 and 639 unit hits, far above
anything the laser law produces". It is a great deal more than that.

**It is an OBJECT IN FLIGHT, not a shot.** `[0x1E28]/[0x1E2A]` are its sector,
`[0x1E2C]/[0x1E2E]` its quadrant, and it persists across turns. Leaving the
quadrant clears it (0x0165A7).

### Fired -- fn at 0x16CF4

    if ([0x1DF0] < 7)            no bolt    ; V = level+4, so LEVEL 3 AND UP
    if ([0x1E28] != 0)           no bolt    ; ONE in flight at a time
    if (Random(100) <= 93)       no bolt    ; 6 in 100, per enemy, per turn
    if (type != 3 && type != 4)  no bolt    ; battleships and Commanders only
    [0x1E28] = ship sector row + 1          ; aimed WHERE YOU ARE NOW
    [0x1E2A] = ship sector col + 1
    [0x1E2C], [0x1E2E] = the quadrant

Scouts and supply ships never fire one. The warning message is
"WARNING:  Mongol at R-C fires plasma bolt." -- its own line, before the hit.

### Detonated -- fn 0x1658F

    if (not our quadrant)     the bolt is cleared
    if ([0x26E1] != 0)        nothing            ; the PLASMA BOLT SHIELD
    d    = sqrt(dy^2 + dx^2)                     ; bolt to ship, 0x0165D3
    dmg  = (90 + Random(10)) * (8 - d)           ; 0x016601..0x01662B
    if (dmg < 0)              nothing            ; so eight sectors of reach
    SHIELD CHARGE -= dmg                         ; 0x0166AF, WHOLE
    "N unit hit from plasma bolt."               ; the same real, 0x0166C5

**It takes the charge whole.** No absorption split, no 0.8 -- the operator at
0x0166AF is the same call proved to be subtraction at 0x017064, where
`shields -= absorbed * 0.8` is built. Main energy is not touched at all.

**AIMED WHERE YOU WERE, WHICH MAKES MOVEMENT THE COUNTER.** The bolt takes the
ship's sector at the moment it is fired and the damage falls off with your
distance FROM THAT POINT, reaching zero at eight sectors. Sitting still takes
the full `90..99 x 8`; moving away is worth about a hundred points a sector.
That is a real tactical rule and it is the first one in this game that rewards
manoeuvring rather than shooting.

### Confirmed against the rig, on data taken before the routine was read

Five bolts landed during the absorption run and were solved out of the pool
readings on the model "the bolt lands first and takes the charge whole, then
the ordinary shot is absorbed at the REDUCED charge":

    solved damage   606  613  639  645  652        (screen record: 619, 639)

The photographed one solves to **638.8 against a printed 639** -- 0.03% -- and
639 / (8 - sqrt(2)) = **97.03**, where the roll is an integer 90..99. The law
reproduces the screen exactly at d = sqrt(2) with a roll of 97.

### Still unread, and it is small

The "Plasma bolt failed to detonate." branch, whether the detonation also
damages ENEMIES near it (NOTES has recorded since 2026-08-20 that it "can kill
several Mongols at once", which this routine does not obviously do), and how
`[0x26E1]` is set and spent -- it is the captured `ITEM_PLASMA_SHIELD`, and
"Raising plasma bolt shield..." at 0x097BA belongs to using it.

### ~~UNBUILT~~ BUILT 2026-08-28

`bolt_cell` / `bolt_quad`, fired in `trek_enemy_turn`'s fire loop and
detonated by `run_bolt` before it. It needed what the death pod needed -- a
persistent position and a per-turn roll -- plus the thing the core had never
had: an object belonging to NEITHER SIDE whose damage depends on where the
ship has moved to since it was created.

Verified on the C128: a bolt one sector away gave "DAMAGE: 651 UNIT HIT FROM
PLASMA BOLT.", shields 2500 -> 1849 and energy unmoved, against a predicted
band of 630..693.

**The roll comes BEFORE the type test** and this port had it the other way
round for a minute. The suite caught it on an unrelated MEASURED rule -- a
supply ship and a Commander with equal hit points must fire for equal damage,
and they stopped doing so the moment the stream depended on class.


## The warp engines break on DISTANCE, and the message says "speed" (2026-08-28)

`fn 0x0C609`, the MOVE routine, at 0x0D649..0x0D6DD. The constants decode
exact, which is itself a check on the decoding.

    d = sqrt(dy^2 + dx^2)                    ; 0x0CC?? , quadrants
    if (Random < (d - 1.5) / 2.5)  nothing   ; 0x0D649, 0x0D665, 0x0D672, 0x0D67A
    loss = Round(Random * d * 10 + 10)       ; 0x0D684..0x0D6B4
    [0x2364] -= loss                         ; 0x0D6C5
    if (this was a warp move)  print         ; 0x0D6C8, the message at 0x0D6D8

`[0x2364]` is the SIXTH word of the twelve-word system array at 0x235A --
index 5, which is `SYS_WARP` in this port. 1.5, 2.5 and 10.0 all decode exact.

So the risk starts at a distance of 1.5 quadrants and is a certainty past 4,
and the damage is 10 points plus up to ten per quadrant travelled. Warp is not
in it.

### Measured, and the measurement had to defeat the rig to happen

**Ten one-quadrant hops at WARP 8: zero damage.** The "speed" reading predicts
damage there -- warp 8 is the fastest there is -- and the "distance" reading
predicts none, because (1 - 1.5)/2.5 is negative. Ten for ten.

That also settles a question the read left open: **d is in QUADRANTS, not
sectors.** A one-quadrant hop is eight sectors, and eight sectors would give
(8-1.5)/2.5 = 2.6, damage every time.

The moves only landed because the sector map was poked to '.' first. Jamie
said "blocked by object" three times while this was being set up, and he was
right every time: a Mongol or a star in the path aborts the move, and 22 of
the 24 readings in an earlier attempt at this same mechanic were that -- with
the "system damage" being recorded actually being the blocking Mongol's fire.

### What is NOT measured, and why the rig cannot currently do it

The positive direction. A long move should ALWAYS damage the engines, and
that is read but not confirmed, because **multi-quadrant moves are reliably
blocked on this rig**: clearing the source quadrant does not help, since the
destination is built fresh around the arrival sector and blocks there. Three
attempts, three blocks.

**The experiment that would settle it:** pick a destination from the galaxy
chart whose word is 000 -- no enemies, no base, no stars -- and move there.
An empty quadrant has nothing to block with. The chart is at DS:0x2560, stride
16, and empty quadrants demonstrably exist (one was found at 8,3 in an earlier
session). **BUILT 2026-08-29 anyway, from the branches** -- and the read was
incomplete: a WARP >= 8 GATE that the first pass missed, which is also why
three rig attempts could never have shown anything.


## What HAIL schedules: the StarBase's reply is DELAYED (2026-08-28)

The last thing `fn 0x207FD` does, and the last entry on the read list. Slot
[0x1D78] is the StarBase's ANSWER, arriving after travel time:

    v = 8.0                                   ; 0x020816, the "none found"
                                              ; sentinel -- and it is the
                                              ; port's HAIL_START_Q 64, squared
    for each StarBase in the box:  v = min(v, sqrt(dy^2 + dx^2))   ; 0x020958
    if (v >= 8.0)   nothing is scheduled      ; 0x02002C..0x020A31
    [0x1D78] = stardate + (v - 1.0) * 0.5     ; 0x020A65

and when the clock reaches it, at 0x0206AD:

    "COMMUNICATIONS:  The StarBase in R-C is responding to our hail."
    [0x1D78] = 9999.0                         ; 0x02071D, back to never

**So a hail is a radio call with a light-lag.** The immediate reply only says
the frequencies are open; the base's actual response arrives half a stardate
per quadrant of distance later, minus a half. A base two quadrants off answers
in 0.5 stardates; one at the edge of the box takes about 2.3.

The 8.0 sentinel doubles as the range gate, which is why nothing is scheduled
when no base was found -- v is still 8.0 and the compare fails. This port
already carries that number as `HAIL_START_Q 64`, in squared form, from the
2026-08-27 read; the constant was right and its second job was not known.

**Unbuilt.** It wants a third schedule slot, an EV_ kind and one message, and
it is the only one of the original's eight slots this core has a use for and
lacks.

## The MAIN VIEWER, read end to end (2026-09-02)

Read out of the binary with `tools/dis16.py`, not sampled. It answers the one
thing that has been carried as unread since 2026-08-26 -- **what cycles the
pages** -- and it OVERTURNS what this repository has been saying about the
orbit page. Every address below is a raw file offset.

### The pages, and how each routine was named

The nine page titles sit in the CODE segment as Turbo Pascal length-prefixed
strings, each immediately followed by the procedure that uses them -- so the
routines are named by the strings before their prologue, not by a guess. The
segment base is **0x022070** (seg 0x1D27), solved from far calls landing in it.

    page  routine   named by the strings before it
     0   0x022CF9   GRAV FIELD 502
     1   0x022DD0   ARRAY MONITOR 504     (ONLINE/OFFLINE, three rows)
     2   0x022EF0   STRUC INTEGRITY 505   (four-row table)
     3   0x02322E   *** UNTITLED ***      ENG:10:0, LEFT/RIGHT/FRONT/REAR,
                                          FUNC1-2, SCAN1-5, AUX1-2, DC00-DC22
     4   0x022C83   SHIP STATUS 501
     5   0x022A1A   POWER DISTRIB 509     (PMAX/PAVL/PPCT, three pools)
     6   0x022773   ENG:99:00 / System
     7   0x0225CD   SPACE COMM NET 411
     8   0x022892   STANDARD ORBIT 301    (APOGEE:/PERIGEE:)
     9   0x022FEB   PLANET LIST 601/602   -- two titles, ONE routine

**TEN pages, not nine.** Page 3 has no title in the string run and was
therefore missing from the 2026-08-26 list, which counted titles. It is the
largest page of the set: twelve rows of a two-column engineering readout.

### The dispatcher, and the answer: it is RANDOM

`fn 0x023FD2` (far, CS 0x1D27:0x1F62) is the viewer. Its page dispatch is a
plain `cmp ax, 0..9` chain at 0x024A98-0x024AF2, and `ax` comes from:

    0x024A6C  mov al, [DS:0x1DDA]        ; the page-force variable
    0x024A70  mov di, 0x1F42             ; a 32-byte TP set
    0x024A75  <set membership test>
    0x024A7A  je  -> Random(10)          ; NOT a member: pick at random
    0x024A7C  mov ax, [DS:0x1DDA]        ; a member: use it

The set at CS:0x1F42 dumps to `ff03` followed by thirty zero bytes -- bits 0
to 9, **exactly the ten pages**.

**[DS:0x1DDA] is initialised to 99** (`mov word [1DDA],99` at 0x027CDB), which
is not in the set. So the default, and the state of a fresh game, is **a fresh
Random(10) on every redraw**.

**The player forces a page by TYPING ITS NUMBER.** The only other write to
[0x1DDA] is at 0x005E3E, in the command-line handler: [DS:0x2131] holds the
upcased first character of the typed command, and 0x005DFA-0x005E23 compares it
against '0' through '9' -- all ten jump to the same block, which `Val`s the
command string into [0x1DDA]. A page number is a COMMAND, alongside M, W, L
and the rest.

This is a complete read, not a lower bound: the immediate `0x1DDA` occurs at
**four** places in the whole 338KB file, and all four are accounted for above.

### What triggers a redraw -- and the 5.9-second timer

`fn 0x023FD2` is called from exactly three sites:

  * **0x00590B and 0x0059E9**, both in the command loop, both gated
    `cmp byte [DS:0x26D4],0 / je`. [0x26D4] is a DIRTY FLAG: eleven sites set
    it to 1 (0x0058CC, 0x00ACA5, 0x00AF9B, 0x00D19F, 0x00D9AC, 0x017AED,
    0x017F86, 0x01816D, 0x01889A, 0x01EB3D, 0x027C88) and the draw itself
    clears it at 0x024B1B.
  * **0x01764F**, inside the KeyPressed poll loop, gated
    `cmp word [DS:0x1CC4], 0x6C / jl`.

[0x1CC4] is zeroed at the TOP of the draw (0x023FE3) and **incremented by the
timer ISR** at 0x024B78 -- the same ISR that plays the music, which this file
already records as the stock **18.2065 Hz** timer, never reprogrammed, one tick
54.9ms (see "DGROUP, derived twice"). So:

    108 ticks x 54.9ms = 5.93 seconds

**The viewer re-rolls its page every ~5.9 seconds while it waits for you to
type**, and immediately whenever an event dirties it. Sitting still, the
instrument display keeps changing on its own.

### The alternation the manual describes is [DS:0x26D5]

The manual, on the main viewer: *"This display alternates between a view from
outside the ship and a graphical display of some ship function."* That is one
byte. [0x26D5] is toggled at the end of EVERY draw (0x024B18), and read at
0x024047 to choose:

    [0x26D5] == 0  ->  0x024060, the outside view (2.5K of silhouette code)
    [0x26D5] != 0  ->  0x0240C0 -> 0x024A35, clear the panel, dispatch a page

So the outside view and an instrument page take turns, and the page is a fresh
random draw each time its turn comes round.

There is also a fast path at the head (0x023FE6-0x023FFB): not dirty, [0x26D5]
and [0x26D2] both set, and it simply blits the cached image from the far
pointer at [0x26C8] instead of drawing anything.

### THE MUTANTS TAKE THE VIEWER -- [DS:0x26D1]

Before the page dispatch is reached:

    0x024A4D  cmp byte [DS:0x26D1], 0
    0x024A52  je   -> the ten instrument pages
              ; otherwise:
    0x024A54  Random(2) -> fn 0x022506 or fn 0x0223BE, then done

Those two routines have **no strings at all** and are called from nowhere else
in the file. [0x26D1] is set to 1 at **0x00721D**, immediately after the death
ray's `...half the crew has turned into some kind of mutants!` and
`The apparatus is going unstable` strings.

So a mutant crew TAKES OVER THE MAIN VIEWER: the ten instrument pages stop
appearing and one of two other displays is shown at random instead. It is not
cosmetic and it is not temporary-looking -- **[0x26D1] is one of five booleans
written to the save file.**

**Proof of that last claim, and it is exact.** The save reader at 0x0087D4
through 0x0088C4 does the same thing five times: read a string, compare it
against the Pascal constant `TRUE` at CS 0x011C:0x218C, set a flag byte. In
order the five flags are [0x26D6], [0x26D9], [0x26DA], [0x26DC], [0x26D1].
`reference/EGATREK.SAV` has exactly five TRUE/FALSE lines, 239-243, reading
FALSE FALSE FALSE TRUE FALSE. Five readers, five lines, same order.

While the mutants hold the ship, 0x005B1A gives them a **1 in 10 chance per
turn of ending** (`Random(10)` = 0 clears [0x26D1]), and either way one of five
random messages is printed.

### CORRECTION: nothing selects the orbit page on orbit

This repository has been recording, in `c128/src/ui.c` and in NOTES.md, that
*"STANDARD ORBIT is the page it shows while orbiting"* and that *"orbit selects
301"*. **Both are wrong.** [0x1DDA] is written from the command line and
nowhere else; entering orbit does not touch it. STANDARD ORBIT is page 8 and is
reached at random like any other.

What actually happens is the reverse, and it is neater. `fn 0x022892`, page 8,
opens:

    0x0228A1  cmp byte [DS:0x26D9], 0     ; the "in orbit" flag -- the SECOND
    0x0228A6  jne -> draw the orbit display   of the five saved booleans
    0x0228A8  jmp 0x0229CC                ; not in orbit:
    0x0229CC  call 0x022773               ;   fall through to PAGE 6

**Page 8 falls back to page 6 when there is no orbit to show.** The page is
picked at random; it decides for itself whether it has anything to say.

The port is not wrong to draw the orbit page while orbiting -- with one page
and no cycling that is a reasonable adaptation -- but the claim that it is what
the ORIGINAL does was never read, and it is not true.

## The landing party's attack, read from the branches (2026-09-02)

`fn 0x0E3A1` is LAND. The attack roll was read on 2026-08-27 -- the odds and
the casualty count were right -- but what happens AFTER it was described from
the ORDER of the blocks rather than from the jumps, and both readings that came
out of that are wrong. Traced properly:

    0x00E3BE  settlers: quadrant == [0x1E1C]/[0x1E1E] and [0x1DA2] > stardate
    0x00E432    jmp 0xE4EB  -> THE EPILOGUE. A rescue skips everything below.
    0x00E435  Random(5); or ax,ax; jne 0xE48F     -> 4 in 5, on to the find
    0x00E442    "Landing party attacked..."       CS:0x503E
    0x00E455    Random(5) + 2                     -> 2..6
    0x00E464    Str(n) + " casualties."           CS:0x5058
    0x00E484    [0x1DDE] += n
    0x00E48D    jmp 0xE4DD  -> THE COMMON TAIL. The find is never reached.
    0x00E48F  planet[q] > 20 -> supply capture (fn 0x181E8), jmp 0xE4DD
    0x00E4B7  planet[q] > 10 -> "energium successfully mined.", [0x2350]++
    0x00E4D3  else              "Nothing found."
    0x00E4DD  the tail both outcomes and the ATTACK reach
    0x00E4EE  ret

The four strings check out against the segment base solved from the routine
itself -- CS:0x500E, 0x5027, 0x503E and 0x5058 land on "Planet settlers
found...", "Evacuating settlers...", "Landing party attacked..." and
" casualties." Four for four, so the block is named by the strings it PUSHES.

### First correction: AN ATTACKED PARTY BRINGS BACK NOTHING

`jmp 0xE4DD` at 0x00E48D is the whole finding. 0xE4DD is where the supply
capture and both find messages converge, and the routine returns eleven
instructions later. The port had it falling through, so a mauled landing party
still mined its energium.

**This is the third time on this project that reading ORDER instead of BRANCHES
has shipped a wrong model** -- after HAIL's gating and warp damage's speed
gate. The blocks really do sit in that order; only the jumps say what runs.

### Second correction: EVERY LANDING SPENDS THE FIND

`fn 0x0E3A1` has two callers, 0x00E843 and 0x00E8CF -- the transporter and the
shuttle. Both follow the call with the SAME unguarded sequence:

    call 0xE3A1
    ...                                     ; a message, 0.2 stardates, a flag
    [0x24A9+q] = [0x24A9+q] mod 10          ; 0x00E88F and 0x00E905

Nothing branches around it. The `mod 10` strips the tens digit, which is
exactly `PF_TAKEN`, so the find is spent on every landing without exception --
energium mined, supplies captured, settlers evacuated, and a party that was
ambushed and came back with none of it.

That kills the note in `core/planet.h` that said a Mongol station "is not
cleared by being walked into" because 0x00E4A3 "calls the capture and RETURNS".
The return is real and it is not the end of the landing.

### And there is no "already landed" case

A second landing on a picked-clean planet runs the whole routine again: the
attack is rolled, and `planet[q]` -- now 1..3 -- falls past both tests to
"Nothing found.". `LAND_ALREADY` is this port's own message and it is fine, but
it must be returned AFTER the roll or going back for a second look becomes
free, which in the original it is not.

### What the port does now

`trek_land()` returns `LAND_ATTACKED` immediately, sets `PF_TAKEN` on every
path including the rescue and the Mongol station, and checks the spent flag
after the roll rather than before it. Six assertions in `core/test/test_trek.c`
pin it, each confirmed by deleting the line it protects:

  * removing the early return fails "a landing party can be attacked on an
    energium world"
  * removing the station's PF_TAKEN fails "the station is spent"
  * moving LAND_ALREADY back to the top fails "a landing party can still be
    attacked going back"

The energium world is deliberate: on a barren world LAND_ATTACKED and
LAND_NOTHING differ only in the return code, and a test that cannot tell the
two mechanics apart is not testing one.

## Clearing the unread list with the disassembler (2026-09-02)

Six items were on it wanting a read. Four were genuinely unread and are below.
**Two were not unread at all** and are dealt with at the end.

### The settlement's evacuation deadline -- ARMED, then SET

`[0x1DA2]` is the deadline and `[0x1D9C]` its schedule slot, both Turbo Pascal
reals. At `trek_new_game` (0x027BFE) the slot is 9999 and the deadline is 0.

**Arming, at 0x005876 in setup:**

    cmp word [0x1DF0], 7 / jl  -> skip entirely      ; V >= 7, so level 3+
    [0x1D9C] = 3505.0 + Random() * 3.0

3505.0 decodes from `cx=0x008C si=0x0000 di=0x5B10` and 3.0 from
`cx=0x0082 si=0x0000 di=0x4000`; STARDATE_START is 3500, so the request is
armed five to eight stardates into the mission and **only at command level 3
and above**. Below that a galaxy's settlement is never heard from.

**Firing, fn 0x151D0, once per turn:**

    al = [0x1E1C]                      ; the settled planet's quadrant ROW
    if al not in {1..8}: return        ; the 32-byte set at CS:0x0096
    if stardate <= [0x1D9C]: return
    [0x1D9C] = 9999                    ; so this fires EXACTLY ONCE
    [0x1DA2] = stardate + 1.0 + Random() * 3.0
    "<planet>, quad r-c, requests evacuation. They can only hold out until <x>"

So the deadline is one to four stardates after the request, which the routine
map already had; **what was unread is the arming**, and it carries a command
level gate that nothing in this port has.

Three sites set the slot back to 9999 and clear `[0x1E1C]`: the rescue itself
(0x00E416) and the two supernova routes (0x00AAE9, 0x01EDA8).

### The plasma bolt: one in three is a dud

fn 0x09563 is USE PLASMA BOLTS. It asks "Sector to fire at: ", decrements
`[0x234D]` (item 2), prints "Tracking...", and then:

    Random(3); or ax,ax; jne  ->  "Plasma bolt detonates..."
    "Plasma bolt failed to detonate."   and RETURN

**A third of the bolts you fire do nothing at all.** The strings resolve at
CS:0x01D9 and CS:0x01F9 against a base of 0x009310, which also lands CS:0x01B7
on "Sector to fire at: " and CS:0x0237 on " unit hit from plasma bolt." -- four
for four, so the block is named by the strings it pushes.

### YES, A BOLT HURTS EVERY ENEMY NEAR IT -- and it can hurt you

NOTES has claimed since 2026-08-20 that a bolt "can kill several Mongols at
once", and the claim has been carried as unverified ever since. It is right.

A detonation calls `fn 0x01EB48` with (1000, x, y), and that routine is a loop
over the enemy table: index from 1, stride 6, `[i*6 + 0x25F0] > 0` for alive,
`[i*6 + 0x25EC]` and `[i*6 + 0x25EE]` for the coordinates, differenced against
the target and converted to reals. A distance of exactly zero is replaced by
**0.5** (`cx=0x0080`), which is the divide-by-zero guard and tells you the law
is a falloff.

And after the enemies, at 0x009641, the SAME routine measures the bolt against
`[0x1DEA]`/`[0x1DE8]` -- the ship's own sector. **Your own plasma bolt can hit
you**, and the only thing that stops it is the shield below.

### The plasma bolt shield is NEVER SPENT

`[0x26E1]`. fn 0x97D7 is the whole of USE plasma-bolt-shield: print "Raising
plasma bolt shield...", `[0x26E1] = 1`, return. Nothing else in the binary
writes it except the initialiser at 0x027BB2, which writes 0.

Both bolt paths test it and jump clean past the damage -- 0x0165C9 for an
enemy's bolt and 0x009641 for your own. So it is not a charge that absorbs one
hit: **raise it once and you are immune to plasma bolts for the rest of the
game**, including your own.

### The two that were not unread

**`[DS:0x235A + idx*2]`, "population or countdown".** Neither. `DS:0x1188` is
the **twelve ship system NAMES** -- dumping it prints EnergyConverter, Shields,
Life Support, Lasers, EnTorp Tubes, Warp Engines, Impulse Engine, S.R. Scanner,
L.R. Scanner, Computer, Transporter, Shuttlecraft -- and `DS:0x235A` is their
twelve repair percentages. fn 0x15C07 is the SPY ("SECURITY:  A spy has been
captured aboard the Lexington, but he has damaged the <system>."), one turn in
150 at V > 7, taking `10 + Random(90)` off one system at random.

**All of that was retracted on 2026-08-27** in "RETRACTED: fn 0x15C07 is the
SPY, not the distress signal". The question survived because the retraction did
not reach the paragraph that raised it -- the same failure, in the same file,
as the twelve-entry-table claim fixed earlier today.

**"Whether laser heat decays over time is unmeasured"**, in `core/trek.h`. It
is measured, and it is built twelve lines from the comment saying otherwise:
`LASER_HEAT_COOL_TURN` 20 is the `sub ax, 0x14` at 0x0059BC in the command
loop, floored at zero three instructions later; `LASER_HEAT_COOL_DAY` 360 is
0x0201B3; and `HEAT_PER_UNIT` 15 is the `idiv cx, 0xF` at 0x009EF3 that adds
`fired / 15` to `[0x1DFC]`. All three are BINARY in trek.h already.

## [0x1F31] is the RESTORED-GAME FLAG, and the 'Y' survives (2026-09-02)

Written the same day as the read above, which closed the question of WHICH
setup answer the byte holds and left its lifetime open. Both are settled now,
and the second half corrected the first half's framing.

### The lifetime: only ESC resets it

The suspicious write was a string assignment at 0x014E7A into DS:0x1F30, the
same buffer the answer lives in. It assigns **CS:0x44B4, the one-character
Pascal string "N"** (0x01 0x4E), and it is on the **ESC-abort path only** --
`0x014E61 jne 0x014EAE` sends every non-ESC case past it.

Its purpose is not the turn loop at all. It is the test at 0x014FD6: a 'Y'
there skips the name, command level and password questions, because a restored
save already holds them. An ABANDONED restore must still ask them, so the byte
is put back to 'N'.

Everything else leaves it alone:

  * the file-name prompt reads into **DS:0x2030**, a different buffer
  * "*File Not Found*" (0x014EFC) and "*Wrong File Type*" (0x014F8C) both loop
    back to the file-name prompt via `0x014FD3 jmp 0x014E1D` -- they never
    leave the restore branch, so neither can exit with a stale 'Y'
  * the whole binary contains exactly TWO writes to [0x1F31]: the upcased
    answer at 0x014E03 and the turn loop's 'N' at 0x0059CE

**So a successful restore reaches the turn loop with 'Y' still in the byte.**

### And it is not a "free first turn"

`core/trek.h` described this as "a 'Y' suppresses the enemy's FIRST turn",
which is one consumer out of four. `[0x1F31] == 'Y'` is simply **THIS IS A
RESTORED GAME**:

    0x005177   jmp 0x5652      skip ~1,200 bytes of GALAXY GENERATION
    0x005824   call 0x008151   LOAD THE SAVE -- and that is the routine
                               holding the five TRUE/FALSE flag reads at
                               0x0087D4..0x0088C4; no prologue lies between,
                               so it is one routine
    0x0058A6   skip fn 0x015F51, the QUADRANT ENTRY
    0x005910   skip THE ENEMY'S FIRST TURN
    0x0059CE   force 'N' -- everything above is normal from turn two

A flag named by ONE of its four uses reads as a bonus rule; named by all four
it is bookkeeping with one side effect the player can feel. The port already
does the first three structurally -- it does not regenerate on restore, and
`ui_setup()` returns straight to the console -- so the only unbuilt part is the
enemy's first turn, and `Setup.restored` is already the flag for it.

## Message colour is PER SITE, not per department (2026-09-02)

Read from the binary and confirmed on the rig, which is the only reason to
trust it: the two methods were run independently and agree twice.

### The mechanism

There is no department colour table. Each message site calls BGI `SetColor`
(fn 0x029960, the immediate in the `mov ax, N` before it) and then the message
routine. The message routine `fn 0x021CC2` adds exactly one rule of its own, at
0x021CEF: it reads the current colour back and, **if it is 10, uses 15
instead** -- light green is bumped to white and nothing else is touched.

Attributing the 145 message call sites to the nearest preceding `SetColor`
gives **ten distinct colours in use** (1, 2, 3, 4, 6, 7, 11, 13, 14, 15). A
four-way department map cannot produce that.

### Two readings, each confirmed on screen

**NAVIGATION.** Four sites in the 0x0451 segment (base 0x009310, solved on far
calls and checked against three ORBIT strings) each carry `SetColor(3)` three
to fifteen bytes before the push:

    00E307 -> 00E310   "NAVIGATION: Not adjacent to planet."
    00E967 -> 00E970   "NAVIGATION: Not orbitting a planet"
    00F105 -> 00F114   "NAVIGATION: Docked." + the COMMUNICATIONS line
    00F178 -> 00F181   "NAVIGATION: Not adjacent to base"

Driven on the rig to quad 7-2 and typing O with no planet adjacent, the box
that appears samples as **1110 pixels of EGA 3 and nothing else but black**.
Read and capture agree.

**COMMUNICATIONS, and it is NOT yellow here.** `fn 0x207FD`'s region holds
exactly one SetColor, **7** at 0x020983. HAIL with no base in range paints a box
that samples as **1134 pixels of EGA 7 and nothing else but black**. Agreed
again.

That does not contradict the earlier capture-derived "COMMUNICATIONS is
yellow" -- it is what per-site colour MEANS. One department's messages are
different colours at different sites, and a capture can only ever show the
sites that happen to be on screen.

**DAMAGE REPORT** samples as EGA 6 brown on the panel's blue, which is what the
port already has.

### What this means for the port

`dept_color()` in `c128/src/ui.c` maps 'D' to brown, "COMM" to yellow and
everything else to green. It is a four-way map of a thing that is not a map,
and its "everything else" bucket is where NAVIGATION lives -- so **every HELM
message in this port is green and should be cyan**, which is the most common
message class in the game. NOT CHANGED in this commit; recorded.

The honest options are a per-message colour in the string table, or keeping a
department map and choosing its colours from the commonest site per department.
The second is a deliberate simplification and should say so.

### Free on the way past: the viewer read confirmed on screen

The ORBIT capture caught the MAIN VIEWER showing **ARRAY MONITOR 504** with its
ONLINE rows -- one of the ten instrument pages read this morning, drawn on a
turn nobody asked for it. That is the `Random(10)` selection working, seen
rather than inferred.
