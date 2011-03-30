/* local-trees.h
 *
 * Mark Johnson, 21st May 1997
 *
 * Identifiers for local tree counts
 */

#ifndef LOCAL_TREES_H
#define LOCAL_TREES_H

#define REWRITES 	"-->"
#define CATSEP		"-=|"		/* separates categories */
#define BINSEP		' '		/* joins new (binarized) categories */
#define PARENTSEP	'^'		/* parent label follows this char */

typedef unsigned long 	si_index;	/* type of category indices */
#define SI_INDEX_MAX	ULONG_MAX

typedef double	FLOAT;		/* type of floating point calculations */

#define MAXRHS		128	/* max length of prodn rhs */
#define MAXLABELLEN	2048	/* max length of a label */
#define MAXBLABELLEN	2048	/* max length of a binarized label */ 
#define NLABELS 	128	/* guesstimate of no of category labels */
#define CHART_CELLS	16384	/* initial size of chart cells */

#endif
