/* Exercise CLI failure handling at allocator, file, and locale boundaries. */
#include "rrd_cli.h"
#include <locale.h>
#include <stdarg.h>

static int fail_allocation, fail_seek, read_error, large_title, malloc_calls;
static int failed;

/* One line per assertion, so no line runs only when a check fails. */
#define CHECK(c, ...) do { if (!(c)) { fprintf(stderr, __VA_ARGS__); failed = 1; } } while (0)

static void *test_malloc(size_t size)
{
    malloc_calls++;
    return fail_allocation ? NULL : malloc(size);
}

static void *test_calloc(size_t count, size_t size)
{
    return fail_allocation ? NULL : calloc(count, size);
}

static int test_seek(rrd_file_t *file, off_t offset, int whence)
{
    (void) file;
    (void) offset;
    (void) whence;
    return fail_seek;
}

static ssize_t test_read(rrd_file_t *file, void *buffer, size_t count)
{
    (void) file;
    (void) buffer;
    (void) count;
    if (read_error)
        rrd_set_error("injected archive read failure");
    return -1;
}

static size_t test_strlen(const char *text)
{
    /* Supply a virtual length for the size guard without allocating a
     * near-SIZE_MAX string. The guard must run before reading its bytes. */
    return large_title ? (size_t) -2 : strlen(text);
}

/* Simulate the two libc locale interfaces together, without depending on
 * which language packs the test machine has installed. */
static struct lconv *test_localeconv(void)
{
    static struct lconv locale;
    locale.decimal_point = ",";
    return &locale;
}

static int test_snprintf(char *buffer, size_t size, const char *format, ...)
{
    va_list args;
    int length;
    char *point;

    va_start(args, format);
    length = vsnprintf(buffer, size, format, args);
    va_end(args);
    point = strchr(buffer, '.');
    if (point)
        *point = ',';
    return length;
}

#define malloc test_malloc
#define calloc test_calloc
#define rrd_read test_read
#define rrd_seek test_seek
#define localeconv test_localeconv
#define snprintf test_snprintf
#include "../src/rrd_cli_json.c"
#undef localeconv
#undef snprintf
#include "../src/rrd_cli_verify.c"
#define strlen test_strlen
#include "../src/rrd_graph.c"
#undef strlen
#undef malloc
#undef calloc
#undef rrd_read
#undef rrd_seek

static void test_encoding(void)
{
    char buffer[128] = "";
    FILE *stream = tmpfile();
    size_t length;

    CHECK(stream != NULL, "tmpfile: %s\n", strerror(errno));
    if (stream != NULL) {
        rrd_json_number(stream, 1.5);
        fputc(' ', stream);
        rrd_json_string(stream, "\xed\xa0\x80"); /* UTF-8 surrogate, not Unicode. */
        rewind(stream);
        length = fread(buffer, 1, sizeof(buffer) - 1, stream);
        buffer[length] = '\0';
        fclose(stream);
    }
    /* each byte of the invalid sequence becomes U+FFFD */
    CHECK(strcmp(buffer, "1.5 \"\\ufffd\\ufffd\\ufffd\"") == 0, "JSON encoding: %s\n", buffer);
}

int main(void)
{
    const char *args[] = {"xport"};
    verify_output_t out = {0, 0, "fixture"};
    stat_head_t header = {0};
    rra_def_t archive = {0};
    rrd_file_t file = {0};
    rrd_t rrd;
    image_title_t title;
    int before;

    test_encoding();
    rrd_init(&rrd);
    rrd.stat_head = &header;
    rrd.rra_def = &archive;
    header.ds_cnt = header.rra_cnt = archive.row_cnt = 1;
    fail_allocation = 1;
    rrd_clear_error();
    json_xport(1, args);
    CHECK(rrd_test_error() && strstr(rrd_get_error(), "allocating JSON xport"), "json_xport allocation failure: '%s'\n", rrd_get_error());
    rrd_clear_error();
    check_sources(&out, &rrd);
    CHECK(out.errors == 1, "check_sources allocation failure: %lu errors\n", out.errors);
    CHECK(count_values(&out, &rrd, &file) == NULL && out.errors == 2, "count_values allocation failure: %lu errors\n", out.errors);
    title = graph_title_split("title");
    CHECK(title.lines == NULL && title.count == 0, "graph_title_split allocation failure kept %d lines\n", title.count);
    fail_allocation = 0;
    large_title = 1;
    before = malloc_calls;
    title = graph_title_split("virtual large title");
    CHECK(title.lines == NULL && title.count == 0 && malloc_calls == before, "graph_title_split accepted an oversized title\n");
    large_title = 0;
    header.ds_cnt = (size_t) -1 / sizeof(value_counts_t) + 1;
    CHECK(count_values(&out, &rrd, &file) == NULL && out.errors == 3, "count_values size overflow: %lu errors\n", out.errors);
    header.ds_cnt = 1;
    for (int mode = 0; mode < 3; mode++) {
        fail_seek = mode == 0;
        read_error = mode == 2;
        rrd_clear_error();
        CHECK(count_values(&out, &rrd, &file) == NULL && out.errors == (unsigned long) (4 + mode) && !rrd_test_error(), "count_values I/O failure mode %d: %lu errors, '%s'\n", mode, out.errors, rrd_get_error());
    }
    return failed;
}
