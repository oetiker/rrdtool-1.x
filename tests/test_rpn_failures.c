#include "rrd_tool.h"
#include "rrd_rpncalc.h"

static int allocations, fail_at;
static void *test_malloc(size_t size)
{
    return ++allocations == fail_at ? NULL : malloc(size);
}

static void *test_realloc(void *ptr, size_t size)
{
    return ++allocations == fail_at ? NULL : realloc(ptr, size);
}

#undef rrd_realloc
#define rrd_realloc test_realloc
#define malloc test_malloc
#include "../src/rrd_rpncalc.c"
#undef malloc
#undef rrd_realloc

static int failed;
#define CHECK(c, ...) do { if (!(c)) { fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); failed = 1; } } while (0)

/* Every operator COMPUTE does not allow, and the name the open-time
 * check of a file reports for it. */
static const struct {
    enum op_en op;
    const char *name;
} forbidden_ops[] = {
    {OP_TIME, "TIME"}, {OP_LTIME, "LTIME"}, {OP_PREV, "PREV"},
    {OP_COUNT, "COUNT"}, {OP_TREND, "TREND"}, {OP_TRENDNAN, "TRENDNAN"},
    {OP_PREDICT, "PREDICT"}, {OP_PREDICTSIGMA, "PREDICTSIGMA"},
    {OP_PREDICTPERC, "PREDICTPERC"}, {OP_STEPWIDTH, "STEPWIDTH"},
    {OP_NEWDAY, "NEWDAY"}, {OP_NEWWEEK, "NEWWEEK"},
    {OP_NEWMONTH, "NEWMONTH"}, {OP_NEWYEAR, "NEWYEAR"},
};

int main(void)
{
    rpn_cdefds_t program[3] = {{0}};
    char *text = NULL;
    char expected[100];
    size_t i;

    program[0].op = OP_NUMBER;
    program[0].val = 1;
    program[1].op = OP_NUMBER;
    program[1].val = 2;
    program[2].op = OP_END;
    fail_at = 2; /* Grow the comma after the first value. */
    rpn_compact2str(program, NULL, &text);
    CHECK(rrd_test_error() && text != NULL && strcmp(text, "1") == 0,
          "failed growth: kept \"%s\", error \"%s\"", text ? text : "(null)", rrd_get_error());
    free(text);
    text = NULL;
    program[0].op = OP_END;
    allocations = 0;
    fail_at = 1;
    rrd_clear_error();
    rpn_compact2str(program, NULL, &text);
    CHECK(text == NULL && strstr(rrd_get_error(), "failed to alloc memory"),
          "failed first allocation: error \"%s\"", rrd_get_error());
    free(text);

    /* A file's COMPUTE program VARIABLE(0) <op> END: the check names the
     * operator COMPUTE does not allow. */
    fail_at = 0;
    program[0].op = OP_VARIABLE;
    program[0].val = 0;
    program[2].op = OP_END;
    for (i = 0; i < sizeof(forbidden_ops) / sizeof(forbidden_ops[0]); i++) {
        program[1].op = (char) forbidden_ops[i].op;
        program[1].val = 0;
        rrd_clear_error();
        snprintf(expected, sizeof(expected), "COMPUTE program uses operator %s, "
                 "which COMPUTE does not allow", forbidden_ops[i].name);
        CHECK(rpn_compact_check(program, 1) == -1
              && strcmp(rrd_get_error(), expected) == 0,
              "%s: error \"%s\"", forbidden_ops[i].name, rrd_get_error());
    }
    return failed;
}
