/* What fonts does this machine's ROM actually carry, and what shape are they?
   Line-A init returns a1 = pointer to an array of three font header pointers.
   Read the headers rather than assuming the usual 4x6 / 6x6 / 8x8 trio. */
#include <tos.h>
#include <stdio.h>

__regsused("d0/d1/a0/a1/a2") void *linea_fonts(void) =
  "\tmovem.l\td2/a2,-(sp)\n"
  "\tdc.w\t$a000\n"
  "\tmove.l\ta1,d0\n"
  "\tmovem.l\t(sp)+,d2/a2\n";

struct fnthdr {
    short id, point;
    char  name[32];
    unsigned short first_ade, last_ade;
    short top, ascent, half, descent, bottom;
    short max_char_width, max_cell_width;
    short left_offset, right_offset;
    short thicken, ul_size, lighten, skew;
    unsigned short flags;
    unsigned char *hor_table;
    unsigned short *off_table;
    unsigned char *dat_table;
    unsigned short form_width, form_height;
    struct fnthdr *next_font;
};

int main(void)
{
    struct fnthdr **fonts = (struct fnthdr **)linea_fonts();
    int i;

    printf("font table at %p\r\n", (void *)fonts);
    for (i = 0; i < 3; i++) {
        struct fnthdr *f = fonts[i];
        if (!f) { printf("[%d] NULL\r\n", i); continue; }
        printf("[%d] id=%d pt=%d '%.16s'\r\n", i, f->id, f->point, f->name);
        printf("    ade %u..%u  cellw=%d formw=%u formh=%u flags=$%04x\r\n",
               f->first_ade, f->last_ade, f->max_cell_width,
               f->form_width, f->form_height, f->flags);
        printf("    dat=%p off=%p\r\n", (void *)f->dat_table, (void *)f->off_table);
        if (f->off_table)
            printf("    off['A']=%u off['B']=%u\r\n",
                   f->off_table['A' - f->first_ade],
                   f->off_table['B' - f->first_ade]);
    }
    printf("DONE\r\n");
    return 0;
}
