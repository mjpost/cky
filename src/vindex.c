/* vindex.c  -- Vectors of si_index
 *
 * Mark Johnson, 22nd May 1997
 */

#include <string.h>
#include "vindex.h"
#include "mmm.h"
#include "hash-templates.h"

vindex 
make_vindex(const size_t n)
{
  vindex v = MALLOC(sizeof(struct vindex));
  v->e = MALLOC(n*sizeof(si_index));
  return v;
}

static size_t 
vindex_hash(const vindex v)
{
  int      i;
  unsigned h = 0, g;
  for (i=v->n-1; i>=0; i--) {
    h = (h << 4) + v->e[i];
    if ((g = h&0xf0000000)) {
      h = h ^ (g >> 24);
      h = h ^ g;
    }}
  return h;
}

static int 
vindex_cmp(const vindex v1, const vindex v2)
{
  if (v1->n==v2->n)
    return memcmp(v1->e, v2->e, v1->n*sizeof(si_index));
  else {
    if (v1->n<v2->n)
      return -1;
    else
      return +1;
  }}

static vindex 
vindex_copy(const vindex v)
{
  vindex v1 = MALLOC(sizeof(struct vindex));
  v1->n = v->n;
  v1->e = MALLOC(v1->n*sizeof(si_index));
  memcpy(v1->e, v->e, v1->n*sizeof(si_index));
  return v1;
}

void 
vindex_free(vindex v)
{
  FREE(v->e);
  FREE(v);
}

HASH_CODE_ADD(vihashl, vindex, long, vindex_hash, vindex_cmp, 
	      vindex_copy, vindex_free, NULL, 0, NO_OP)

