/*****************************************************************************
 * RRDtool 1.9.0 Copyright by Tobi Oetiker, 1997-2024
 * This code is stolen from rateup (mrtg-2.x) by Dave Rand
 *****************************************************************************
 * diff calculate the difference between two very long integers available as
 *      strings
 *****************************************************************************/

#include <ctype.h>
#include "rrd_tool.h"
#include "rrd_strtod.h"

double rrd_diff(
    char *a,
    char *b)
{
    char      res[LAST_DS_LEN + 1], *a1, *b1, *r1, *fix;
    int       c, x, m;
    char      a_neg = 0, b_neg = 0;
    double    result;

    while (!(isdigit((int) *a) || *a == 0)) {
        if (*a == '-')
            a_neg = 1;
        a++;
    }
    fix = a;
    while (isdigit((int) *fix))
        fix++;
    *fix = 0;           /* maybe there is some non digit data in the string */
    while (!(isdigit((int) *b) || *b == 0)) {
        if (*b == '-')
            b_neg = 1;
        b++;
    }
    fix = b;
    while (isdigit((int) *fix))
        fix++;
    *fix = 0;           /* maybe there is some non digit data in the string */
    if (!isdigit((int) *a) || !isdigit((int) *b))
        return DNAN;
    if (a_neg + b_neg == 1) /* can not handle numbers with different signs yet */
        return DNAN;
    a1 = &a[strlen(a) - 1];
    m = max(strlen(a), strlen(b));
    if (m > LAST_DS_LEN)
        return DNAN;    /* result string too short */

    r1 = &res[m + 1];
    for (b1 = res; b1 <= r1; b1++)
        *b1 = ' ';
    b1 = &b[strlen(b) - 1];
    r1[1] = 0;          /* Null terminate result */
    c = 0;
    for (x = 0; x < m; x++) {
        if (a1 >= a && b1 >= b) {
            *r1 = ((*a1 - c) - *b1) + '0';
        } else if (a1 >= a) {
            *r1 = (*a1 - c);
        } else {
            *r1 = ('0' - *b1 - c) + '0';
        }
        if (*r1 < '0') {
            *r1 += 10;
            c = 1;
        } else if (*r1 > '9') { /* 0 - 10 */
            *r1 -= 10;
            c = 1;
        } else {
            c = 0;
        }
        a1--;
        b1--;
        r1--;
    }
    if (c) {
        r1 = &res[m + 1];
        for (x = 0; isdigit((int) *r1) && x < m; x++, r1--) {
            *r1 = ('9' - *r1 + c) + '0';
            if (*r1 > '9') {
                *r1 -= 10;
                c = 1;
            } else {
                c = 0;
            }
        }
        if (rrd_strtodbl(res, NULL, &result, "expected a number") != 2){
            result = DNAN;
        }
        else {
            result = -result;
        }
    } else {
        if (rrd_strtodbl(res, NULL, &result, "expected a number") != 2){
            result = DNAN;
        }
    }
    if (a_neg + b_neg == 2) /* both are negatives, reverse sign */
        result = -result;

    return result;
}
