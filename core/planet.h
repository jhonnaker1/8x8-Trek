#ifndef PLANET_H
#define PLANET_H

#include <stdint.h>
#include "trek.h"

/* Planets, and the ORBIT / LAND / USE chain that hangs off them.
 *
 * A SEPARATE MODULE from trek.c for the reason serial.c is one: the linker
 * takes whole modules, so a port that never builds the planet chain should
 * not have to carry it. trek.c calls exactly two things here -- planet_new()
 * when a game starts and planet_place() when a quadrant is built -- so the
 * dependency runs one way and is two symbols wide.
 *
 * The core still formats no text. Every message the original prints for this
 * chain is in reference/strings.txt and is the UI's business; what is here is
 * the model, the gates and the outcomes.
 */

/* --------------------------------------------------- the list, not a byte
 *
 * MEASURED 2026-08-21 off the MAIN VIEWER's PLANET LIST page:
 *
 *     5-4N Gallista-5      6-4N Cygnus-6      6-4M Gallista-6
 *     7-1N Andromeda-7     7-6O Sigma-7
 *
 * QUADRANT 6-4 HOLDS TWO OF THEM. That rules out the obvious model -- one
 * byte per galaxy cell, beside gal_stars and gal_base -- and it is why this
 * is an array of records with a quadrant in each rather than an array
 * indexed by quadrant. The original plainly keeps a list too: it has a page
 * that prints one.
 */

/* DERIVED from the ancestor's setup.c, which rolls its uninhabited planets as
 * MAXUNINHAB/2 + (MAXUNINHAB/2+1)*Rand() with MAXUNINHAB = 10 -- five to ten.
 * The one EGA Trek galaxy whose PLANET LIST we have read held FIVE, which is
 * that range's exact minimum. That is the same shape of evidence that settled
 * the enemy count (see ENEMY_BASE in trek.h): a reading landing on a model's
 * endpoint is worth more than one landing in its middle.
 *
 * The ancestor's other planet population, its NINHAB inhabited worlds, is
 * NOT reproduced. That is a GALSIZE*GALSIZE/2 flood of class-M worlds behind
 * an option flag, and EGA Trek shows no sign of it: seven names in the whole
 * binary, a list page that fits on two lines, and a settlement that is a
 * per-planet find rather than a category of planet. */
/* REFUTED 2026-08-26, the same evening it was written, by a photograph of the
 * original's PLANET LIST page:
 *
 *     5-1N Andromeda-5     6-5M Gamma Regula-6   7-4N Gallista-7
 *     5-4N Gallista-5      6-7N Vega-6           7-8N Xevious-7
 *     5-5M Gamma Regula-5  7-2N Ceti Alpha-7     8-1M Andromeda-8
 *     5-6O Sigma-5         6-2O Ceti Alpha-6
 *     PLANET LIST 602
 *
 * ELEVEN PLANETS ON ONE PAGE, and that is page 602 of two -- the binary
 * carries PLANET LIST 601 as well, holding the quadrants below 5-1, since the
 * list is sorted by quadrant. So the galaxy holds at least twelve and at most
 * twenty-two, against a model that said five to ten. The ancestor's
 * uninhabited-planet count was the wrong population to derive from.
 *
 * PROVISIONAL and deliberately generous: twelve to twenty-two spans what the
 * evidence allows, and PLANET_MAX covers the top of it. Page 601 has not been
 * captured -- the viewer cycles its pages on its own and 601 did not come
 * round -- and one photograph of it would settle the count exactly. That is
 * on the open list in NOTES.md.
 *
 * Cost of being generous: six bytes of RAM and six of save file per slot. */
#define PLANET_MIN       12
#define PLANET_SPREAD    11      /* trek_rand_n(11) gives 0..10, so 12..22 */
#define PLANET_MAX       22

#define PLANET_NONE    0xFF      /* "no planet" / "not orbiting one" */

/* THE DIGIT AFTER THE NAME IS THE QUADRANT ROW, and it is not part of the
 * name. Seven samples from three separate sessions, seven agreements:
 *
 *     Gallista-5   quad 5-4        Cygnus-6     quad 6-4
 *     Gallista-6   quad 6-4        Andromeda-7  quad 7-1
 *     Sigma-7      quad 7-6        Gallista-8   quad 8-4
 *     Xevious-8    quad 8-8
 *
 * So a planet stores a name INDEX and nothing else about its name; the
 * printed form is planet_name[i], '-', and the 1-based quadrant row. It is
 * also what makes the duplicates in that list legible -- Gallista-5 and
 * Gallista-6 are two different planets sharing a name, told apart by the row.
 *
 * EIGHT names, and getting that number wrong is the cautionary tale attached
 * to this file. The list was first taken from reference/strings.txt, which
 * shows seven -- and `strings` had silently dropped VEGA. The binary holds a
 * fixed-stride table, 13 bytes per entry, length-prefixed Pascal strings:
 *
 *     \x09Andromeda\0\0\0   \x0aCeti Alpha\0\0   \x06Cygnus\0\0\0\0\0\0
 *     \x08Gallista\0\0\0\0   \x0cGamma Regula     \x05Sigma\0\0\0\0\0\0\0
 *     \x04Vega\0\0\0\0\0\0\0\0  \x07Xevious\0\0\0\0\0
 *
 * Alphabetical, stride 13, longest name twelve characters. The same block
 * shows why strings.txt cannot be trusted for a TABLE: two entries below it
 * come out joined as "Life support suppliesRaw energium", with no separator
 * at all. It is fine for finding a message and wrong for counting a list.
 *
 * The port shipped seven for a day, so it could never name a planet Vega and
 * named one Xevious wherever the original would have said Vega. Found by
 * Jamie photographing the original's PLANET LIST page -- the same way the
 * truncated command list was found. Read the BINARY for tables.
 *
 * An earlier note in NOTES.md also transcribed one name off a screen capture
 * as "Gallisto"; the binary says Gallista. The binary wins twice. */
#define PLANET_NAMES      8
extern const char *const planet_name[PLANET_NAMES];

/* Planet class, the Star Trek convention, printed as a letter with no space
 * between the quadrant and the name: "6-4M Gallista-6".
 *
 * MEASURED: M, N and O all appear in the lists we have. DERIVED for the
 * distribution -- the ancestor rolls `Rand()*3.0`, a flat third each.
 *
 * CONTESTED 2026-08-26: the eleven planets of PLANET LIST 602 read six N,
 * three M and two O. A flat third would expect 3.7 each, and six N against
 * that is not damning on eleven samples -- but it leans, and it leans the way
 * a weighted roll would. Page 601 doubles the sample for free once captured.
 * Left flat until then rather than fitted to eleven planets.
 *
 * The class is DISPLAY ONLY here. Whether EGA Trek gives it a mechanical
 * meaning is UNMEASURED: the one class-O planet we watched had energium on
 * it, which is one sample of a correlation that may not exist. If it turns
 * out class gates the find, this is where that would go. */
#define PCLASS_M          0
#define PCLASS_N          1
#define PCLASS_O          2
#define PCLASS_COUNT      3
extern const char planet_class_letter[PCLASS_COUNT];

/* What ORBIT's scan finds, and what LAND then collects. These are the four
 * SCIENCE lines in the binary, in the binary's own order:
 *
 *     "Scanners show a [destroyed ]settlement on the planet."
 *     "Scanners show a Mongol supply station on planet."
 *     "Scanners indicate the presence of energium on planet."
 *     "Scanners indicate nothing of interest on planet."
 *
 * "destroyed " is an INSERT into the settlement line, not a fifth find --
 * which is why a ruined settlement is a flag below rather than its own kind.
 * A settlement is destroyed by the same thing that destroys a base: an
 * evacuation deadline nobody met. That event is NOT built here; see the note
 * on PF_RUINED. */
#define PFIND_SETTLERS    0
#define PFIND_MONGOL      1      /* a Mongol supply station */
#define PFIND_ENERGIUM    2
#define PFIND_NOTHING     3

/* Energium on ONE PLANET IN THREE, DERIVED from the ancestor: its setup rolls
 * `crystals = Rand()*1.5`, which floors to present exactly a third of the
 * time, and its own comment says so. The other three finds split the
 * remaining two thirds, and THAT SPLIT IS PROVISIONAL -- invented to give
 * each of the binary's four SCIENCE lines something to print. Nine outcomes
 * so the measured third stays exact:
 *
 *     0,1,2  energium     DERIVED, one in three
 *     3,4    settlers     PROVISIONAL
 *     5      Mongol       PROVISIONAL -- the rarest, since it is the one
 *                         that costs a landing party
 *     6,7,8  nothing      PROVISIONAL
 *
 * A single PLANET LIST page cannot settle this; the finds are not on it. What
 * would is one survey orbiting every planet in a galaxy and reading the four
 * SCIENCE lines off, which is a cheap DOSBox session and is now on the list
 * in NOTES.md. */
#define PFIND_ROLL_OF_N   9
#define PFIND_ENERGIUM_TO 2      /* rolls 0..2 */
#define PFIND_SETTLERS_TO 4      /* rolls 3..4 */
#define PFIND_MONGOL_TO   5      /* roll  5    */

#define PF_SCANNED     0x01      /* ORBIT has revealed `find` to the player */
#define PF_TAKEN       0x02      /* the find has been collected or evacuated */
/* A settlement that was not relieved in time. Nothing sets this yet: the
   scheduled evacuation event ("Planet Gallista-8, quad 8-4, requests
   evacuation. They can only hold out until 3516.5.") is a separate job, the
   twin of SCHED_BASE_ATTACK. The flag and the ruined-settlement scan line
   exist so that job is a schedule slot and a message, not a model change. */
#define PF_RUINED      0x04

typedef struct {
    uint8_t quad;      /* galaxy cell, 0..63 -- fixed for the game */
    uint8_t sec;       /* sector cell, 0..63 -- chosen when the quadrant is
                          built, and stored rather than recomputed so that a
                          game saved in orbit reloads in the same orbit */
    uint8_t name;      /* index into planet_name[] */
    uint8_t cls;       /* PCLASS_* */
    uint8_t find;      /* PFIND_* */
    uint8_t flags;     /* PF_* */
} Planet;

extern Planet  planets[PLANET_MAX];
extern uint8_t planet_count;

/* ----------------------------------------------------------- the inventory
 *
 * MEASURED off the USE AN ITEM dialog, which is a numbered list of item types
 * with quantities in brackets -- `1. Raw energium (1)` after one mining run.
 * So it is counts per type with the empty types left out, which is what this
 * array is.
 *
 * The five names are the binary's own table and appear in its order. Only
 * ITEM_RAW_ENERGIUM has a source in this core so far; the other four are kept
 * as ids with no producer for the same reason the score sheet keeps rows that
 * are always zero -- so that building the thing that fills them is a call to
 * one place, not a change to the shape of the game. */
#define ITEM_MONGOL_ENERGIUM  0
#define ITEM_PLASMA_BOLTS     1
#define ITEM_PLASMA_SHIELD    2
#define ITEM_LIFE_SUPPORT     3
#define ITEM_RAW_ENERGIUM     4
#define ITEM_COUNT            5
extern const char *const item_name[ITEM_COUNT];
extern uint8_t inventory[ITEM_COUNT];

/* --------------------------------------------------------------- the chain */

void planet_new(void);            /* called by trek_new_game */
void planet_place(void);          /* called by trek_enter_quadrant */

/* The planet in the current quadrant nearest the given sector, or
   PLANET_NONE. Quadrant 6-4 held two, so "the planet here" is a question with
   more than one answer and the UI needs the one being addressed. */
uint8_t planet_here(void);

/* ORBIT. Adjacency, exactly as docking works, and the original's refusal says
   so: "NAVIGATION: Not adjacent to planet."

   MEASURED: it costs 0.0 stardates, and the scan is part of it -- one command
   both enters orbit and reveals the find. The manual agrees: "This will allow
   the planet to be scanned for the presence of energium crystals and other
   things." */
#define ORBIT_OK           0
#define ORBIT_NO_PLANET    1     /* none adjacent */
#define ORBIT_ALREADY      2
uint8_t trek_orbit(void);

/* Leaving orbit. Any movement does it, the same way movement breaks a dock. */
void trek_leave_orbit(void);

/* LAND, by one of the two ways down.
 *
 * MEASURED: the transporter costs 0.0 stardates and runs a four-beat
 * sequence. The manual gives the shuttlecraft's price and the reason to
 * prefer the transporter: it "takes 0.2 stardays to make the round trip
 * whereas the transporter is virtually instantaneous" (l.490-492).
 *
 * Both are 100%-or-nothing, which is the manual's rule for these two systems
 * and is already in trek_transporter_ok()/trek_shuttle_ok(). Shields up
 * refuses both: "ENGINEERING: Cannot use transporters or shuttlecraft with
 * shields up." */
#define LAND_BY_SHUTTLE      1
#define LAND_BY_TRANSPORTER  2

#define LAND_NO_ORBIT        0   /* "NAVIGATION: Not orbitting a planet" */
#define LAND_SHIELDS_UP      1
#define LAND_DAMAGED         2   /* that way down is not at 100% */
#define LAND_NOTHING         3   /* "Nothing found." */
#define LAND_ENERGIUM        4   /* "energium successfully mined." */
#define LAND_SETTLERS        5   /* "Planet settlers found... Evacuating" */
#define LAND_ATTACKED        6   /* "Landing party attacked... casualties." */
#define LAND_ALREADY         7   /* this planet has already given up its find */

/* `casualties` receives the losses on LAND_ATTACKED, so the UI can print the
   number the original prints. Pass NULL if the caller does not care. */
uint8_t trek_land(uint8_t how, uint16_t *casualties);

/* A landing party that walks into a Mongol supply station comes back short.
   PROVISIONAL: the original prints a casualty count and we have never seen
   one, so this is a plausible range and nothing more. It feeds ship.casualties
   and therefore scores at a point each, which is the only place it is felt. */
#define LANDING_CASUALTY_MIN   1
#define LANDING_CASUALTY_SPAN 10   /* 1..10 */

/* --------------------------------------------------------------------- USE
 *
 * The gate is the one place in this chain that is CONFIRMED rather than
 * derived, and the manual states it outright: "regulations prohibit the use
 * of raw energium except in extreme emergencies; your shields must be under
 * 50% and main energy under 20%" (l.351-354).
 *
 * 20% of ENERGY_MAX is 1000, WHICH IS THE ANCESTOR'S OWN THRESHOLD TO THE
 * UNIT -- its usecrystals() refuses at `game.energy >= 1000`. Two independent
 * documents agreeing on a number is the strongest evidence in this file.
 *
 * MEASURED, and it is the reading worth staring at: energy went 500 to 7435
 * and shields 300 to 2500. SEVEN THOUSAND is well above the 5000 maximum
 * every other mechanic clamps to -- energium OVERCHARGES the ship, and that
 * is the whole point of a mechanic gated on being nearly dead.
 *
 * The amount is DERIVED from the ancestor, which adds `5000.0*(1.0+0.9*Rand())`
 * -- 5000 to 9500. Our one sample added 6935, comfortably inside it. Shields
 * going to full is EGA Trek's own addition; the ancestor does not do it. */
#define CRYSTAL_ENERGY_BASE   5000
#define CRYSTAL_ENERGY_SPAN   4500   /* 0.9 * 5000, so 5000..9499 */

/* Three outcomes, one per pair of strings in the binary:
 *
 *   good       "Crystal loaded...it appears good!"  "Energy levels increasing..."
 *   defective  "This crystal is defective! Energy systems going unstable..."
 *              "Damage to main energy systems."
 *   dud        "The crystal appears to be damaged." "No energy gain is apparent."
 *
 * The ancestor's version of the bad branch DESTROYS THE SHIP -- its cryprob
 * starts at 0.05 and DOUBLES with every use, and a failed roll calls kaboom().
 * EGA Trek softened that: its bad branch damages the main energy systems and
 * leaves you alive. The escalating probability is kept, because a mechanic
 * you can safely repeat is not the gamble either game is describing.
 *
 * PROVISIONAL, both of them. 5% doubling is the ancestor's number, not a
 * measured one, and the dud's flat 10% is invented outright -- it is the
 * weakest constant in this file and nothing but a message depends on it.
 *
 * The binary carries a SECOND set of loading strings ("Loading energium
 * crystal...", "The crystal must be damaged...", "No energy is gained.") in a
 * different literal pool. The reading here is that those belong to Mongol
 * energium, a separate inventory item with a parallel routine. Unconfirmed,
 * and it changes nothing until that item has a source. */
#define CRYSTAL_DEFECT_PCT_0   5     /* first use */
#define CRYSTAL_DUD_PCT       10

#define USE_NO_ITEM        0   /* "No energium crystals are available to load" */
#define USE_REFUSED        1   /* the First Officer's regulations speech */
#define USE_GOOD           2
#define USE_DEFECTIVE      3   /* main energy systems damaged */
#define USE_DUD            4   /* nothing happens */

/* The Y/N confirmation ENGINEERING asks for is the UI's; by the time this is
   called the captain has said yes. `ev`/`max` carry the system damage on
   USE_DEFECTIVE in the usual way, and may be NULL/0. */
uint8_t trek_use_energium(TrekEvent *ev, uint8_t max);

/* Whether the gate is open, so the UI can ask before it offers. */
uint8_t trek_energium_allowed(void);

/* The escalating odds of a defective crystal, for core/serial.c only -- the
   same arrangement as trek_rng_state(). A save that forgets this reopens the
   game at the starting odds, which turns SAVE into a way to launder the risk
   out of the one mechanic built to carry some. */
uint8_t planet_defect_pct(void);
void    planet_defect_restore(uint8_t v);

#endif
