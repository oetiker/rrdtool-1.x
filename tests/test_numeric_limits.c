#include "rrd_tool.h"
#include "rrd_strtod.h"
#include "rrd_snprintf.h"
#include <limits.h>

/* One line per assertion so failure-only code adds no uncovered lines. */
#define CHECK(c, msg) do { if (!(c)) { fprintf(stderr, "%s\n", msg); failed = 1; } } while (0)

int main(void)
{
    const char *numbers[] = {
        "1e99999999999999999999999", "1e-2147483647",
        "1.00e-2147483647", "1e2147483647"
    };
    char formatted[64];
    int failed = 0;

    for (size_t i = 0; i < sizeof(numbers) / sizeof(numbers[0]); i++) {
        errno = 0;
        CHECK(rrd_strtod(numbers[i], NULL) == HUGE_VAL && errno == ERANGE, numbers[i]);
    }
    CHECK(rrd_snprintf(formatted, sizeof(formatted), "%lld", LLONG_MIN) == 20, "LLONG_MIN length");
    CHECK(strcmp(formatted, "-9223372036854775808") == 0, "LLONG_MIN text");
    return failed;
}
