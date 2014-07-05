//
// rrd_strtod.c
//
// strtod.c
//
// Convert string to double
//
// Copyright (C) 2002 Michael Ringgaard. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
// 3. Neither the name of the project nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.
//

#include <errno.h>
#include <ctype.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>

#include "rrd.h"
#include "rrd_strtod.h"

/* returns 2 on success */
/* i.e. if the whole string has been converted to a double successfully */
unsigned int rrd_strtoding(const char * str, char ** endptr, double * dbl) {
    *dbl = rrd_strtod( str, endptr );

    if( endptr != NULL && *endptr == str ) {
        /* no conversion has been done */
        /* for inputs like "abcdj", i.e. no number at all */
        rrd_set_error("Cannot convert %s to float", str);
        return 0;
    } else if( endptr != NULL && endptr[0] != '\0' ) {
        /* conversion has been done, but whole string is not a number */
        /* for inputs like "33.343djdjk" */
        rrd_set_error("Cannot convert %s to float", str);
        return 1;
    } else if( endptr != NULL && endptr[0] == '\0' ) {
        /* conversion successfully done */
        /* for inputs that are totally numbers "23.343" */
        return 2;
    } else {
        /* just to be safe */
        rrd_set_error("..");
        return 3;
    }
}

double rrd_strtod(const char *str, char **endptr) {
    double number;
    int exponent;
    int negative;
    char *p = (char *) str;
    double p10;
    int n;
    int num_digits;
    int num_decimals;

    // Skip leading whitespace
    while (isspace(*p)) p++;

    // Handle optional sign
    negative = 0;
    switch (*p) {
       case '-': negative = 1; // Fall through to increment position
       case '+': p++;
    }

    number = 0.;
    exponent = 0;
    num_digits = 0;
    num_decimals = 0;

    // Process string of digits
    while (isdigit(*p)) {
        number = number * 10. + (*p - '0');
        p++;
        num_digits++;
    }

    // Process decimal part
    if (*p == '.') {
        p++;

        while (isdigit(*p)) {
            number = number * 10. + (*p - '0');
            p++;
            num_digits++;
            num_decimals++;
        }

        exponent -= num_decimals;
    }

    if (num_digits == 0) {
        errno = ERANGE;
        return 0.0;
    }

    // Correct for sign
    if (negative) number = -number;

    // Process an exponent string
    if (*p == 'e' || *p == 'E') {
        // Handle optional sign
        negative = 0;
        switch (*++p) {
            case '-': negative = 1; // Fall through to increment pos
            case '+': p++;
        }

        // Process string of digits
        n = 0;
        while (isdigit(*p)) {
            n = n * 10 + (*p - '0');
            p++;
        }

        if (negative) {
            exponent -= n;
        } else {
            exponent += n;
        }
    }

    if (exponent < DBL_MIN_EXP || exponent > DBL_MAX_EXP) {
        errno = ERANGE;
        return HUGE_VAL;
    }

    // Scale the result
    p10 = 10.;
    n = exponent;
    if (n < 0) n = -n;
    while (n) {
        if (n & 1) {
            if (exponent < 0) {
                number /= p10;
            } else {
                number *= p10;
            }
        }
        n >>= 1;
        p10 *= p10;
    }

    if (number == HUGE_VAL) errno = ERANGE;
    if (endptr) *endptr = p;

    return number;
}
