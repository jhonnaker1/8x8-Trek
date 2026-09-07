/* Amiga first light. One screenshot answers four questions:
 *   1. does the toolchain produce a binary this machine runs,
 *   2. does a 640x200 four-bitplane screen give 80x25 cells exactly,
 *   3. do all sixteen EGA colours come out on their own palette index,
 *   4. do the box-drawing glyphs -- which are this port's OWN artwork here,
 *      because topaz has letters at those screen codes -- actually join up.
 *
 * The console frame is drawn through the SHARED c128/src/layout.c, so this
 * also proves that file compiles and runs for 68000 against a video layer
 * that is nothing like a VDC.
 */
#include "../../c128/src/input.h"
#include "../../c128/src/vdc.h"
#include "../../c128/src/layout.h"
#include "../../core/strpool.h"

/* A STUB STRING POOL, and it exists so this can draw the REAL console frame.
   layout.c fetches every panel title through S(), which on a finished port
   reads STRINGS.DAT out of far memory -- machinery this milestone has not
   built. These seven titles are the pool's own text for the ids layout.c
   asks for; anything else answers with its number so a wrong id is visible
   rather than blank. Replaced by c128/src/strpool.c once the file seam is in. */
const char *S(StrId id) {
    switch (id) {
        case 127: return "LASERS";
        case 145: return "SHORT RANGE SCAN";
        case 146: return "STATUS";
        case 147: return "CHART OF KNOWN GALAXY";
        case 148: return "COMMAND";
        case 149: return "MAIN VIEWER";
        case 150: return "SYSTEMS STATUS";
        default:  return "?";
    }
}

static const char *names[16] = {
    "BLACK","BLUE","GREEN","CYAN","RED","MAGENTA","BROWN","LTGRAY",
    "DKGRAY","LTBLUE","LTGREEN","LTCYAN","LTRED","LTMAGENTA","YELLOW","WHITE" };

int main(void) {
    int i;

    vdc_init();

    /* Sixteen colours, each label written in its own colour. An unreadable
       name means that index is wrong, and index 0 is meant to be invisible. */
    for (i = 0; i < 16; i++)
        scr_puts((unsigned char)(42 + (i / 8) * 19),
                 (unsigned char)(12 + (i % 8)), names[i], (unsigned char)i);

    /* THE REAL CONSOLE FRAME, through the shared layout.c -- nine panels, the
       titles in their borders, and the junctions where panels share a row.
       Those junctions shipped broken on three ports until 2026-09-06, and
       here they are drawn from GLYPHS THIS PORT AUTHORED rather than from a
       character ROM, so whether they join is a real question. */
    draw_console();

    /* EVERY GLYPH THIS PORT HAD TO DRAW, in the panels that use them, because
       a code that renders as a letter is the failure mode here and it is
       invisible until something asks for it. The set was re-derived from the
       shared sources -- fifteen codes, not the eleven an eyeball count of the
       box drawing gives. */
    {
        static const unsigned char codes[] = {
            G_HLINE, G_VLINE, G_TL, G_TR, G_BL, G_BR,
            G_TEE_L, G_TEE_R, G_TEE_D, G_TEE_U, G_CROSS,
            98, 226, 160, 228, 81
        };
        unsigned char k;
        for (k = 0; k < sizeof codes; k++)
            scr_put((unsigned char)(3 + k * 2), 19, codes[k], 11);
        scr_puts(3, 18, "EVERY DRAWN GLYPH", 14);

        /* The ship, exactly as ui.c assembles it: saucer, neck, hull. */
        scr_puts(3, 21, "SHIP", 14);
        scr_put(9, 21, 81, 15);
        scr_hline(10, 21, 4, G_HLINE, 15);
        scr_put(14, 21, 160, 15);

        /* The badge disc: top cap, body, bottom cap, stacked. */
        scr_puts(18, 21, "DISC", 14);
        scr_put(24, 20, 98, 9); scr_put(24, 21, 160, 9); scr_put(24, 22, 226, 9);

        /* Systems bars -- seven pixels on an eight-pixel pitch, so there must
           be a visible hairline between them rather than one solid block. */
        scr_puts(28, 21, "BARS", 14);
        scr_hline(34, 21, 6, 228, 10);

        /* AND ONE CODE ON PURPOSE THAT NOBODY DREW. 97 is a C64 graphics code
           this port has no glyph for, so it must come out as the hollow box
           amigagfx.c substitutes -- proving that a glyph nobody noticed is
           missing shows up as something rather than as nothing. A marker that
           has never fired is not a marker. */
        scr_puts(42, 19, "UNDRAWN CODE 97 -> HOLLOW BOX:", 8);
        scr_put(74, 19, 97, 12);

        /* THE PLAY-AGAIN PROMPT, verbatim from ui.c. Its brackets are screen
           codes 27 and 29, which this port had no glyph for until the marker
           fired on them -- so it is drawn here every run rather than left to
           be discovered at the end of somebody's game. */
        scr_puts(3, 23, "PLAY AGAIN?  [YES]  [NO]", 13);
    }

    scr_puts(42, 21, "640X200, FOUR PLANES, 80X25 CELLS", 10);
    scr_puts(42, 22, "SIXTEEN EGA COLOURS ON THEIR OWN INDEX", 10);
    scr_puts(42, 23, "PRESS RETURN TO LEAVE", 12);

    /* THE INPUT SEAM, ECHOED. Every key comes back as its character and its
       code, so this screenshot says what kb_waitkey() actually returns rather
       than what this file assumes -- which is the check the X16 did not have
       until every typed order had already been answering NO SUCH ORDER.
       The arrows must read as 1 and 2, and letters must arrive UPPER CASE
       whichever way they were typed. RETURN (13) ends it. */
    kb_init();
    scr_puts(42, 5, "TYPE: KEYS ECHO AS CHAR AND CODE", 14);
    scr_puts(42, 6, "ARROWS MUST READ 1 AND 2, RETURN ENDS", 8);
    {
        unsigned char col = 42, row = 8;
        for (;;) {
            char k = kb_waitkey();
            unsigned char u = (unsigned char)k;
            char one[2];

            if (k == KB_RETURN) break;

            scr_fill_rect(col, row, 12, 1, 32, 15);
            one[0] = (u >= 32 && u < 127) ? k : '?';
            one[1] = '\0';
            scr_puts(col, row, one, 15);
            scr_put((unsigned char)(col + 2), row,
                    (unsigned char)(48 + (u / 100) % 10), 10);
            scr_put((unsigned char)(col + 3), row,
                    (unsigned char)(48 + (u / 10) % 10), 10);
            scr_put((unsigned char)(col + 4), row,
                    (unsigned char)(48 + u % 10), 10);
            if (++row > 19) { row = 8; col = (unsigned char)(col + 6); }
            if (col > 70) col = 42;
        }
    }

    vdc_shutdown();
    plat_exit();
    return 0;
}
