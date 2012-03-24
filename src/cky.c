/* cky.c
 *
 * Modified Mark Johnson, 1st July 1998 
 *
 * to correctly randomly select possible parses, and to print out
 * max_neglog_prob for each sentence.
 */

#include "local-trees.h"
#include "mmm.h"		/* memory debugger */
#include "hash-string.h" 	/* hash tables and string-index tables */
#include "tree.h"
#include "vindex.h"
#include "ledge.h"
#include "grammar.h"
#include "hash.h"
#include "hash-templates.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#include <sys/time.h>
#include <unistd.h>

#define RAND_SEED	time(0)

#define CHART_SIZE(n)			(n)*((n)+1)/2
#define CHART_ENTRY(chart, i, j)	chart[(j)*((j)-1)/2+(i)]

typedef struct chart_cell {
  struct bintree tree;
  FLOAT		 prob;
  unsigned int   nalt;
} *chart_cell;


chart_cell
make_chart_cell(si_index label, bintree left, bintree right,
		FLOAT prob, unsigned int nalt)
{
  chart_cell c = MALLOC(sizeof(struct chart_cell));
  c->tree.label = label;
  c->tree.left = left;
  c->tree.right = right;
  c->prob = prob;
  c->nalt = nalt;
  return c;
}

/* chart_cell_free() frees the memory associated with this chart cell.
 * A chart cell has a tree associated with it, but since every tree
 * node is associated with exactly one chart cell, only free the
 * top-most node of each tree.
 */

HASH_HEADER(sihashcc, si_index, chart_cell)
HASH_CODE(sihashcc, si_index, chart_cell, IDENTITY, NEQ, IDENTITY, NO_OP, 
	  0, NULL, FREE)

typedef sihashcc *chart;

chart
chart_make(size_t n)
{
  size_t  i, nn = CHART_SIZE(n);
  chart   c = MALLOC(nn*sizeof(sihashcc));
  
  for (i=0; i<nn; i++) 
    c[i] = NULL;	/* chart cell will be constructed in apply_unary() */ 
  
  return c;
}

void
chart_free(chart c, size_t n)
{
  size_t i;

  for (i=0; i<CHART_SIZE(n); i++)
    free_sihashcc(c[i]);

  FREE(c);
}

void
chart_entry_display(FILE *fp, sihashcc chart_entry, si_t si)
{
  sihashccit hit;
  tree t;

  for (hit=sihashccit_init(chart_entry); sihashccit_ok(hit); 
       hit = sihashccit_next(hit)) {
    fprintf(fp, "\n %s: %g \t", si_index_string(si, hit.key),
	    (double) hit.value->prob);
    t = bintree_tree(&hit.value->tree, si);
    write_tree(fp, t, si);
    free_tree(t);
  }
  fprintf(fp, "\n---\n");
}
  
static chart_cell
add_edge(sihashcc chart_entry, si_index label, bintree left, bintree right,
	 FLOAT prob)
{
  chart_cell *cp = sihashcc_valuep(chart_entry, label);
  chart_cell cc = *cp;

  if (cc == NULL) {  /* construct a new chart entry */
    *cp = make_chart_cell(label, left, right, prob, 1);
    return *cp;
  }

  /* we're dealing with an old chart entry */

  assert(cc->tree.label==label);

  if (cc->prob > prob)
    return NULL;      /* current chart cell entry is better than this one */


  if (cc->prob < prob) {  /* new entry is better than old one */
    cc->tree.left = left;
    cc->tree.right = right;
    cc->prob = prob;
    cc->nalt = 1;
    return(cc);
  }

  /* old and new entries have same probability */

  assert(cc->nalt<UINT_MAX);

  if (rand() > RAND_MAX/(++(cc->nalt)))
    return NULL;

  cc->tree.left = left;	/* overwrite tree node */
  cc->tree.right = right;
  return cc;
}

/* follow this unary rule */
static void 
follow_unary(chart_cell child_cell, sihashcc chart_entry, grammar g)   
{
  int	 i;
  urules urs = sihashurs_ref(g.urs, child_cell->tree.label);

  for (i=0; i<urs.n; i++) {
    chart_cell parent_cell;

    if ((parent_cell = add_edge(chart_entry, 
				urs.e[i]->parent, &child_cell->tree, NULL,
				child_cell->prob * urs.e[i]->prob)))
      follow_unary(parent_cell, chart_entry, g);
  }}

static void
apply_unary(sihashcc chart_entry, grammar g)
{
  sihashursit	ursit;
  size_t	i;

  for (ursit=sihashursit_init(g.urs); sihashursit_ok(ursit); 
       ursit = sihashursit_next(ursit)) {
    /* look up the rule's child category */
    chart_cell c = sihashcc_ref(chart_entry, ursit.key);	
    
    if (c)			/* such categories exist in this cell */
      for (i=0; i<ursit.value.n; i++) {
	chart_cell cc;

	if ((cc = add_edge(chart_entry, ursit.value.e[i]->parent, 
			   &c->tree, NULL, 
			   c->prob * ursit.value.e[i]->prob)))
	  follow_unary(cc, chart_entry, g);
      }}}


static void
apply_binary(sihashcc parent_entry, sihashcc left_entry, sihashcc right_entry,
	     grammar g)
{
  sihashbrsit	brsit;
  size_t	i;

  for (brsit=sihashbrsit_init(g.brs); sihashbrsit_ok(brsit); 
       brsit = sihashbrsit_next(brsit)) {
    /* look up the rule's left category */
    chart_cell cl = sihashcc_ref(left_entry, brsit.key);
    if (cl)	/* such categories exist in this cell */
      for (i=0; i<brsit.value.n; i++) {
	chart_cell cr = sihashcc_ref(right_entry, brsit.value.e[i]->right);
	if (cr) 
	  add_edge(parent_entry, brsit.value.e[i]->parent, 
		   &cl->tree, &cr->tree, 
		   cl->prob * cr->prob *  brsit.value.e[i]->prob);
      }}}

chart
cky(struct vindex terms, grammar g, si_t si)
{
  int left, right, mid;
  chart c;

  c = chart_make(terms.n);
  
  /* insert lexical items */

  for (left=0; left< (int) terms.n; left++) {
    si_index	label = terms.e[left];
    sihashcc	chart_entry = make_sihashcc(NLABELS);
    chart_cell  cell = make_chart_cell(label, NULL, NULL, 1.0, 0);

    CHART_ENTRY(c, left, left+1) = chart_entry;
    sihashcc_set(chart_entry, label, cell);

    /* close under unary rules */
    follow_unary(cell, chart_entry, g);      	

    /* fprintf(stderr, "Chart entry %d-%d\n", (int) left, (int) left+1);
       chart_entry_display(stderr, CHART_ENTRY(c,left,left+1), si); */
    
  }

  for (right=2; right<= (int) terms.n; right++)
    for (left=right-2; left>=0; left--) {
      sihashcc chart_entry = make_sihashcc(CHART_CELLS);   
      CHART_ENTRY(c, left, right) = chart_entry;

      for (mid=left+1; mid<right; mid++) 
	apply_binary(chart_entry, 
		     CHART_ENTRY(c,left,mid), CHART_ENTRY(c,mid,right), g);

      apply_unary(chart_entry, g);
      /*
      printf("Chart entry %d-%d\n", (int) left, (int) right);
      chart_entry_display(CHART_ENTRY(c,left,right), si);
      */
    }

  return c;
}

static vindex
read_terms(FILE *fp, si_t si)
{
  size_t i = 0, nsize = 10;
  vindex v = make_vindex(nsize);
  si_index term;

  while ((term = read_cat(fp, si))) {
    if (i >= nsize) {
      nsize *= 2;
      vindex_resize(v, nsize);
    }
    assert(i < nsize);
    vindex_ref(v,i++) = term;
  }
 
  if (i > 0) {
    v->n = i;
    vindex_resize(v, v->n);
    return (v);
  }
  else {
    vindex_free(v);
    return(NULL);
  }
}

int      
main(int argc, char **argv)
{
  si_t          si = make_si(1024);
  FILE          *grammarfp = stdin, *yieldfp;
  FILE		*tracefp = NULL;  	/* trace output */
  FILE		*summaryfp = stderr;	/* end of parse stats output */
  FILE		*parsefp = stdout;      /* parse trees */
  FILE		*probfp = NULL;         /* max_neglog_prob */

  chart_cell	root_cell;
  grammar	g;
  chart		c;
  vindex 	terms;
  int		maxsentlen = 0;
  int           sentenceno = 0, parsed_sentences = 0, failed_sentences = 0;
  double	sum_neglog_prob = 0;

  srand(RAND_SEED);	/* seed random number generator */

  if (argc<2 || argc>4) {
    fprintf(stderr, "%s yieldfile [maxsentlen [grammarfile]]\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  if ((yieldfp = fopen(argv[1], "r")) == NULL) {
    fprintf(stderr, "%s: Couldn't open yieldfile %s\n", argv[0], argv[1]);
    exit(EXIT_FAILURE);
  }

  if (argc >= 3)
    if (!sscanf(argv[2], "%d", &maxsentlen)) {
      fprintf(stderr, "%s: Couldn't parse maxsentlen %s\n", argv[0], argv[2]);
      exit(EXIT_FAILURE);
    }

  if (argc >= 4)
    if ((grammarfp = fopen(argv[3], "r")) == NULL) {
      fprintf(stderr, "%s: Couldn't open grammarfile %s\n", argv[0], argv[3]);
      exit(EXIT_FAILURE);
    }
   
  g = read_grammar(grammarfp, si);
  /* write_grammar(tracefp, g, si); */

  while ((terms = read_terms(yieldfp, si))) {
    sentenceno++;

    /* skip if sentence is too long */
    if (!maxsentlen || (int) terms->n <= maxsentlen) { 
      size_t	i;

      if (tracefp) {
	fprintf(tracefp, "\nSentence %d:\n", sentenceno);
	for (i=0; i<terms->n; i++)
	  fprintf(tracefp, " %s", si_index_string(si, terms->e[i]));
	fprintf(tracefp, "\n");
      }
     
      c = cky(*terms, g, si);

      /* fetch best root node */

      root_cell = sihashcc_ref(CHART_ENTRY(c, 0, terms->n), g.root_label);

      if (root_cell) {
	tree parse_tree = bintree_tree(&root_cell->tree, si);
	double prob = (double) root_cell->prob;

	parsed_sentences++;
	assert(prob > 0.0);
	sum_neglog_prob -= log(prob);

	if (probfp)
	  fprintf(probfp, "max_neglog_prob(%d, %g).\n", 
		  sentenceno, -log(prob)); 

	if (tracefp) 
	  fprintf(tracefp, " Prob = %g\n", prob);

	if (parsefp) {
	  write_tree(parsefp, parse_tree, si);
	  fprintf(parsefp, "\n");
	}

	free_tree(parse_tree);
      }
      else {
	failed_sentences++;
	if (tracefp)
	  fprintf(tracefp, "Failed to parse\n");
	if (parsefp)
	  fprintf(parsefp, "parse_failure.\n");
      }
      
      chart_free(c, terms->n);			/* free the chart */
    }
    else { 					/* sentence too long */
      if (parsefp)
	fprintf(parsefp, "too_long.\n");
    }

    vindex_free(terms);				/*  free the terms */
    assert(trees_allocated == 0);
    assert(bintrees_allocated == 0);
  }
  free_grammar(g);
  si_free(si);

  if (summaryfp) {
    fprintf(summaryfp, "\n%d/%d = %g%% test sentences met the length criteron,"
	    " of which %d/%d = %g%% were parsed\n", 
	    parsed_sentences+failed_sentences, sentenceno,
	    (double) (100.0 * (parsed_sentences+failed_sentences)) / 
	                       sentenceno,
	    parsed_sentences, parsed_sentences+failed_sentences, 
	    (double) (100.0 * parsed_sentences) / 
                              (parsed_sentences + failed_sentences));
    fprintf(summaryfp, "Sum(-log prob) = %g\n", sum_neglog_prob);
  }

  /* check that everything has been deallocated */
  assert(mmm_blocks_allocated == 0);		
  exit(EXIT_SUCCESS);
}
