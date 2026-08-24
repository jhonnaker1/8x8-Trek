/* TREK.SCR, the hall of fame file.
 *
 * The layout assertion is the one that matters. Ten records against five
 * ranks looks like two-per-rank in file order, and it is not -- the file is
 * place-major, measured by crafting a file with SLOT0..SLOT9 in order and
 * reading the names back off the original's own screen. See core/hof.h. */
#include <stdio.h>
#include <string.h>
#include "../hof.h"

static int failures;

static void check(int cond, const char *what) {
    if (!cond) { printf("FAIL: %s\n", what); failures++; }
}

static void check_eq(long got, long want, const char *what) {
    if (got != want) {
        printf("FAIL: %s (got %ld, want %ld)\n", what, got, want);
        failures++;
    }
}

/* MEASURED: records 0..4 are first place for ranks 1..5, records 5..9 are
   second place. Rank-major is the natural guess and it is wrong, so this is
   asserted rather than left to the reader of hof_index(). */
static void test_place_major_layout(void) {
    check_eq(hof_index(1, 0), 0, "Lt. Commander, first place  -> record 0");
    check_eq(hof_index(2, 0), 1, "Commander, first place      -> record 1");
    check_eq(hof_index(3, 0), 2, "Captain, first place        -> record 2");
    check_eq(hof_index(4, 0), 3, "Commodore, first place      -> record 3");
    check_eq(hof_index(5, 0), 4, "Admiral, first place        -> record 4");
    check_eq(hof_index(1, 1), 5, "Lt. Commander, second place -> record 5");
    check_eq(hof_index(5, 1), 9, "Admiral, second place       -> record 9");
}

/* The shipped reference/TREK.SCR is 300 bytes: ten blank names and ten zero
   scores. Rebuilt here rather than read from disk so the test needs no
   gitignored material. */
static void test_shipped_file(void) {
    uint8_t buf[HOF_BUF];
    HofEntry tbl[HOF_ENTRIES];
    uint16_t pos = 0;
    uint8_t i, j;

    for (i = 0; i < HOF_ENTRIES; i++) {
        for (j = 0; j < HOF_NAME; j++) buf[pos++] = '.';
        buf[pos++] = '\r'; buf[pos++] = '\n';
        buf[pos++] = '0';
        buf[pos++] = '\r'; buf[pos++] = '\n';
    }
    check_eq(pos, 300, "the shipped file's shape is 300 bytes");
    check(hof_parse(buf, pos, tbl), "the shipped file parses");
    check_eq(tbl[0].score, 0, "every shipped score is zero");
    check_eq(tbl[9].name[0], '.', "every shipped name is dots");
}

static void test_round_trip(void) {
    uint8_t buf[HOF_BUF];
    HofEntry a[HOF_ENTRIES], b[HOF_ENTRIES];
    uint16_t n;
    uint8_t i;

    hof_clear(a);
    for (i = 0; i < HOF_ENTRIES; i++) {
        a[i].name[0] = (char)('A' + i);
        a[i].score = (int16_t)(i * 100);
    }
    /* One negative, because the original scores a scuttled ship at -930 and a
       formatter that cannot write a minus sign writes a file it cannot read. */
    a[3].score = -930;

    n = hof_format(a, buf, sizeof buf);
    check(n > 0, "format succeeds");
    check(hof_parse(buf, n, b), "and what it wrote parses back");

    for (i = 0; i < HOF_ENTRIES; i++) {
        check_eq(b[i].score, a[i].score, "score survives the round trip");
        check_eq(b[i].name[0], a[i].name[0], "name survives the round trip");
    }
    check_eq(b[3].score, -930, "a negative score survives");
}

static void test_offer(void) {
    HofEntry t[HOF_ENTRIES];

    hof_clear(t);

    check(hof_offer(t, 3, "FIRST", 500), "a score beats an empty table");
    check_eq(t[hof_index(3, 0)].score, 500, "and lands in first place");

    check(hof_offer(t, 3, "BETTER", 900), "a better score is accepted");
    check_eq(t[hof_index(3, 0)].score, 900, "takes first");
    check_eq(t[hof_index(3, 1)].score, 500, "and pushes the old first to second");

    check(hof_offer(t, 3, "MIDDLE", 700), "a middling score takes second");
    check_eq(t[hof_index(3, 1)].score, 700, "second place updated");
    check_eq(t[hof_index(3, 0)].score, 900, "first place untouched");

    check(!hof_offer(t, 3, "WORSE", 100), "a score beating neither is refused");

    /* Ranks are independent tables that happen to share a file. */
    check_eq(t[hof_index(4, 0)].score, 0, "another rank is unaffected");
    check(hof_offer(t, 4, "ADMIRAL", 1), "and takes its own first place");
    check_eq(t[hof_index(3, 0)].score, 900, "without disturbing rank 3");
}

/* A corrupt file must read as an EMPTY hall of fame, never a partly-filled
   one -- half a table from the disk and half from memory is the worse
   failure. */
static void test_refuses_garbage(void) {
    uint8_t buf[HOF_BUF];
    HofEntry t[HOF_ENTRIES];

    memset(buf, 'x', sizeof buf);
    check(!hof_parse(buf, sizeof buf, t), "garbage is refused");
    check_eq(t[0].score, 0, "and the table is left empty");
    check_eq(t[0].name[0], '.', "with blank names");

    check(!hof_parse(buf, 10, t), "a truncated file is refused");
}

int main(void) {
    test_place_major_layout();
    test_shipped_file();
    test_round_trip();
    test_offer();
    test_refuses_garbage();

    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("hall of fame file: all checks passed\n");
    return 0;
}
