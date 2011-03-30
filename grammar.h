/* grammar.h
 */

#ifndef GRAMMAR_H
#define GRAMMAR_H

#include "local-trees.h"
#include "hash.h"
#include "hash-string.h"

typedef struct brule {
  si_index	parent, left, right;
  FLOAT 	prob;
} *brule;

typedef struct brules {
  brule		*e;
  size_t        nsize, n;
} brules;

typedef struct urule {
  si_index	parent, child;
  FLOAT 	prob;
} *urule;

typedef struct urules {
  urule		*e;
  size_t        nsize, n;
} urules;

HASH_HEADER(sihashurs, si_index, urules)
HASH_HEADER(sihashbrs, si_index, brules)

/* HASH_HEADER_ADD(sihashst, si_index, size_t) */
HASH_HEADER_ADD(sihashf, si_index, FLOAT)

typedef struct grammar {
  sihashurs	urs;
  sihashbrs	brs;
  si_index      root_label;
} grammar;

si_index read_cat(FILE *fp, si_t si);
grammar read_grammar(FILE *fp, si_t si);
void write_grammar(FILE *fp, grammar g, si_t si);
void free_grammar(grammar g);

#endif

