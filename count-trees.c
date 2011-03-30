/* count-trees.c
 *
 * Mark Johnson, 21st May 1997
 *
 * Reads the f2-21.txt treebank format trees from stdin, and counts
 * the local trees.
 *
 * readlabel() ignores all but the first component of node labels
 *
 * Modified 10th December to count preterm -> term rules
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "local-trees.h"	/* local tree count format */
#include "mmm.h"		/* memory debugger */
#include "hash-string.h" 	/* hash tables and string-index tables */
#include "vindex.h"
#include "tree.h"

char root_label[] = "S1";

void count_local_trees(const tree tree, vihashl localtree_ht);
	/* adds local tree counts from local trees in tree to localtree_ht */

void write_local_trees(const vihashl localtree_ht, si_t si, si_index root);
	/* writes local tree hash table to stdout */

void count_local_trees(const tree t, vihashl localtree_ht)
{
  si_index      e[MAXRHS];
  struct vindex vi = {0, e};
  tree	        p;

  if (!t) return;

  vi.n = 0;
  vi.e[vi.n++] = t->label;

  for(p = t->subtrees; p; p=p->sibling) {
    assert(vi.n < MAXRHS);
    vi.e[vi.n++] = p->label;
    count_local_trees(p, localtree_ht);
  }

  vihashl_inc(localtree_ht, &vi, 1);	/* save this list */
}

void write_local_trees(const vihashl localtree_ht, si_t si, si_index root)
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
    if (vi->e[0] != root)
      continue;
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
  }


  for (hit = vihashlit_init(localtree_ht); vihashlit_ok(hit); hit = vihashlit_next(hit)) {
    vi = (vindex) hit.key;
    assert(vi->n > 0);
    assert(vi->n <= MAXRHS);
    if (vi->e[0] == root)
      continue;
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
  }
}

int 
main(int argc, char **argv)
{
  tree     t, p;
  si_t     si = make_si(100);
  FILE     *fp = stdin;
  vihashl  localtree_ht = make_vihashl(1000);

  si_index root = si_string_index(si, root_label);
  assert(root == 1);

  if (argc != 1) {
    fprintf(stderr, "%s < trees > local-tree-counts\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  /* read the trees and save the local tree counts */

  while ((t = readtree_root(fp, si))) {

/*  si_display(si, stdout);
 *  displaytree(tree, si, 0); 
 *  printf("\n"); 
 */
    p = collapse_identical_unary(t); free_tree(t);
    count_local_trees(p, localtree_ht);
    free_tree(p);
  }

  write_local_trees(localtree_ht, si, root);
  si_free(si);
  free_vihashl(localtree_ht);
  /*  assert(mmm_bytes_allocated == 0); */
  exit(0);
}
