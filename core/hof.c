#include "hof.h"

#define PAD '.'

uint8_t hof_index(uint8_t level, uint8_t place) {
    /* MEASURED, see hof.h: place-major, not rank-major. */
    return (uint8_t)(place * HOF_RANKS + (level - 1));
}

void hof_clear(HofEntry *tbl) {
    uint8_t i, j;
    for (i = 0; i < HOF_ENTRIES; i++) {
        for (j = 0; j < HOF_NAME; j++) tbl[i].name[j] = PAD;
        tbl[i].name[HOF_NAME] = '\0';
        tbl[i].score = 0;
    }
}

/* Reads a decimal number up to the CR, leaving `pos` past the LF. Scores can
   be negative -- a scuttled ship scored -930 in the original -- so the sign
   is not optional decoration. */
static int16_t read_num(const uint8_t *buf, uint16_t len, uint16_t *pos,
                        uint8_t *ok) {
    int16_t v = 0;
    uint8_t neg = 0, digits = 0;
    uint16_t p = *pos;

    if (p < len && buf[p] == '-') { neg = 1; p++; }
    while (p < len && buf[p] >= '0' && buf[p] <= '9') {
        v = (int16_t)(v * 10 + (buf[p] - '0'));
        p++; digits++;
    }
    if (!digits) *ok = 0;
    if (p + 1 >= len || buf[p] != '\r' || buf[p + 1] != '\n') *ok = 0;
    *pos = (uint16_t)(p + 2);
    return neg ? (int16_t)(-v) : v;
}

uint8_t hof_parse(const uint8_t *buf, uint16_t len, HofEntry *out) {
    uint16_t pos = 0;
    uint8_t i, j, ok = 1;

    hof_clear(out);

    for (i = 0; i < HOF_ENTRIES && ok; i++) {
        if ((uint16_t)(pos + HOF_NAME + 2) > len) { ok = 0; break; }
        for (j = 0; j < HOF_NAME; j++) out[i].name[j] = (char)buf[pos + j];
        out[i].name[HOF_NAME] = '\0';
        pos = (uint16_t)(pos + HOF_NAME);
        if (buf[pos] != '\r' || buf[pos + 1] != '\n') { ok = 0; break; }
        pos = (uint16_t)(pos + 2);
        out[i].score = read_num(buf, len, &pos, &ok);
    }

    if (!ok) hof_clear(out);
    return ok;
}

static uint16_t put_num(uint8_t *buf, uint16_t pos, int16_t v) {
    uint8_t digits[6];
    uint8_t n = 0;
    uint16_t mag;

    if (v < 0) { buf[pos++] = '-'; mag = (uint16_t)(0u - (uint16_t)v); }
    else       { mag = (uint16_t)v; }

    do { digits[n++] = (uint8_t)('0' + (mag % 10)); mag /= 10; } while (mag);
    while (n) buf[pos++] = digits[--n];
    return pos;
}

uint16_t hof_format(const HofEntry *in, uint8_t *buf, uint16_t max) {
    uint16_t pos = 0;
    uint8_t i, j;

    for (i = 0; i < HOF_ENTRIES; i++) {
        if ((uint16_t)(pos + HOF_NAME + 12) > max) return 0;
        {
            /* Same rule as hof_offer: the first NUL ends the name and the
               rest of the field is padding, not a byte-by-byte substitution.
               The original pads with dots, and a NUL or a space in the middle
               of the field would make a file its own reader rejects. */
            char c = 1;
            for (j = 0; j < HOF_NAME; j++) {
                if (c) c = in[i].name[j];
                buf[pos + j] = (uint8_t)(c ? c : PAD);
            }
        }
        pos = (uint16_t)(pos + HOF_NAME);
        buf[pos++] = '\r'; buf[pos++] = '\n';
        pos = put_num(buf, pos, in[i].score);
        buf[pos++] = '\r'; buf[pos++] = '\n';
    }
    return pos;
}

uint8_t hof_offer(HofEntry *tbl, uint8_t level, const char *name, int16_t score) {
    uint8_t first  = hof_index(level, 0);
    uint8_t second = hof_index(level, 1);
    uint8_t place, j;

    if (score > tbl[first].score)       place = first;
    else if (score > tbl[second].score) place = second;
    else return 0;

    /* A new first pushes the old first into second. Ranks do not interact:
       an Admiral's score never displaces a Captain's. */
    if (place == first) tbl[second] = tbl[first];

    /* Once the terminator is reached, EVERYTHING after it is padding. The
       first version tested each byte on its own and padded only the NULs,
       which reads straight past the end of the caller's buffer: the setup
       screen's name is 13 bytes and its password sits directly after it, so
       a Captain called WROTE was recorded as "WROTE......BOOM......" with the
       self-destruct password baked into the hall of fame. Seen on screen
       2026-08-23, which is the only place it could have been seen -- both
       buffers are live memory and neither read was out of bounds enough to
       fault. */
    {
        char c = 1;
        for (j = 0; j < HOF_NAME; j++) {
            if (c) c = name[j];
            tbl[place].name[j] = c ? c : PAD;
        }
    }
    tbl[place].name[HOF_NAME] = '\0';
    tbl[place].score = score;
    return 1;
}
