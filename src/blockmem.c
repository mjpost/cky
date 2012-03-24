/* blockalloc.c
 *
 * Mark Johnson, 14th November 1998
 *
 * A memory allocator in which all blocks must be freed
 * simultaneously.
 */

#include <stdlib.h>
#include <mmm.h>

#define NUNITS(N)    (1 + (N-1)/sizeof(align))

typedef int align;

/* blocksize is the number of align-sized units in the memory
 * block grabbed from malloc.  This setting will give approximately
 * 1Mb-sized blocks.  I subtract 8 align-sized units from exactly
 * 1Mb so that the block with my and the system's overhead will
 * probably still fit in a real 1Mb-size unit
 */

#define BLOCKSIZE    (262144-8)

struct blockmem {
  align           data[BLOCKSIZE];
  size_t          top;
  struct blockmem *next;
};

static struct blockmem *current_block;

void
blockmem_free_all(void)
{
  struct blockmem *p = current_block;

  while (p) {
    struct blockmem *q = p->next;
    FREE(p);
    p = q;
  }

  current_block = NULL;
}

void *
blockmem_malloc(size_t n_char)
{
  struct blockmem *p = current_block;
  size_t n = NUNITS(n_char);

  assert(n<=BLOCKSIZE);

  if (p == NULL || p->top < n) {
    p = MALLOC(sizeof(struct blockmem));
    p->next = current_block;
    p->top = BLOCKSIZE;
  }

  p->top -= n;
  return (void *) &p->data[p->top];
}
