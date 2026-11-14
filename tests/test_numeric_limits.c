#include "rrd_tool.h"
#include "rrd_strtod.h"
#include "rrd_snprintf.h"
#include <limits.h>

static int failed;

/* One line per assertion, so no line runs only when a check fails. */
#define CHECK(c, ...) do { if (!(c)) { fprintf(stderr, __VA_ARGS__); failed = 1; } } while (0)

int main(void)
{
    const char *numbers[] = {
        "1e99999999999999999999999", "1e-2147483647",
        "1.00e-2147483647", "1e2147483647"
    };
    char formatted[64];
    double value;
    int length;

    for (size_t i = 0; i < sizeof(numbers) / sizeof(numbers[0]); i++) {
        errno = 0;
        value = rrd_strtod(numbers[i], NULL);
        CHECK(value == HUGE_VAL && errno == ERANGE, "rrd_strtod(\"%s\") = %g, errno %d\n", numbers[i], value, errno);
    }
    length = rrd_snprintf(formatted, sizeof(formatted), "%lld", LLONG_MIN);
    CHECK(length == 20 && strcmp(formatted, "-9223372036854775808") == 0, "rrd_snprintf(LLONG_MIN) = \"%s\" (%d)\n", formatted, length);
    return failed;
}
