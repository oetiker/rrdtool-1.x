/* 
 * File:   quicksort.h
 * Author: Peter Stamfest
 *
 * Created on August 26, 2014, 6:07 PM
 * 
 * This file is in the public domain.
 */

#ifndef QUICKSORT_H
#define	QUICKSORT_H

#ifdef	__cplusplus
extern "C" {
#endif

typedef int compar_ex_t(const void *a, const void *b, const void *extra);

/**
 * An extended quick sort algorithm compared to stdlib qsort. Takes an extra 
 * parameter passed through to the comparation function (of type compar_ex_t).
 * 
 * @param array		The array to sort
 * @param size		The size of array elements
 * @param nitems	The number of array elements to sort
 * @param cmp		The comparation function.
 * @param extra		Extra data passed to the comparation function
 */
void quick_sort(void *array, size_t size, size_t nitems, compar_ex_t *cmp, void *extra);

#ifdef	__cplusplus
}
#endif

#endif	/* QUICKSORT_H */

