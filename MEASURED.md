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
or scouts are 255. Unresolved.

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

**Open item 15, are hit points fixed per class** -- YES, and every class is now
measured: Commander 695, Battleship 355, **Scout 255**, Supply 120. The scout
figure came from the INFO panel naming the ship, and it removes the last
unmeasured hit-point constant.

Both readings that had ever looked anomalous are accounted for. **255 was a
scout at full strength**, not a damaged battleship as this file previously
guessed. The scattered values -- 610, 535, 345, 320, 230 -- are Vandal Death
Pod damage, which hits every ship in a quadrant by the same amount and roams
between them, so any enemy may already be carrying some when we arrive. That
mechanism was watched directly: four enemies reading 626, 286, 286, 286, each
exactly 69 below their class figure.

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

### New mechanic: warp speed can break the engines

Setting warp 8 and moving printed:

    Warp engines damaged by excessive speed.

No time passed and the ship did not move. Warp 4 then worked. So high warp
carries a damage risk that nothing in trek.h models, and the ship's own Warp
Engines entry showed damage afterwards.

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

**That aftermath is a screen this port does not have**, and it is not the
ordinary Detailed Evaluation:

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

ONE SAMPLE. Which of the four outcomes is how likely is not measured, and the
ancestor's failure table is a hypothesis for that, not an answer.

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

### The big one: eleven of twelve systems take damage that does NOTHING

The manual specifies an effect for every system. The port models all twelve
repair percentages, draws them on SYSTEMS STATUS, and repairs them -- and
`sys[SYS_CONVERTER]` is **the only one any rule consults**. Everything below is
documented and unimplemented:

| system | documented effect |
|---|---|
| Warp Engines | max warp = 1 + 0.09 x pct |
| Impulse Engines | below **50%** they simply stop |
| Lasers | pct is directly the fraction of energy converted to damage -- 100% does twice the damage of 50% |
| EnTorp Tubes | **100% = three tubes, 67-99% = two, 34-66% = one** |
| Short Range Scanners | above 90% full; **below 90% cannot see anything smaller than a star**; below 50% dead |
| Long Range Scanners | **below 100% cannot detect enemy ships**; below 50% dead |
| Computer | chart entries can be lost and need re-scanning; **automatic navigation needs 100%** |
| Life Support | must be 100% to make food and oxygen; without it the ship lasts **two days** on reserves |
| Transporter | must be **100%** to use |
| Shuttlecraft | must be **100%** to use, and takes **0.2 stardays** round trip |
| Shields | pct is how efficiently shield energy becomes actual shielding |

This is the largest single body of specified-but-unbuilt work in the project,
and none of it needs an emulator to settle.

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
STANDARD ORBIT is the page it shows while orbiting a planet -- which is a
mechanic, not scenery. And PLANET LIST needs TWO pages, so whatever the
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
the law is it is not simply proportional to charge. **Unresolved.**

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
