#include "rrd_tool.h"
#include "rrd_rpncalc.h"

static int allocations, fail_at;
static void *test_malloc(size_t size)
{
    return ++allocations == fail_at ? NULL : malloc(size);
}

#define malloc test_malloc
#include "../src/rrd_modify.c"
#undef malloc

static int failed;
#define CHECK(c, ...) do { if (!(c)) { fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); failed = 1; } } while (0)

int main(void)
{
    stat_head_t in_head = {0}, out_head = {0};
    ds_def_t ds_def[2];
    pdp_prep_t pdp_prep[2];
    rrd_t in, out;
    rpn_cdefds_t *program;
    char ops[] = "cc";
    int in_rows = 0, out_rows = 0;
    short refs[] = {7, -1};
    size_t i;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    memset(ds_def, 0, sizeof(ds_def));
    memset(pdp_prep, 0, sizeof(pdp_prep));
    strcpy(ds_def[0].ds_nam, "a");
    strcpy(ds_def[0].dst, "GAUGE");
    strcpy(ds_def[1].ds_nam, "c");
    strcpy(ds_def[1].dst, "COMPUTE");
    program = (rpn_cdefds_t *) &ds_def[1].par[DS_cdef];
    program[0].op = OP_VARIABLE;
    program[1].op = OP_END;
    in_head.ds_cnt = 2;
    in.stat_head = &in_head;
    in.ds_def = ds_def;
    in.pdp_prep = pdp_prep;
    out.stat_head = &out_head;

    /* A reference to a DS the file does not have is left alone here:
     * rrd_open refuses such a program before modify sees it. */
    for (i = 0; i < sizeof(refs) / sizeof(refs[0]); i++) {
        program[0].val = refs[i];
        out_head.ds_cnt = 0;
        CHECK(copy_or_delete_DSs(&in, &out, ops) == 0,
              "copy with DS reference %d: %s", refs[i], rrd_get_error());
        program = (rpn_cdefds_t *) &out.ds_def[1].par[DS_cdef];
        CHECK(program[0].val == refs[i],
              "DS reference %d became %d", refs[i], program[0].val);
        program = (rpn_cdefds_t *) &ds_def[1].par[DS_cdef];
        free(out.ds_def);
        free(out.pdp_prep);
        out.ds_def = NULL;
        out.pdp_prep = NULL;
    }

    /* handle_rra_defs cannot allocate the RRA renumbering table. */
    rrd_clear_error();
    allocations = 0;
    fail_at = 1;
    CHECK(handle_rra_defs(&in, &out, NULL, 0, ops, 2, &in_rows, &out_rows) == -1
          && strcmp(rrd_get_error(), "Cannot allocate memory") == 0,
          "failed allocation: error \"%s\"", rrd_get_error());
    return failed;
}
