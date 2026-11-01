#include "rrd_tool.h"
#include "rrd_strtod.h"
#include "rrd_snprintf.h"
#include <limits.h>

int main(void)
{
    const char *numbers[] = {
        "1e99999999999999999999999", "1e-2147483647",
        "1.00e-2147483647", "1e2147483647"
    };
    char formatted[64];

    for (size_t i = 0; i < sizeof(numbers) / sizeof(numbers[0]); i++) {
        errno = 0;
        if (rrd_strtod(numbers[i], NULL) != HUGE_VAL || errno != ERANGE)
            return 1;
    }
    if (rrd_snprintf(formatted, sizeof(formatted), "%lld", LLONG_MIN) != 20 ||
        strcmp(formatted, "-9223372036854775808") != 0)
        return 1;
    return 0;
}
