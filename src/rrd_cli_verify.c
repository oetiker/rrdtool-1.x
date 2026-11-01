/*****************************************************************************
 * RRDtool 1.11.0 Copyright by Tobi Oetiker, 1997-2026
 * rrd_cli_verify.c  Read-only integrity checks and UNKNOWN diagnostics
 *****************************************************************************/

#include "rrd_cli.h"
#include "rrd_rpncalc.h"

#include <stdarg.h>
#include <stdint.h>

typedef struct {
    int json;
    unsigned long errors;
    const char *filename;
} verify_output_t;

typedef struct {
    unsigned long known;
    unsigned long unknown;
    unsigned long infinite;
} value_counts_t;

static void issue(verify_output_t *out, const char *code,
                  const char *format, ...)
{
    char message[512];
    va_list args;

    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    if (out->json) {
        if (out->errors)
            fputc(',', stdout);
        fputs("{\"code\":", stdout);
        rrd_json_string(stdout, code);
        fputs(",\"message\":", stdout);
        rrd_json_string(stdout, message);
        fputc('}', stdout);
    } else {
        printf("%s: %s: %s\n", out->filename, code, message);
    }
    out->errors++;
}

static int name_compare(const void *left, const void *right)
{
    return strcmp(*(const char *const *) left, *(const char *const *) right);
}

static int ds_type(const char *name)
{
    static const char *types[] = {
        "COUNTER", "ABSOLUTE", "GAUGE", "DERIVE", "COMPUTE",
        "DCOUNTER", "DDERIVE"
    };
    size_t i;

    for (i = 0; i < sizeof(types) / sizeof(types[0]); i++)
        if (strcmp(name, types[i]) == 0)
            return (int) i;
    return -1;
}

static int cf_type(const rra_def_t *rra)
{
    int cf;

    if (memchr(rra->cf_nam, '\0', CF_NAM_SIZE) == NULL)
        return -1;
    cf = (int) rrd_cf_conv(rra->cf_nam);
    if (cf == -1)
        rrd_clear_error();
    return cf;
}

static void check_compute(verify_output_t *out, const ds_def_t *ds,
                          unsigned long index)
{
    int i;

    for (i = 0; i < DS_CDEF_MAX_RPN_NODES; i++) {
        rpn_cdefds_t node;
        unsigned int op;

        memcpy(&node, (const char *) &ds->par[DS_cdef] + i * sizeof(node),
               sizeof(node));
        op = (unsigned char) node.op;
        if (op == OP_END) {
            if (i == 0)
                issue(out, "compute_program", "DS %lu has an empty COMPUTE program", index);
            return;
        }
        if (op > OP_ROUND || op == OP_TIME || op == OP_LTIME ||
            op == OP_PREV || op == OP_COUNT || op == OP_TREND ||
            op == OP_TRENDNAN || op == OP_PREDICT || op == OP_PREDICTSIGMA ||
            op == OP_PREDICTPERC || op == OP_STEPWIDTH || op == OP_NEWDAY ||
            op == OP_NEWWEEK || op == OP_NEWMONTH || op == OP_NEWYEAR) {
            issue(out, "compute_program", "DS %lu has an invalid COMPUTE operator %u",
                  index, op);
            return;
        }
        if ((op == OP_VARIABLE || op == OP_PREV_OTHER) &&
            (node.val < 0 || (unsigned long) node.val >= index)) {
            issue(out, "compute_reference", "DS %lu refers to a non-preceding DS %d",
                  index, node.val);
            return;
        }
    }
    issue(out, "compute_program", "DS %lu COMPUTE program has no end marker", index);
}

static void check_sources(verify_output_t *out, const rrd_t *rrd)
{
    unsigned long i, count = 0;
    const char **names = malloc(rrd->stat_head->ds_cnt * sizeof(*names));

    if (names == NULL) {
        issue(out, "allocation_failed", "could not allocate the DS name index");
        return;
    }
    for (i = 0; i < rrd->stat_head->ds_cnt; i++) {
        const ds_def_t *ds = &rrd->ds_def[i];
        const unsigned char *p = (const unsigned char *) ds->ds_nam;
        int type;

        if (memchr(ds->ds_nam, '\0', DS_NAM_SIZE) == NULL) {
            issue(out, "ds_name", "DS %lu name is not terminated", i);
        } else {
            names[count++] = ds->ds_nam;
            if (*p == '\0')
                issue(out, "ds_name", "DS %lu name is empty", i);
            for (; *p; p++) {
                if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                      (*p >= '0' && *p <= '9') || *p == '_' || *p == '-')) {
                    issue(out, "ds_name", "DS %lu name contains an invalid character", i);
                    break;
                }
            }
        }
        if (memchr(ds->dst, '\0', DST_SIZE) == NULL ||
            (type = ds_type(ds->dst)) == -1) {
            issue(out, "ds_type", "DS %lu has an invalid type", i);
        } else if (type == DST_CDEF) {
            check_compute(out, ds, i);
        } else if (!isnan(ds->par[DS_min_val].u_val) &&
                   !isnan(ds->par[DS_max_val].u_val) &&
                   ds->par[DS_min_val].u_val > ds->par[DS_max_val].u_val) {
            issue(out, "ds_range", "DS %lu minimum exceeds its maximum", i);
        }
        if (memchr(rrd->pdp_prep[i].last_ds, '\0', LAST_DS_LEN) == NULL)
            issue(out, "last_input", "DS %lu last input is not terminated", i);
    }
    if (count > 1)
        qsort(names, count, sizeof(*names), name_compare);
    for (i = 1; i < count; i++)
        if (strcmp(names[i - 1], names[i]) == 0)
            issue(out, "duplicate_ds", "DS name '%s' is repeated", names[i]);
    free(names);
}

static void check_archives(verify_output_t *out, const rrd_t *rrd,
                           const rrd_file_t *file)
{
    unsigned long i;
    size_t expected = file->header_len;

    for (i = 0; i < rrd->stat_head->rra_cnt; i++) {
        const rra_def_t *rra = &rrd->rra_def[i];
        int cf = cf_type(rra);

        /* rrd_open has already bounded the sum and products by file_len. */
        expected += rra->row_cnt * rrd->stat_head->ds_cnt * sizeof(rrd_value_t);
        if (cf == -1) {
            issue(out, "rra_type", "RRA %lu has an invalid consolidation function", i);
            continue;
        }
        if (rrd->rra_ptr[i].cur_row >= rra->row_cnt)
            issue(out, "rra_pointer", "RRA %lu current row %lu is outside its %lu rows",
                  i, rrd->rra_ptr[i].cur_row, rra->row_cnt);
        if (cf <= CF_LAST) {
            double xff = rra->par[RRA_cdp_xff_val].u_val;

            if (!(xff >= 0.0 && xff < 1.0))
                issue(out, "rra_xff", "RRA %lu xff is outside [0, 1)", i);
        } else {
            unsigned long dep = rra->par[RRA_dependent_rra_idx].u_cnt;
            int partner = dep < rrd->stat_head->rra_cnt ? cf_type(&rrd->rra_def[dep]) : -1;
            int match;

            if (cf == CF_HWPREDICT || cf == CF_MHWPREDICT)
                match = partner == CF_SEASONAL;
            else if (cf == CF_SEASONAL || cf == CF_DEVSEASONAL)
                match = partner == CF_HWPREDICT || partner == CF_MHWPREDICT;
            else
                match = partner == CF_DEVSEASONAL;
            if (!match)
                issue(out, "hw_dependency", "RRA %lu has an invalid Holt-Winters partner %lu",
                      i, dep);
            if (cf == CF_HWPREDICT || cf == CF_MHWPREDICT) {
                double alpha = rra->par[RRA_hw_alpha].u_val;
                double beta = rra->par[RRA_hw_beta].u_val;

                if (!(alpha > 0.0 && alpha < 1.0 && beta >= 0.0 && beta <= 1.0))
                    issue(out, "hw_parameters", "RRA %lu has an invalid alpha or beta", i);
            } else if (cf == CF_SEASONAL || cf == CF_DEVSEASONAL) {
                double gamma = rra->par[RRA_seasonal_gamma].u_val;
                double window = rra->par[RRA_seasonal_smoothing_window].u_val;

                if (!(gamma > 0.0 && gamma < 1.0))
                    issue(out, "hw_parameters", "RRA %lu has an invalid gamma", i);
                if (atoi(rrd->stat_head->version) >= 4 &&
                    !(window >= 0.0 && window <= 1.0))
                    issue(out, "hw_parameters", "RRA %lu has an invalid smoothing window", i);
            }
        }
    }
    if (expected != file->file_len)
        issue(out, "file_size", "file has %llu trailing bytes",
              (unsigned long long) (file->file_len - expected));
}

static value_counts_t *count_values(verify_output_t *out, rrd_t *rrd,
                                    rrd_file_t *file)
{
    rrd_value_t values[1024];
    unsigned long rra, ds_count = rrd->stat_head->ds_cnt;
    size_t count = rrd->stat_head->rra_cnt * ds_count;
    value_counts_t *counts;

    /* The cdp_prep count is checked by rrd_open, but our element size
     * differs, so check this allocation independently. */
    if (count > (size_t) -1 / sizeof(*counts)) {
        issue(out, "allocation_failed", "UNKNOWN counter array is too large");
        return NULL;
    }
    counts = calloc(count, sizeof(*counts));
    if (counts == NULL) {
        issue(out, "allocation_failed", "could not allocate UNKNOWN counters");
        return NULL;
    }
    if (rrd_seek(file, file->header_len, SEEK_SET) != 0)
        goto read_error;
    for (rra = 0; rra < rrd->stat_head->rra_cnt; rra++) {
        size_t total = rrd->rra_def[rra].row_cnt * ds_count;
        size_t offset = 0;

        while (offset < total) {
            size_t chunk = total - offset;
            size_t i;

            if (chunk > sizeof(values) / sizeof(values[0]))
                chunk = sizeof(values) / sizeof(values[0]);
            if (rrd_read(file, values, chunk * sizeof(*values)) !=
                (ssize_t) (chunk * sizeof(*values)))
                goto read_error;
            for (i = 0; i < chunk; i++) {
                value_counts_t *cell = &counts[rra * ds_count + (offset + i) % ds_count];

                if (isnan(values[i]))
                    cell->unknown++;
                else if (isinf(values[i]))
                    cell->infinite++;
                else
                    cell->known++;
            }
            offset += chunk;
        }
    }
    return counts;
read_error:
    issue(out, "read_failed", "could not read archive values: %s",
          rrd_test_error() ? rrd_get_error() : "short read");
    rrd_clear_error();
    free(counts);
    return NULL;
}

static void print_unknown(const rrd_t *rrd, const value_counts_t *counts,
                          int json)
{
    unsigned long i, j, ds_count = rrd->stat_head->ds_cnt;

    if (json)
        fputs(",\"unknown_diagnostics\":{\"historical_causes_recorded\":false,"
              "\"possible_causes\":[\"explicit_unknown_input\",\"heartbeat_gap\","
              "\"range_filter\",\"xff_threshold\",\"undefined_computation\","
              "\"holt_winters_initialization\"],\"data_sources\":[", stdout);
    else
        puts("UNKNOWN diagnostics: historical causes are not recorded in the RRD.");
    for (i = 0; i < ds_count; i++) {
        const ds_def_t *ds = &rrd->ds_def[i];
        const pdp_prep_t *pdp = &rrd->pdp_prep[i];
        int compute = strcmp(ds->dst, "COMPUTE") == 0;

        if (json) {
            if (i)
                fputc(',', stdout);
            fputs("{\"name\":", stdout);
            rrd_json_string(stdout, ds->ds_nam);
            fputs(",\"last_input\":", stdout);
            rrd_json_string(stdout, pdp->last_ds);
            fprintf(stdout, ",\"last_input_unknown\":%s",
                    pdp->last_ds[0] == '\0' || strcmp(pdp->last_ds, "U") == 0 ?
                    "true" : "false");
            fprintf(stdout, ",\"pdp_unknown_seconds\":%lu,\"heartbeat\":",
                    pdp->scratch[PDP_unkn_sec_cnt].u_cnt);
            if (compute)
                fputs("null", stdout);
            else
                fprintf(stdout, "%lu", ds->par[DS_mrhb_cnt].u_cnt);
            fputs(",\"minimum\":", stdout);
            rrd_json_number(stdout, compute ? DNAN : ds->par[DS_min_val].u_val);
            fputs(",\"maximum\":", stdout);
            rrd_json_number(stdout, compute ? DNAN : ds->par[DS_max_val].u_val);
            fputc('}', stdout);
        } else {
            printf("DS %s: last input '%s', current PDP unknown seconds %lu",
                   ds->ds_nam, pdp->last_ds, pdp->scratch[PDP_unkn_sec_cnt].u_cnt);
            if (!compute) {
                printf(", heartbeat %lu", ds->par[DS_mrhb_cnt].u_cnt);
                fputs(", minimum ", stdout);
                rrd_json_number(stdout, ds->par[DS_min_val].u_val);
                fputs(", maximum ", stdout);
                rrd_json_number(stdout, ds->par[DS_max_val].u_val);
            }
            putchar('\n');
        }
    }
    if (json)
        fputs("],\"archives\":[", stdout);
    for (i = 0; i < rrd->stat_head->rra_cnt; i++) {
        int cf = cf_type(&rrd->rra_def[i]);

        if (json) {
            if (i)
                fputc(',', stdout);
            fprintf(stdout, "{\"index\":%lu,\"cf\":", i);
            rrd_json_string(stdout, rrd->rra_def[i].cf_nam);
            fprintf(stdout, ",\"rows\":%lu,\"pdp_per_row\":%lu",
                    rrd->rra_def[i].row_cnt, rrd->rra_def[i].pdp_cnt);
            if (cf <= CF_LAST) {
                fputs(",\"xff\":", stdout);
                rrd_json_number(stdout, rrd->rra_def[i].par[RRA_cdp_xff_val].u_val);
            }
            fputs(",\"data_sources\":[", stdout);
        }
        for (j = 0; j < ds_count; j++) {
            const value_counts_t *cell = &counts[i * ds_count + j];

            if (json) {
                if (j)
                    fputc(',', stdout);
                fputs("{\"name\":", stdout);
                rrd_json_string(stdout, rrd->ds_def[j].ds_nam);
                fprintf(stdout, ",\"known\":%lu,\"unknown\":%lu,\"infinite\":%lu",
                        cell->known, cell->unknown, cell->infinite);
                if (cf <= CF_LAST)
                    fprintf(stdout, ",\"cdp_unknown_datapoints\":%lu",
                            rrd->cdp_prep[i * ds_count + j].scratch[CDP_unkn_pdp_cnt].u_cnt);
                fputc('}', stdout);
            } else {
                printf("RRA %lu (%s), DS %s: %lu known, %lu UNKNOWN, %lu infinite",
                       i, rrd->rra_def[i].cf_nam, rrd->ds_def[j].ds_nam,
                       cell->known, cell->unknown, cell->infinite);
                if (cf <= CF_LAST) {
                    printf(", current CDP unknown PDPs %lu, xff ",
                           rrd->cdp_prep[i * ds_count + j].scratch[CDP_unkn_pdp_cnt].u_cnt);
                    rrd_json_number(stdout, rrd->rra_def[i].par[RRA_cdp_xff_val].u_val);
                }
                putchar('\n');
            }
        }
        if (json)
            fputs("]}", stdout);
    }
    if (json)
        fputs("]}", stdout);
    else
        puts("Possible causes include explicit U input, heartbeat gaps, range limits, "
             "insufficient known PDPs (xff), or undefined COMPUTE/Holt-Winters results. "
             "These are possibilities, not reconstructed sample histories.");
}

int rrd_cli_verify(int argc, const char **argv, int json)
{
    verify_output_t out;
    const char *filename = NULL;
    const char *usage_error = NULL;
    int diagnose = 0, options = 1, i;
    rrd_t rrd;
    rrd_file_t *file = NULL;
    value_counts_t *counts = NULL;

    for (i = 1; i < argc; i++) {
        if (options && strcmp(argv[i], "--") == 0)
            options = 0;
        else if (options && strcmp(argv[i], "--json") == 0)
            json = 1;
        else if (options && strcmp(argv[i], "--diagnose-unknown") == 0)
            diagnose = 1;
        else if ((options && argv[i][0] == '-') || filename != NULL)
            usage_error = "usage: rrdtool verify [--json] [--diagnose-unknown] file.rrd";
        else
            filename = argv[i];
    }
    out.json = json;
    out.errors = 0;
    out.filename = filename ? filename : "";
    if (json) {
        fputs("{\"filename\":", stdout);
        rrd_json_string(stdout, out.filename);
        fputs(",\"errors\":[", stdout);
    }
    rrd_init(&rrd);
    if (usage_error || filename == NULL) {
        issue(&out, "usage", "%s", usage_error ? usage_error :
              "usage: rrdtool verify [--json] [--diagnose-unknown] file.rrd");
        goto done;
    }
    file = rrd_open(filename, &rrd, RRD_READONLY | RRD_LOCK_BLOCK);
    if (file == NULL) {
        issue(&out, "open_failed", "%s", rrd_get_error());
        rrd_clear_error();
        goto done;
    }
    check_sources(&out, &rrd);
    check_archives(&out, &rrd, file);
    if (diagnose && out.errors == 0)
        counts = count_values(&out, &rrd, file);
done:
    if (json) {
        fprintf(stdout, "],\"valid\":%s", out.errors ? "false" : "true");
        if (file) {
            fprintf(stdout, ",\"step\":%lu,\"ds_count\":%lu,\"rra_count\":%lu,\"last_update\":%lld",
                    rrd.stat_head->pdp_step, rrd.stat_head->ds_cnt,
                    rrd.stat_head->rra_cnt, (long long) rrd.live_head->last_up);
        }
    }
    if (counts)
        print_unknown(&rrd, counts, json);
    if (json)
        fputs("}\n", stdout);
    else
        printf("%s: %s (%lu errors)\n", out.filename,
               out.errors ? "INVALID" : "OK", out.errors);
    free(counts);
    if (file)
        rrd_close(file);
    rrd_free(&rrd);
    return out.errors || ferror(stdout) ? 1 : 0;
}
