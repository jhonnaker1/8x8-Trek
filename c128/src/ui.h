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

/* Prompts in the COMMAND panel and returns a NUL-terminated line. Echoes as
   it goes and handles backspace. */
void ui_read_command(char *buf, uint8_t max);

/* Modal dialog, the way the original runs weapons and energy transfers: a
   bordered box drawn over the console, its own running transcript inside,
   and the console restored when it closes. Putting these exchanges in the
   COMMUNICATIONS panel instead worked but read nothing like the original,
   and the four-line log pushed the results off before they could be read. */
void ui_dialog_open(const char *title);
void ui_dialog_line(const char *text);
void ui_dialog_ask(const char *prompt, char *buf, uint8_t max);
void ui_dialog_close(void);

/* The STATE OF REPAIR report: every system's condition, and how long it would
   take to mend docked and adrift. Its own screen rather than a dialog line
   because twelve systems plus headers do not fit the modal box, and because
   the original gives it a full-width panel of its own. */
void ui_repair_report(void);

#endif
