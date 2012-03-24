/* ncky.c
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
#include "blockalloc.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#define RAND_SEED	time(0)

#define CHART_SIZE(n)			(n)*((n)+1)/2
#define CHART_ENTRY(chart, i, j)	chart->cell[(j)*((j)-1)/2+(i)]

#define MALLOC_CHART(n)			blockalloc_malloc(n)
#define FREE_CHART			blockalloc_free_all()

typedef struct chart_cell {
  struct bintree tree;
  FLOAT		 prob;
  unsigned int   nalt;
  int		 rightpos;
  struct chart_cell *next;
} *chart_cell;


chart_cell
make_chart_cell(si_index label, bintree left, bintree right,
		FLOAT prob, int rightpos, chart_cell next)
{
  chart_cell c = MALLOC_CHART(sizeof(struct chart_cell));
  c->tree.label = label;
  c->tree.left = left;
  c->tree.right = right;
  c->prob = prob;
  c->nalt = 1;
  c->rightpos = rightpos;
  c->next = next;
  return c;
}

/* chart_cell_free() frees the memory associated with this chart cell.
 * A chart cell has a tree associated with it, but since every tree
 * node is associated with exactly one chart cell, only free the
 * top-most node of each tree.
 */

/* HASH_HEADER(sihashcc, si_index, chart_cell) */
/* HASH_CODE(sihashcc, si_index, chart_cell, IDENTITY, NEQ, IDENTITY, NO_OP, 
	  NULL, FREE) */
	
#include "ncky-helper.c"

typedef struct chart {
  sihashcc *cell;
  sihashcc *vertex;
} *chart;


chart
chart_make(size_t n)
{
  size_t  i, left, right, nn = CHART_SIZE(n);
  chart   c = MALLOC(sizeof(struct chart));
  
  c->vertex = MALLOC((n+1)*sizeof(sihashcc));

  for (i = 0; i <= n; i++)
    c->vertex[i] = make_sihashcc(CHART_CELLS);

  c->cell = MALLOC(nn*sizeof(sihashcc));
  
  for (left = 0; left < n; left++)
    for (right = left+1; right <= n; right++) 
      CHART_ENTRY(c, left, right) = 
	make_sihashcc(left+1 == right ? NLABELS : CHART_CELLS);

  return c;
}


void
chart_free(chart c, size_t n)
{
  size_t i;

  /* vertex cell values will be freed below */
  for (i=0; i<=n; i++) {
    FREE(c->vertex[i]->table);
    FREE(c->vertex[i]);
  }

  for (i = 0; i < CHART_SIZE(n); i++) {
    assert(c->cell[i]);
    free_sihashcc(c->cell[i]);
  }
  
  FREE(c->vertex);
  FREE(c->cell);
  FREE(c);
  FREE_CHART;
}

size_t chart_nedges(chart c, size_t n)
{
  size_t i, nedges = 0;
  
  for (i = 0; i < CHART_SIZE(n); i++)
    nedges += c->cell[i]->size;

  return nedges;
}

/*
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
*/
  
static chart_cell
add_edge(sihashcc chart_entry, si_index label, bintree left, bintree right,
	 FLOAT prob, int right_pos, sihashcc left_vertex)
{
  chart_cell *cp = sihashcc_valuep(chart_entry, label);
  chart_cell cc = *cp;

  if (cc == NULL) {                   /* construct a new chart entry */
    chart_cell *vertex_ptr = sihashcc_valuep(left_vertex, label);
    *cp = make_chart_cell(label, left, right, prob, right_pos, *vertex_ptr);
    *vertex_ptr = *cp;
    return *cp;
  }

  /* we're dealing with an old chart entry */

  /* assert(cc->tree.label==label); */

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
follow_unary(chart_cell child_cell, sihashcc chart_entry, grammar g, 
	     int right_pos, sihashcc left_vertex)   
{
  int	 i;
  urules urs = sihashurs_ref(g.urs, child_cell->tree.label);

  for (i=0; i<urs.n; i++) {
    chart_cell parent_cell = add_edge(chart_entry, urs.e[i]->parent, 
				      &child_cell->tree, NULL,
				      child_cell->prob * urs.e[i]->prob,
				      right_pos, left_vertex);

    if (parent_cell)
      follow_unary(parent_cell, chart_entry, g, right_pos, left_vertex);
  }}


static void
apply_unary(sihashcc chart_entry, grammar g, int right_pos, 
	    sihashcc left_vertex)
{
  sihashursit	ursit;
  size_t	i;

  for (ursit=sihashursit_init(g.urs); sihashursit_ok(ursit); 
       ursit = sihashursit_next(ursit)) {
    /* look up the rule's child category */
    chart_cell c = sihashcc_ref(chart_entry, ursit.key);	
    
    if (c)			/* such categories exist in this cell */
      for (i=0; i<ursit.value.n; i++) {
	chart_cell cc = add_edge(chart_entry, ursit.value.e[i]->parent, 
				 &c->tree, NULL, 
				 c->prob * ursit.value.e[i]->prob,
				 right_pos, left_vertex);
	if (cc)
	  follow_unary(cc, chart_entry, g, right_pos, left_vertex);
      }}}


static void
apply_binary(sihashcc left_entry, int left, int mid, chart c, grammar g)
{
  sihashbrsit	brsit;
  size_t	i;

  for (brsit=sihashbrsit_init(g.brs); sihashbrsit_ok(brsit); 
       brsit = sihashbrsit_next(brsit)) {
    /* look up the rule's left category */
    chart_cell cl = sihashcc_ref(left_entry, brsit.key);
    if (cl)	/* such categories exist in this cell */
      for (i=0; i<brsit.value.n; i++) {
	chart_cell cr;
	for (cr = sihashcc_ref(c->vertex[mid], brsit.value.e[i]->right);
	     cr; cr = cr->next) 
	  add_edge(CHART_ENTRY(c,left,cr->rightpos), brsit.value.e[i]->parent,
		   &cl->tree, &cr->tree,
		   cl->prob * cr->prob *  brsit.value.e[i]->prob,
		   cr->rightpos, c->vertex[left]);
      }}}


chart
cky(struct vindex terms, grammar g, si_t si)
{
  int left, mid;
  chart c;

  c = chart_make(terms.n);
  
  /* insert lexical items */

  for (left = 0; left < (int) terms.n; left++) {
    si_index	label = terms.e[left];
    chart_cell  cell = add_edge(CHART_ENTRY(c, left, left+1), label,
				NULL, NULL, 1.0, left+1, c->vertex[left]);
    assert(cell);  /* check that cell was actually added */
  }

  /* actually do syntactic rules! */

  for (left = (int) terms.n-1; left >= 0; left--) {
    for (mid = left+1; mid < (int) terms.n; mid++) {
      sihashcc chart_entry = CHART_ENTRY(c, left, mid);
      /* unary close cell spanning from left to mid */
      apply_unary(chart_entry, g, mid, c->vertex[left]);
      /* now apply binary rules */
      apply_binary(chart_entry, left, mid, c, g);
    }
    /* apply unary rules to chart cells spanning from left to end of sentence
     * there's no need to apply binary rules to these
     */
    apply_unary(CHART_ENTRY(c, left, terms.n), g, 
		(int) terms.n, c->vertex[left]);
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
  FILE		*parsefp = stdout;     	/* parse trees */
  FILE		*probfp = NULL;	       /* max_neglog_prob */

  chart_cell	root_cell;
  grammar	g;
  chart		c;
  vindex 	terms;
  int		maxsentlen = 0;
  int           sentenceno = 0, parsed_sentences = 0, failed_sentences = 0;
  double	sum_neglog_prob = 0;
  double	sum_nedges = 0;
  size_t	max_nedges = 0;

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
      size_t	nedges;

      if (tracefp) {
	fprintf(tracefp, "\nSentence %d:\n", sentenceno);
	for (i=0; i<terms->n; i++)
	  fprintf(tracefp, " %s", si_index_string(si, terms->e[i]));
	fprintf(tracefp, "\n");
	fflush(tracefp);
      }
     
      c = cky(*terms, g, si);

      /* fetch best root node */

      root_cell = sihashcc_ref(CHART_ENTRY(c, 0, terms->n), g.root_label);
      
      nedges = chart_nedges(c, terms->n);
      sum_nedges += nedges;
      if (max_nedges < nedges)
	max_nedges = nedges;

      if (root_cell) {
	tree parse_tree = bintree_tree(&root_cell->tree, si);
	double prob = (double) root_cell->prob;

	parsed_sentences++;
	assert(prob > 0.0);
	sum_neglog_prob -= log(prob);

	if (probfp)
	  fprintf(probfp, "max_neglog_prob(%d, %g).\n", 
		  sentenceno, -log(prob)); 

	if (tracefp) {
	  fprintf(tracefp, " Prob = %g, nedges = %d\n", prob, nedges);
	  fflush(tracefp);
	}

	if (parsefp) {
	  write_tree(parsefp, parse_tree, si);
	  fprintf(parsefp, "\n");
	  /* write_prolog_tree(parsefp, parse_tree, si); */
	}
	
	free_tree(parse_tree);
      }

      else {
	failed_sentences++;
	if (tracefp)
	  fprintf(tracefp, "Failed to parse, nedges = %d\n", nedges);
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
    fprintf(summaryfp, "Sum -log_2(prob) = %g\n", sum_neglog_prob/log(2.0));
    fprintf(summaryfp, "sum_nedges = %g, average nedges = %g, max_nedges = %d\n",
	    sum_nedges, sum_nedges/(parsed_sentences + failed_sentences), max_nedges);
  }

  /* check that everything has been deallocated */
  /* printf("mmm_blocks_allocated = %ld\n", (long) mmm_blocks_allocated); */
  assert(mmm_blocks_allocated == 0);		
  exit(EXIT_SUCCESS);
}
