/* count-pblts.c
 *
 * Mark Johnson, 7th June 1997
 *
 * Reads the f2-21.txt treebank format trees from stdin, annotates
 * their category label with that of their parent, and counts
 * the right-binarized binary branching local trees.
 *
 * readlabel() ignores all but the first component of node labels
 *
 * We ignore the terminals, and only count the preterminals and nonterminals
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "local-trees.h"	/* local tree count format */
#include "mmm.h"		/* memory debugger */
#include "hash-string.h" 	/* hash tables and string-index tables */
#include "vindex.h"
#include "tree.h"


void count_local_trees(const tree t, vihashl localtree_ht);
	/* adds local tree counts from local trees in tree to localtree_ht */

void write_local_trees(const vihashl localtree_ht, si_t si);
	/* writes local tree hash table to stdout */


void count_local_bintrees(const bintree t, vihashl localtree_ht)
{
  si_index      e[3];
  struct vindex vi = {0, e};

  assert(t);

  vi.n = 0;
  vi.e[vi.n++] = t->label;

  if (t->left) {
    vi.e[vi.n++] = t->left->label;
    count_local_bintrees(t->left, localtree_ht);

    if (t->right) {
      vi.e[vi.n++] = t->right->label;
      count_local_bintrees(t->right, localtree_ht);
    }
    vihashl_inc(localtree_ht, &vi, 1);	/* save this list */
  }
  else
    assert(!t->right);			/* non-NULL t->right when t->left NULL */
}

void write_local_trees(const vihashl localtree_ht, si_t si)
{
  vihashlit hit;
  vindex  vi;
  long    count;
  size_t  i;
  char    *string;

  for (hit = vihashlit_init(localtree_ht); vihashlit_ok(hit); hit = vihashlit_next(hit)) {
    vi = (vindex) hit.key;
    assert(vi->n > 0);
    assert(vi->n <= MAXRHS);
    count = hit.value;
    string =  si_index_string(si, vi->e[0]);
    assert(string);
    printf("%ld\t%s " REWRITES, count, string);
    
    for (i=1; i<vi->n; i++) {
      string = si_index_string(si, vi->e[i]);
      assert(string);
      printf(" %s", string);
    }

    printf("\n");
  }}

int main(int argc, char **argv)
{
  tree     t, p;
  bintree  bt;
  si_t     si = make_si(100);
  FILE     *fp = stdin;
  vihashl  localtree_ht = make_vihashl(1000);

  /* read the trees and save the local tree counts */

  while ((t = readtree_root(fp, si))) {
    p = collapse_identical_unary(t); free_tree(t);
    t = annotate_with_parent(p, si); free_tree(p);
    /* p = remove_parent_annotation(t, si); */
    /* display_tree(stdout, p, si, 0); printf("\n\n"); */

    bt = right_binarize(t, si); free_tree(t);
    count_local_bintrees(bt, localtree_ht);
    free_bintree(bt);
  }

  write_local_trees(localtree_ht, si);
  si_free(si);
  free_vihashl(localtree_ht);
  assert(mmm_bytes_allocated == 0);
  exit(EXIT_SUCCESS);
}
