/* 
 * File:   quicksort.c
 * Author: Peter Stamfest
 *
 * Created on August 26, 2014, 6:02 PM
 *
 * This file is in the public domain.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "quicksort.h"

static void swap(void *x, void *y, size_t l) {
    char *a = x, *b = y, c;
    while(l--) {
        c = *a;
        *a++ = *b;
        *b++ = c;
    }
}
 
static void sort(char *array, size_t size, compar_ex_t cmp, int begin, int end, void *extra) {
    if (begin >= end) return;
    // begin < end
    
    int l = begin + size, r = end;
    void *p = array + begin;  // pivot element
    
    while (l < r) {
        if (cmp(array + l, p, extra) <= 0) {
            l += size;
        } else if(cmp(array + r, p, extra) >= 0) {
            r -= size;
        } else {
            swap(array + l, array + r, size);
        }
    }
    
    if (cmp(array + l, p, extra) <= 0) {
        swap(array + l, p, size);
        l -= size;
    } else {
        l -= size;
        swap(array + l, p, size);
    }
    
    // recurse
    sort(array, size, cmp, begin, l,   extra);
    sort(array, size, cmp, r,     end, extra);
}

void quick_sort(void *array, size_t size, size_t nitems, compar_ex_t *cmp, void *extra) {
    sort(array, size, cmp, 0, (nitems - 1) * size, extra);
}
