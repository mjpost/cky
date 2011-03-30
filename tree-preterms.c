/* count-local-trees.c
 *
 * Mark Johnson, 21st May 1997
 *
 * Reads the f2-21.txt treebank format trees from stdin, and counts
 * the local trees.
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

void write_preterms(const tree t, si_t si, char* prefix)
{
  tree child;

  assert(t);

  if (t->subtrees) 
    for (child = t->subtrees; child; child=child->sibling) {
      write_preterms(child, si, prefix);
      prefix = " ";
    }
  else 
    printf("%s%s", prefix, si_index_string(si, t->label));
}

int 
main(int argc, char **argv)
{
  tree     t;
  si_t     si = make_si(100);
  FILE     *fp = stdin;

  if (argc != 1) {
    fprintf(stderr, "%s < trees > preterms\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  /* read the trees and save the local tree counts */

  while ((t = readtree_root(fp, si))) {
    write_preterms(t, si, "");
    printf("\n");
    free_tree(t);
  }

  si_free(si);
  /*  assert(mmm_bytes_allocated == 0); */
  exit(0);
}
