/* blockalloc.h
 *
 * Mark Johnson, 14th November 1998
 *
 * A memory allocator in which all blocks must be freed
 * simultaneously.
 */

#ifndef BLOCKALLOC_H
#define BLOCKALLOC_H

#include <stdlib.h>

void blockalloc_free_all(void);
void * blockalloc_malloc(size_t n_char);

#endif
