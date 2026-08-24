#ifndef UI_H
#define UI_H

#include <stdint.h>

/* C128-VDC presentation layer. Reads the shared core's state and paints it
   into the nine-panel console; the core never knows this file exists and
   formats no text of its own. */

void ui_draw_all(void);        /* frame plus every live panel */
void ui_draw_scan(void);
void ui_draw_chart(void);
void ui_draw_status(void);
void ui_draw_systems(void);
void ui_draw_lasers(void);
void ui_draw_badge(void);
void ui_draw_viewer(void);
void ui_draw_position(void);

/* Four-line message log in the COMMUNICATIONS panel, oldest scrolling off
   the top. `dept` is the originating department, as the original prefixes
   every message with (manual l.310). */
void ui_message(const char *dept, const char *text);
void ui_clear_messages(void);

/* A#, and MSGS. MEASURED 2026-08-23: the panel is a QUEUE awaiting
   acknowledgement and the log is a permanent RECORD, so ui_ack() dismisses
   from the panel and the message is still there when MSGS next opens.
   `n` is 1-based down the panel; 0 is bare `A`, which clears all of them;
   out of range is a silent no-op. Neither command costs a turn. */
void ui_ack(uint8_t n);
void ui_messages_view(void);

/* Prompts in the COMMAND panel and returns a NUL-terminated line. Echoes as
   it goes and handles backspace. */
void ui_read_command(char *buf, uint8_t max);

/* A Y/N question on the COMMAND line, which is where the original asks its
   quit confirmation. Non-zero for yes; takes an explicit Y or N only. */
uint8_t ui_confirm(const char *prompt);

/* Modal dialog, the way the original runs weapons and energy transfers: a
   bordered box drawn over the console, its own running transcript inside,
   and the console restored when it closes. Putting these exchanges in the
   COMMUNICATIONS panel instead worked but read nothing like the original,
   and the four-line log pushed the results off before they could be read. */
void ui_dialog_open(const char *title);
void ui_dialog_line(const char *text);
void ui_dialog_ask(const char *prompt, char *buf, uint8_t max);

/* As ui_dialog_ask, but ESC abandons the prompt and returns 0. EGA Trek's
   self-destruct prompt says "Hit ESC to abort"; nothing else offers it. */
uint8_t ui_dialog_ask_esc(const char *prompt, char *buf, uint8_t max);
void ui_dialog_close(void);

/* Closes with no "hit return" prompt. SAVE uses this because the original's
   save box closes as soon as it has a file name. */
void ui_dialog_dismiss(void);

/* The STATE OF REPAIR report: every system's condition, and how long it would
   take to mend docked and adrift. Its own screen rather than a dialog line
   because twelve systems plus headers do not fit the modal box, and because
   the original gives it a full-width panel of its own. */
void ui_repair_report(void);

/* Full system names, in SYS_* order. F)ix needs them to print its list. */
const char *ui_sys_name(uint8_t i);

/* What the setup screen collects, in the original's own order. Answering yes
   to the restore prompt now reads a saved game; if the file is missing it
   reports so and carries on, which is what the original does. */
typedef struct {
    /* Non-zero if ui_setup() restored a saved game instead of collecting a
       new one. main() must NOT call trek_new_game() in that case -- doing so
       would generate a fresh galaxy straight over the one just loaded. */
    uint8_t  restored;
    char     name[13];
    char     password[9];
    uint8_t  level;      /* 1..5 */
    uint8_t  briefing;   /* non-zero if the player asked for one */
    uint16_t seed;       /* sampled from how long the answers took */
} Setup;

void ui_setup(Setup *s);

/* SAVE. Costs no turn, as the original's does. Asks for a file name with
   EGATREK.SAV as the default. */
void ui_save_game(const Setup *s);

/* The seed derivation, exposed so it can be tested without a keyboard. Mixing
   in the level keeps two identically-timed sittings at different difficulties
   apart, and the result is never zero because the core's xorshift is dead
   there. */
uint16_t setup_seed(uint16_t entropy, uint8_t level);

/* End of game. The evaluation is the original's Detailed Evaluation, nine
   line items and a total; the hall of fame is its five rank rows. Both block
   on Return. */
/* The title screen. Blocks on Return. */
void ui_title(void);

/* INFO: one enemy at a time -- class, sector, range, bearing and its strength
   as a percentage, which is what the original calls its shields. SPACE steps
   to the next, RETURN closes. The original says "up/down to select"; this port
   has no arrow keys in its matrix, so it says what it does. */
void ui_info_panel(void);

/* The prompt the original shows over the Hall of Fame. Non-zero for YES.
   Blocks on an explicit Y or N -- see ui.c for why RETURN is not a shortcut. */
uint8_t ui_play_again(void);

void ui_evaluation(void);
void ui_hall_of_fame(const char *name, uint8_t level, int16_t score);

#endif
