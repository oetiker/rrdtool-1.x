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

int main(void)
{
    rpn_cdefds_t program[3] = {{0}};
    char *text = NULL;

    program[0].op = OP_NUMBER;
    program[0].val = 1;
    program[1].op = OP_NUMBER;
    program[1].val = 2;
    program[2].op = OP_END;
    fail_at = 2; /* Grow the comma after the first value. */
    rpn_compact2str(program, NULL, &text);
    if (!rrd_test_error() || text == NULL || strcmp(text, "1") != 0)
        return 1;
    free(text);
    text = NULL;
    program[0].op = OP_END;
    allocations = 0;
    fail_at = 1;
    rrd_clear_error();
    rpn_compact2str(program, NULL, &text);
    if (text != NULL || !strstr(rrd_get_error(), "failed to alloc memory"))
        return 1;
    return 0;
}
