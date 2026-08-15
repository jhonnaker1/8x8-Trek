#include <stdint.h>

#include "vdc.h"
#include "layout.h"
#include "ui.h"
#include "input.h"
#include "../../core/trek.h"

/* 8x8 Trek -- C128 VDC port, milestone 2.
 *
 * Galaxy generation and movement, driven from the shared core. The console
 * frame from milestone 1 now shows live state: the short range scan, the
 * chart of known galaxy, and the status readouts all read core arrays.
 *
 * No combat, damage, docking or supplies yet.
 */

/* Key values come from input.h (KB_*), the same symbols the scan table is
   built from. They were briefly duplicated here as separate numeric
   constants, which is how `q` came to never quit: input.c's table was
   written with character literals, cc65 translated them to PETSCII, and the
   two spellings drifted silently. One definition, included by both. */

/* PROVISIONAL: a fixed seed, so the same galaxy comes up every run. That is
   deliberate for now -- it makes a side-by-side against the original in
   DOSBox-X repeatable, and the core test asserts a seed reproduces a galaxy
   exactly. Wire this to something varying once there is a title screen to
   take the timing from. */
#define GAME_SEED  12345
#define GAME_LEVEL 3

static char cmd[16];

/* Collects the digits out of a command line, ignoring any separators the
   player chose to use. The original is equally relaxed: "you can use
   whatever is most convenient for separators" (manual l.538). */
static uint8_t grab_digits(const char *s, uint8_t *out, uint8_t max) {
    uint8_t n = 0;
    unsigned char u;

    while (*s && n < max) {
        u = (unsigned char)*s++;
        if (u >= KB_DIGIT0 && u <= KB_DIGIT9) out[n++] = (uint8_t)(u - KB_DIGIT0);
    }
    return n;
}

static void report_move(uint8_t r) {
    switch (r) {
        case MOVE_OK:
            break;
        case MOVE_BLOCKED:
            ui_message("NAVIGATION: ", "BLOCKED BY OBJECT");
            break;
        case MOVE_NO_ENERGY:
            ui_message("ENGINEERING: ", "TOO LITTLE ENERGY");
            break;
        case MOVE_SAME_PLACE:
            ui_message("NAVIGATION: ", "THAT IS OUR LOCATION");
            break;
        default:
            ui_message("NAVIGATION: ", "NO SUCH LOCATION");
            break;
    }
}

/* Coordinates arrive 1-based, as the original presents them, and are
   converted to the core's 0-based indexing here -- the boundary between
   presentation and core is exactly the right place for it. */
static void do_move(const char *line) {
    uint8_t d[8];
    uint8_t n = grab_digits(line, d, 8);
    uint8_t i;

    for (i = 0; i < n; i++) {
        if (d[i] < 1 || d[i] > 8) {
            ui_message("NAVIGATION: ", "NO SUCH LOCATION");
            return;
        }
    }

    if (n == 2) {
        report_move(trek_move_impulse((uint8_t)(d[0] - 1), (uint8_t)(d[1] - 1)));
    } else if (n == 4) {
        report_move(trek_move_warp((uint8_t)(d[0] - 1), (uint8_t)(d[1] - 1),
                                   (uint8_t)(d[2] - 1), (uint8_t)(d[3] - 1)));
    } else {
        ui_message("NAVIGATION: ", "GIVE SECTOR OR QUAD+SEC");
    }
}

static void do_warp(const char *line) {
    uint8_t d[4];
    uint8_t n = grab_digits(line, d, 4);
    uint8_t tenths;

    if (n == 0) {
        ui_message("ENGINEERING: ", "SPEED CAPTAIN?");
        return;
    }
    /* "w5" is warp 5.0, "w5.2" is warp 5.2 (manual l.632). */
    tenths = (uint8_t)(n == 1 ? d[0] * 10 : d[0] * 10 + d[1]);

    if (trek_set_warp(tenths)) ui_message("ENGINEERING: ", "WARP SPEED SET");
    else ui_message("ENGINEERING: ", "INVALID WARP FACTOR");
}

int main(void) {
    unsigned char c;

    vdc_init();
    trek_new_game(GAME_LEVEL, GAME_SEED);
    ui_draw_all();
    ui_message("HELM: ", "AWAITING ORDERS CAPTAIN");

    for (;;) {
        ui_read_command(cmd, sizeof cmd);

        c = (unsigned char)cmd[0];

        if (c == KB_M)      do_move(cmd);
        else if (c == KB_W) do_warp(cmd);
        else if (c == KB_Q) break;
        else if (c)          ui_message("COMPUTER: ", "M)OVE W)ARP Q)UIT");

        ui_draw_scan();
        ui_draw_chart();
        ui_draw_status();
    }

    /* Quit has to visibly end the game. Milestone 1 parked here in an
       endless wait_vsync() so the console would stay on screen, which made
       Q indistinguishable from a hang once there was a command loop to quit
       out of. Acknowledge, wait for the captain, then hand the machine back
       in the state we found it. */
    ui_clear_messages();
    ui_message("HELM: ", "MISSION ENDED, CAPTAIN.");
    ui_message("COMPUTER: ", "RUN/STOP+RESTORE FOR");
    ui_message("COMPUTER: ", "BASIC.");

    /* Drop back to 1MHz first, so the VIC-IIe screen is live again and the
       machine does not look dead on the other window. */
    vdc_shutdown();

    /* KNOWN ISSUE -- returning to BASIC wedges the C128.
     *
     * `return 0` here hands control to cc65's exit path, and the machine
     * stops responding on both screens. Not diagnosed: this program runs at
     * 2MHz, drives the VDC directly, and scans CIA1 behind the KERNAL's
     * back, so there are several candidates and no way to observe the
     * machine from a session on this host (see NOTES.md on why VICE cannot
     * be screenshotted or single-stepped from here).
     *
     * Parking is the honest behaviour rather than a pretend one: the final
     * console stays readable, and because interrupts are left enabled the
     * KERNAL's NMI still works, so RUN/STOP+RESTORE gets the player a BASIC
     * prompt. Plenty of 8-bit games never returned to BASIC at all.
     *
     * To diagnose properly, bisect it: return immediately from main() at the
     * top with no vdc_init() at all and confirm a clean exit, then add back
     * the 2MHz switch, then the VDC register writes, then the CIA scanning.
     * The first one that wedges it is the culprit. */
    for (;;) { }

    return 0;
}
