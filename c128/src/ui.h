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

/* Four-line message log in the COMMUNICATIONS panel, oldest scrolling off
   the top. `dept` is the originating department, as the original prefixes
   every message with (manual l.310). */
void ui_message(const char *dept, const char *text);
void ui_clear_messages(void);

/* Prompts in the COMMAND panel and returns a NUL-terminated line. Echoes as
   it goes and handles backspace. */
void ui_read_command(char *buf, uint8_t max);

#endif
