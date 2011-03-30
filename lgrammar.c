/* lgrammar.c
 */

#include "lgrammar.h"
#include "tree.h"
#include "hash-templates.h"
#include "mmm.h"
#include <stdio.h>
#include <ctype.h>

#define MAX(x,y)	((x) < (y) ? (y) : (x))

/* HASH_CODE_ADD(sihashst, si_index, size_t, IDENTITY, NEQ, IDENTITY, NO_OP, 0, NO_OP) */

HASH_CODE_ADD(sihashf, si_index, FLOAT, IDENTITY, NEQ, IDENTITY, NO_OP, 0, 0.0, NO_OP)

/*
static brule
make_brule(const FLOAT prob, const si_index parent, const si_index left, const si_index right)
{
  brule br = MALLOC(sizeof(struct brule));
  br->prob = prob;
  br->parent = parent;
  br->left = left;
  br->right = right;
  return br;
}
*/

void
brules_free(brules brs)
{
  size_t i;
  for (i=0; i<brs.n; i++)
    FREE(brs.e[i]);
  FREE(brs.e);
}

static brules brules_empty = {NULL, 0, 0};

HASH_CODE(sihashbrs, si_index, brules, IDENTITY, NEQ, IDENTITY, NO_OP, 0, brules_empty, brules_free)

typedef struct {
  si_index parent, left, right;
} brindex;

size_t brindex_hash(brindex bri)
{
  return (2*bri.parent+3*bri.left+5*bri.right);
}

int
brindex_neq(const brindex bri1, const brindex bri2)
{
  return ((bri1.parent != bri2.parent) || (bri1.left != bri2.left) || (bri1.right != bri2.right));
}

HASH_HEADER(brihashbr, brindex, brule)
brindex null_brindex = {0,0,0};
HASH_CODE(brihashbr, brindex, brule, brindex_hash, brindex_neq, IDENTITY, NO_OP, null_brindex, NULL, NO_OP)

static void
add_brule(sihashbrs left_brules_ht, brihashbr brihtbr, 
	  const FLOAT prob, const si_index parent, const si_index left, const si_index right)
{
  brindex bri;
  brule br;
  brules *brsp;

  bri.parent = parent;
  bri.left = left;
  bri.right = right;
  br = brihashbr_ref(brihtbr, bri);

  if (br) {			/* have we seen this rule before? */
    br->prob += prob;		/* yes */
    return;
  }
  
  br = MALLOC(sizeof(struct brule));	/* make new brule */
  br->prob = prob;
  br->parent = parent;
  br->left = left;
  br->right = right;
  
  brsp = sihashbrs_valuep(left_brules_ht, left);
  
  if (brsp->nsize <= brsp->n) {
    brsp->nsize = MAX(2*brsp->nsize, 8);
    brsp->e = REALLOC(brsp->e, brsp->nsize * sizeof(brsp->e[0]));
  }

  assert(brsp->n < brsp->nsize);
  brsp->e[(brsp->n)++] = br;
  brihashbr_set(brihtbr, bri, br);
}


static urule
make_urule(const FLOAT prob, const si_index parent, const si_index child)
{
  urule ur = MALLOC(sizeof(struct urule));
  ur->prob = prob;
  ur->parent = parent;
  ur->child = child;
  return ur;
}

static void
urules_free(urules urs)
{
  size_t i;
  for (i=0; i<urs.n; i++)
    FREE(urs.e[i]);
  FREE(urs.e);
}

static urules urules_empty = {NULL, 0, 0};

HASH_CODE(sihashurs, si_index, urules, IDENTITY, NEQ, IDENTITY, NO_OP, 0, urules_empty, urules_free)

static void
push_urule(sihashurs child_urules_ht, const si_index key, const urule ur)
{
  urules *ursp = sihashurs_valuep(child_urules_ht, key);
  
  if (ursp->nsize <= ursp->n) {
    ursp->nsize = MAX(2*ursp->nsize, 8);
    ursp->e = REALLOC(ursp->e, ursp->nsize * sizeof(ursp->e[0]));
  }

  assert(ursp->n < ursp->nsize);
  ursp->e[(ursp->n)++] = ur;
}

si_index 
read_cat(FILE *fp, si_t si)
{
  char string[MAXLABELLEN];
  int    c;
  size_t i;

  while ((c = fgetc(fp)) && isspace(c) && (c != '\n'))		/* skip spaces */
    ;

  if ((c == '\n') || (c == EOF)) return(0);			/* line ended, return 0 */

  for (i = 0; (c != EOF) && (!isspace(c)) && (i < MAXLABELLEN); c = fgetc(fp)) 
    string[i++] = c;

  ungetc(c, fp);

  if (i >= MAXLABELLEN) {
    string[MAXLABELLEN-1] = '\0';
    fprintf(stderr, "read_cat() in grammar.c: Category label longer than MAXLABELLEN: %s\n", string);
    exit(EXIT_FAILURE);
  }

  string[i] = '\0';
  return(si_string_index(si, string));
}
  
  
grammar
read_grammar(FILE *fp, si_t si) 
{
  sihashbrs left_brules_ht = make_sihashbrs(NLABELS);
  sihashurs child_urules_ht = make_sihashurs(NLABELS);
  sihashurs parent_child_urules_ht = make_sihashurs(5); /* underestimate, to ensure dense packing */
  sihashf  parent_weight_ht = make_sihashf(NLABELS);
  brihashbr brihtbr = make_brihashbr(NLABELS);
  int n;
  double weight;
  urule ur;
  sihashbrsit bhit;
  sihashursit uhit;
  size_t  root_label = 0, lhs, cat, rhs[MAXRHS];

  while ((n = fscanf(fp, " %lg ", &weight)) == 1) {	/* read the rule weight */
    lhs = read_cat(fp, si);
    assert(weight > 0);
    assert(lhs);
    if (!root_label)
      root_label = lhs;

    fscanf(fp, " " REWRITES);				/* read the rewrites symbol */

    for (n=0; n<MAXRHS; n++) {				/* read the rhs, n is length of rhs */
      cat = read_cat(fp, si);
      if (!cat)
	break;
      rhs[n] = cat;
    }

    if (n >= MAXRHS) {
      fprintf(stderr, "read_grammar() in grammar.c: rule rhs too long\n");
      exit(EXIT_FAILURE);
    }

    switch (n) {
    case 0: 
      fprintf(stderr, "read_grammar() in grammar.c: rule with empty rhs\n");
      exit(EXIT_FAILURE);
      break;
    case 1: 
      ur = make_urule(weight, lhs, rhs[0]);
      push_urule(child_urules_ht, ur->child, ur);
      sihashf_inc(parent_weight_ht, ur->parent, weight);
      break;
    case 2:
      add_brule(left_brules_ht, brihtbr, weight, lhs, rhs[0], rhs[1]);
      sihashf_inc(parent_weight_ht, lhs, weight);
      break;
    default: 
      { int start, i, j;
        char bcat[MAXBLABELLEN], *s;
	si_index bparent, left, right;

	right = rhs[n-1];		/* rightmost category */
	for (start=n-2; start>=1; start--) {
	  
	  i = 0;			/* i is index into bcat[] */
	  for (j=start; j<n; j++) {     /* j is index into rhs[] */
	    if (j!=start) {
	      bcat[i++] = BINSEP;
	      assert(i < MAXBLABELLEN);
	    }
	    
	    s = si_index_string(si, rhs[j]);
	    while (*s) {
	      bcat[i++] = *s++;
	      assert(i < MAXBLABELLEN);
	  }}

	  bcat[i] = '\0';
	  bparent = si_string_index(si, bcat);
	  left = rhs[start];
	  add_brule(left_brules_ht, brihtbr, weight, bparent, left, right);
	  sihashf_inc(parent_weight_ht, bparent, weight);
	  right = bparent;
	}
	
	add_brule(left_brules_ht, brihtbr, weight, lhs, rhs[0], right);
	sihashf_inc(parent_weight_ht, lhs, weight);
      }}}
  
  free_brihashbr(brihtbr);	/* free brindex hash table */

  { 
    int i; /* normalize grammar rules */

    for (bhit = sihashbrsit_init(left_brules_ht); sihashbrsit_ok(bhit); bhit = sihashbrsit_next(bhit))
      for (i=0; i<bhit.value.n; i++) 
	bhit.value.e[i]->prob /= sihashf_ref(parent_weight_ht, bhit.value.e[i]->parent);

    for (uhit = sihashursit_init(child_urules_ht); sihashursit_ok(uhit); uhit = sihashursit_next(uhit))
      for (i=0; i<uhit.value.n; i++) 
	uhit.value.e[i]->prob /= sihashf_ref(parent_weight_ht, uhit.value.e[i]->parent);
  }

  {
    int i, j;   /* copy unary rules for binary parents into parent_urs */

    for (bhit = sihashbrsit_init(left_brules_ht); sihashbrsit_ok(bhit); bhit = sihashbrsit_next(bhit))
      for (i=0; i<bhit.value.n; i++) {
	si_index key = bhit.value.e[i]->parent;
	urules urs = sihashurs_ref(child_urules_ht, key);
	
	if (urs.n > 0) {
	  urules *pursp = sihashurs_valuep(parent_child_urules_ht, key);
	
	  if (pursp->nsize == 0) {
	    pursp->n = urs.n;
	    pursp->nsize = urs.nsize;
	    pursp->e = REALLOC(pursp->e, pursp->nsize * sizeof(pursp->e[0]));
	    
	    for (j = 0; j < urs.n; j++) 
	      pursp->e[j] = make_urule(urs.e[j]->prob, urs.e[j]->parent, urs.e[j]->child);
	  }}}}
  
  free_sihashf(parent_weight_ht);
 
  {
    grammar g;
    g.urs = child_urules_ht;
    g.parent_urs = parent_child_urules_ht;
    g.brs = left_brules_ht;
    g.root_label = root_label;
    return g;
  }
}

void 
write_grammar(FILE *fp, grammar g, si_t si) 
{
  sihashbrsit	bhit;
  sihashursit	uhit;
  size_t	i;

  for (bhit=sihashbrsit_init(g.brs); sihashbrsit_ok(bhit); bhit=sihashbrsit_next(bhit)) 
    for (i=0; i<bhit.value.n; i++) 
      fprintf(fp, "%g	%s " REWRITES " %s %s\n", (double) bhit.value.e[i]->prob, 
	      si_index_string(si, bhit.value.e[i]->parent),
	      si_index_string(si, bhit.value.e[i]->left),
	      si_index_string(si, bhit.value.e[i]->right));

  for (uhit=sihashursit_init(g.urs); sihashursit_ok(uhit); uhit=sihashursit_next(uhit)) 
    for (i=0; i<uhit.value.n; i++) 
      fprintf(fp, "%g	%s " REWRITES " %s\n", (double) uhit.value.e[i]->prob, 
	      si_index_string(si, uhit.value.e[i]->parent),
	      si_index_string(si, uhit.value.e[i]->child));
}

void
free_grammar(grammar g)
{
  free_sihashurs(g.urs);
  free_sihashurs(g.parent_urs);
  free_sihashbrs(g.brs);
}

