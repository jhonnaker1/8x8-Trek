/* EVERY 40-COLUMN SCREEN, ONE KEY-POKE APART. A bench, not a game.
 *
 * WHY THIS EXISTS, AND IT SHOULD HAVE EXISTED FOUR SCREENS AGO. Looking at
 * the hall of fame meant booting the real game, typing through six setup
 * prompts, opening the self-destruct dialog, entering a password and pressing
 * RETURN three times -- about a hundred seconds of emulator per look, for a
 * screen whose LAYOUT is the only thing in question. Jamie: "can't you just
 * make test builds to just display the screen you are working on?" That is
 * NOTES.md's own "fix the cycle before iterating", and he has had to say it
 * twice.
 *
 * It links the REAL ui.c against the real string pool and draws ONE screen,
 * chosen by a byte the monitor pokes. The screens are the genuine article
 * rather than mock-ups -- a mock-up would only prove my idea of the layout is
 * consistent with itself.
 *
 * -DTREK_DEBUG_INPUT IS REQUIRED and is not about typing. Most of these end
 * in `while (kb_waitkey() != KB_RETURN)`, so without a key to give them the
 * bench shows exactly one screen per boot and every later poke is ignored --
 * which looked like "the screen did not change" and is really "the machine is
 * still inside the previous screen".
 *
 * NO -DTREK_OVERLAYS: core/overlay.h then turns OVL_CODE into nothing and
 * everything is resident, which is what a bench wants. Different link from
 * the game; it proves nothing about the game's overlay split.
 */
#include <stdint.h>

#include "vdc.h"
#include "vic.h"
#include "layout40.h"
#include "ui.h"
#include "input.h"
#include "../../core/trek.h"
#include "../../core/strpool.h"
#include "../../core/hof.h"

volatile unsigned char screen40 = 0;

static void sample_messages(void)
{
    /* THE DEPARTMENT STRINGS IN THE POOL CARRY THEIR OWN ": ", which is why
       neither the panel nor the viewer inserts one. Passing bare literals
       here produced "HELMAWAITING ORDERS CAPTAIN" and I put that on the open
       list as a 40-column collision. It was the bench. */
    ui_message("HELM: ", "AWAITING ORDERS CAPTAIN");
    ui_message("ENGINEERING: ", "WARP DRIVE AT 100 PERCENT");
    ui_message("SCIENCE: ", "LONG RANGE SCAN COMPLETE");
    ui_message("COMMS: ", "STARBASE ACKNOWLEDGES OUR SIGNAL");
}

int main(void)
{
    unsigned char shown = 0xFF;
    Setup s;

    vdc_init();
    kb_init();
    str_load();
    trek_new_game(3, 0x1234);

    for (;;) {
        if (screen40 == shown) continue;
        shown = screen40;
        switch (shown) {
        case 0:  ui_draw_all();                               break;
        case 1:  layout40_page = PAGE_CHART; ui_draw_all();
                 layout40_page = PAGE_TACTICAL;               break;
        case 2:  ui_hall_of_fame("JAMES T. KIRK", 3, 1234);   break;
        case 3:  ui_evaluation();                             break;
        case 4:  /* off ship.lost_how; a fresh game has lost nothing, which is
                    why this drew NOTHING before. */
                 ship.lost_how = LOSS_RAY;
                 ui_loss_memo();                              break;
        case 5:  /* DAMAGE SOMETHING FIRST. The report deliberately leaves
                    the DOCKED/UNDOCKED times blank for an undamaged system --
                    "a column of 0.0 down twelve rows reads as noise" -- so a
                    fresh ship shows two empty columns, which I mistook for
                    broken geometry and put on the open list as such. */
                 ship.sys[0] = 42; ship.sys[3] = 70; ship.sys[7] = 15;
                 ui_repair_report();                          break;
        case 6:  ui_draw_all(); sample_messages();
                 ui_messages_view();                          break;
        case 7:  ui_info_panel();                             break;
        case 8:  ui_planet_list();                            break;
        case 9:  ui_draw_all(); (void)ui_play_again();        break;
        case 10: ui_title();                                  break;
        case 11: ui_draw_all();
                 ui_dialog_open("WEAPONS CONTROL");
                 ui_dialog_line("THE DEATH RAY IS EXPERIMENTAL");
                 ui_dialog_line("FIRST OFFICER: CAPTAIN, REGULATIONS DO NOT "
                                "ALLOW SUCH A DANGEROUS PROCEDURE EXCEPT "
                                "UNDER EXTREME LOW ENERGY CONDITIONS.");
                 (void)kb_waitkey();                          break;
        case 12: ui_setup(&s);                                break;
        default: scr_clear();                                 break;
        }
    }
}
