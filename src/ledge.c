/* ledge.c
 */

#include "ledge.h"
#include "local-trees.h"
#include "mmm.h"		/* memory debugger */
#include <assert.h>
#include <string.h>

void 
free_ledges(struct ledges *les)
{
  FREE(les->le);
  FREE(les);
}

/* tree_ledges() does not include terminals
 */

static size_t 
tree_ledges_(tree t, struct ledges *les, size_t i, size_t *right) 
{
  tree p;

  for (p=t; p; p=p->sibling) {
    size_t left = *right;

    if (p->subtrees) {
      i = tree_ledges_(p->subtrees, les, i, right);
      if (i >= les->n) {
	les->n *= 2;
	les->le = REALLOC(les->le, les->n*sizeof(*les->le));
      }
      assert(i < les->n);

      les->le[i].left = left;
      les->le[i].right = *right;
      les->le[i++].label = p->label;
    }
    else		/* skip terminals */
      *right = 1+left;
  }
  return(i);
}

struct ledges *
tree_ledges(tree t) {
  size_t right = 0;
  struct ledges *les = MALLOC(sizeof(struct ledges));
  les->n = 10;
  les->le = MALLOC(les->n* sizeof(*les->le));
  les->n = tree_ledges_(t, les, 0, &right);
  les->le = REALLOC(les->le, les->n*sizeof(*les->le));
  return les;
}

static int 
ledges_cmp(const struct ledge *le1, const struct ledge *le2) {
  return memcmp(le1, le2, sizeof(*le1));
}

static void 
ledges_sort(struct ledges *les) {
  qsort(les->le, les->n, sizeof(*les->le), 
	(int (*)(const void *, const void *)) &ledges_cmp);
}
  
size_t 
common_ledge_count(struct ledges *les1, struct ledges *les2)
{
  size_t i1 = 0, i2 = 0, count=0;

  ledges_sort(les1);
  ledges_sort(les2);

  while (i1 < les1->n && i2 < les2->n ) {
    int	cmp = ledges_cmp(les1->le+i1, les2->le+i2);
    if (cmp==0) {
      count++; i1++; i2++;
    }
    else {
      if (cmp < 0) i1++; else i2++;
    }}
  return count;
}
