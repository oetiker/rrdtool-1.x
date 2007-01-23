/*****************************************************************************
 * RRDtool 1.2.18  Copyright by Tobi Oetiker, 1997-2006
 *****************************************************************************
 * rrd_datalang  A system for passing named and typed parameters between
 *               the different parts of rrdtool
 *
 * In rrdtool thre are a number of places where large and complex
 * data structures have to be passed from one function to another
 * eg when rrd_info returns its findings, but also when a function like
 * rrd_graph get called.
 *
 * At the moment function calling is done with an argc/argv type interface
 * it's special property is that all data is passed as strings, which can lead
 * to unnecessary conversions being performed when rrdtool functions are called
 * from a typed language
 *
 * Data returns from functions is not standardized at all, which is
 * efficient in the sense that the data return interface can be tailord to
 * the specific needs of the function at hand, but it also leads to
 * increassed probability for implementation errors as things have to be
 * reinvented for each function. Also adding new functions into all the
 * language bindings is quite cumbersom.
 *
 * Therefore I want to develop a standardized interface for passing named
 * and typed data into functions and for returning data from functions to
 * their callers.  I am thinking about working of the code in rrd_info.c ...
 *
 * Does anyone have experiance in this field or any pointers to read up on
 * related work ? Or maybe even an existing library for this ?
 *
 * Cheers
 * tobi / 2001-03-10
 * 
 *****************************************************************************/
