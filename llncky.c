/* llncky.c
 *
 * Modified Mark Johnson, 1st July 1998 
 *
 * to correctly randomly select possible parses, and to print out
 * max_neglog_prob for each sentence.
 *
 * modified to work with logs of rule probabilities.
 */

#include "local-trees.h"
#include "mmm.h"		/* memory debugger */
#include "hash-string.h" 	/* hash tables and string-index tables */
#include "tree.h"
#include "vindex.h"
#include "ledge.h"
#include "lgrammar.h"
#include "hash.h"
#include "hash-templates.h"
#include "blockalloc.h"

#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
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
  FLOAT		 lprob;
  unsigned int   nalt;
  int		 rightpos;
  struct chart_cell *next;
} *chart_cell;


chart_cell
make_chart_cell(si_index label, bintree left, bintree right,
                FLOAT lprob, int rightpos, chart_cell next)
{
  chart_cell c = MALLOC_CHART(sizeof(struct chart_cell));
  c->tree.label = label;
  c->tree.left = left;
  c->tree.right = right;
  c->lprob = lprob;
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


/*
static void
chart_entry_display(FILE *fp, sihashcc chart_entry, si_t si)
{
  sihashccit hit;
  tree t;

  for (hit=sihashccit_init(chart_entry); sihashccit_ok(hit); 
       hit = sihashccit_next(hit)) {
    fprintf(fp, "\n %s: %g \t", si_index_string(si, hit.key),
            (double) exp(hit.value->lprob));
    t = bintree_tree(&hit.value->tree, si);
    write_tree(fp, t, si);
    free_tree(t);
  }
  fprintf(fp, "\n---\n");
}
*/

int           verbose = 0;
int           edges_proposed = 0;
  
static chart_cell
add_edge(sihashcc chart_entry, si_index label, bintree left, bintree right,
         FLOAT lprob, int right_pos, sihashcc left_vertex)
{
  chart_cell *cp = sihashcc_valuep(chart_entry, label);
  chart_cell cc = *cp;

  edges_proposed++;

  if (cc == NULL) {                   /* construct a new chart entry */
    chart_cell *vertex_ptr = sihashcc_valuep(left_vertex, label);
    *cp = make_chart_cell(label, left, right, lprob, right_pos, *vertex_ptr);
    *vertex_ptr = *cp;
    return *cp;
  }

  /* we're dealing with an old chart entry */

  /* assert(cc->tree.label==label); */

  if (cc->lprob > lprob)
    return NULL;      /* current chart cell entry is better than this one */


  if (cc->lprob < lprob) {  /* new entry is better than old one */
    cc->tree.left = left;
    cc->tree.right = right;
    cc->lprob = lprob;
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

/*
void
dump_chart(int n, chart* c, si_t si)
{

  for (left = (int) terms.n-1; left >= 0; left--) {
    for (mid = left+1; mid < (int) terms.n; mid++) {
      sihashcc chart_entry = CHART_ENTRY(c, left, mid);

  // tree t
  fprintf(fp, "(%s", si_index_string(si, t->label));
}
*/


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
                                      child_cell->lprob + urs.e[i]->prob,
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

  for (ursit=sihashursit_init(g.parent_urs); sihashursit_ok(ursit); 
       ursit = sihashursit_next(ursit)) {
    /* look up the rule's child category */
    chart_cell c = sihashcc_ref(chart_entry, ursit.key);	
    
    if (c)			/* such categories exist in this cell */
      for (i=0; i<ursit.value.n; i++) {
        chart_cell cc = add_edge(chart_entry, ursit.value.e[i]->parent, 
                                 &c->tree, NULL, 
                                 c->lprob + ursit.value.e[i]->prob,
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
                   cl->lprob + cr->lprob +  brsit.value.e[i]->prob,
                   cr->rightpos, c->vertex[left]);
      }}}

int sentenceno = 0;

chart
cky(struct vindex terms, grammar g, si_t si)
{
  int left, mid;
  chart c;

  c = chart_make(terms.n);
  
  /* insert lexical items */

  for (left = 0; left < (int) terms.n; left++) {
    si_index	label = terms.e[left];
    sihashcc    chart_entry = CHART_ENTRY(c, left, left+1);
    sihashcc    left_vertex = c->vertex[left];
    chart_cell  cell = add_edge(chart_entry, label, NULL, NULL, 0.0, 
                                left+1, left_vertex);    
    
    assert(cell);  /* check that cell was actually added */
    follow_unary(cell, chart_entry, g, left+1, left_vertex);
  }

  /* actually do syntactic rules! */

  for (left = (int) terms.n-1; left >= 0; left--) {
    for (mid = left+1; mid < (int) terms.n; mid++) {
      if (verbose)
        printf("SENTNO %d SPAN %d..%d\n", sentenceno, left, mid);

      sihashcc chart_entry = CHART_ENTRY(c, left, mid);
      /* unary close cell spanning from left to mid */
      if (mid - left > 1)
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
    printf("Chart entry %d-%d\n", (int) left, (int) mid);
    chart_entry_display(CHART_ENTRY(c,left,mid), si);
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

  while ((term = read_cat_term(fp, si))) {
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

 void usage() {
  printf("llncky [-m maxsentlen] [-f from] [-t to] [-o outfile] [-l logfile] corpus grammar\n");
  exit(EXIT_FAILURE);
}

int      
main(int argc, char **argv)
{
  si_t          si = make_si(1024);
  FILE          *grammarfp = stdin, *yieldfp = NULL;
  FILE	        *tracefp = NULL;  	/* trace output */
  FILE		*summaryfp = NULL;	/* end of parse stats output */
  FILE		*parsefp = stdout;      /* parse trees */
  FILE		*probfp = NULL;         /* max_neglog_prob */

  chart_cell	root_cell;
  grammar	g;
  chart		c;
  vindex 	terms;
  int		maxsentlen = 0;
  int           parsed_sentences = 0, failed_sentences = 0;
  int           sentfrom = 0, sentto = 0;
  double	sum_neglog_prob = 0;

  srand(RAND_SEED);	/* seed random number generator */

  int arg;
  opterr = 0;

  while ((arg = getopt(argc,argv,"m:l:o:f:t:v")) != -1) { 
    switch (arg) {
    case 'm':
      if (!sscanf(optarg, "%d", &maxsentlen)) {
        fprintf(stderr, "%s: Couldn't parse maxsentlen %s\n", argv[0], optarg);
        exit(EXIT_FAILURE);
      }
      break;
    case 'f':
      if (!sscanf(optarg, "%d", &sentfrom)) {
        fprintf(stderr, "%s: Couldn't parse from %s\n", argv[0], optarg);
        exit(EXIT_FAILURE);
      }
      break;
    case 't':
      if (!sscanf(optarg, "%d", &sentto)) {
        fprintf(stderr, "%s: Couldn't parse from %s\n", argv[0], optarg);
        exit(EXIT_FAILURE);
      }
      break;
    case 'l': // logfile
      if ((tracefp = fopen(optarg, "w")) == NULL) {
        fprintf(stderr, "%s: Couldn't open tracefile %s\n", argv[0], optarg);
        exit(EXIT_FAILURE);
      }
      break;
    case 'o': // output file (instead of stdout)
      if ((parsefp = fopen(optarg, "w")) == NULL) {
        fprintf(stderr, "%s: Couldn't open parsefil %s\n", argv[0], optarg);
        exit(EXIT_FAILURE);
      }
      break;
    case 'v': // verbose output
      verbose = 1;
      break;
    case '?':
      if (optopt == 'c')
        fprintf(stderr, "Option -%c requires an argument.\n", optopt);
      else if (isprint (optopt))
        fprintf(stderr, "Unknown option `-%c'.\n", optopt);
      else 
        fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
      return 1;
    default:
      abort();
    }
  }

  if ( (argc - optind) != 2 )
    usage();

  int from_stdin = 0;
  if (! strcmp(argv[optind],"-")) {
	yieldfp = stdin;
	from_stdin = 1;
	/* if ((yieldfp = fopen(stdin, "r")) == NULL) { */
	/*   fprintf(stderr, "%s: Couldn't read from stdin\n", argv[0]); */
	/*   exit(EXIT_FAILURE); */
	/* } */
  } else {  
	if ((yieldfp = fopen(argv[optind], "r")) == NULL) {
	  fprintf(stderr, "%s: Couldn't open yieldfile %s\n", argv[0], argv[1]);
	  exit(EXIT_FAILURE);
	}
  }  

  if ((grammarfp = fopen(argv[optind+1], "r")) == NULL) {
    fprintf(stderr, "%s: Couldn't open grammarfile %s\n", argv[0], optarg);
    exit(EXIT_FAILURE);
  }

  g = read_grammar(grammarfp, si);
  /* write_grammar(tracefp, g, si); */

  while ((terms = read_terms(yieldfp, si))) {
    sentenceno++;
    edges_proposed = 0;

    if (sentfrom && sentenceno < sentfrom) {
      vindex_free(terms);
      continue;
    }
    if (sentto && sentenceno > sentto) {
      sentenceno--;
      vindex_free(terms);
      break;
    }

    /* skip if sentence is too long */
    if (!maxsentlen || (int) terms->n <= maxsentlen) { 

      /* size_t	i; */
      /* if (tracefp) { */
      /*   fprintf(tracefp, "Sentence %d:", sentenceno); */
      /*   for (i=0; i<terms->n; i++) */
      /*     fprintf(tracefp, " %s", si_index_string(si, terms->e[i])); */
      /*   fprintf(tracefp, "\n"); */
      /* } */

      time_t start_time = time(0);
     
      c = cky(*terms, g, si);

      time_t run_time = time(0) - start_time;

      /* fetch best root node */

      root_cell = sihashcc_ref(CHART_ENTRY(c, 0, terms->n), g.root_label);

      if (root_cell) {
        tree parse_tree = bintree_tree(&root_cell->tree, si);
        double lprob = (double) root_cell->lprob;

        parsed_sentences++;
        assert(lprob < 0.0);
        sum_neglog_prob -= lprob;

        if (probfp)
          fprintf(probfp, "max_neglog_prob(%d, %g).\n", 
                  sentenceno, -lprob); 

        if (parsefp) {

		  fprintf(parsefp,"%f\t", lprob);
		  write_tree(parsefp, parse_tree, si);
		  fprintf(parsefp,"\n");
		  fflush(parsefp);

          /* fprintf(parsefp, "%d ", sentenceno); */
          /* write_tree(parsefp, parse_tree, si); */
          /* fprintf(parsefp, "\n"); */
          /* /\* write_prolog_tree(parsefp, parse_tree, si); *\/ */
          /* fflush(parsefp); */

          if (tracefp) {
            // print time spent parsing
            fprintf(tracefp, "sentence %d: %2lu seconds\n", sentenceno, run_time);

            // print the tree with sentno and log prob
            fprintf(tracefp, "%d %g ", sentenceno, lprob);
            write_tree(tracefp, parse_tree, si);
            fprintf(tracefp, "\n");
        
            // print number of edges proposed
            fprintf(tracefp, "%d: proposed %d edges\n", sentenceno, edges_proposed);
            fflush(tracefp);
          }

        }

        free_tree(parse_tree);
      }		

      else {
        failed_sentences++;

        if (tracefp) {
          // print time spent parsing
          fprintf(tracefp, "sentence %d: %2lu seconds\n", sentenceno, run_time);

          // print the tree with sentno and log prob
          fprintf(tracefp, "%d -inf (TOP)\n", sentenceno);
        
          // print number of edges proposed
          fprintf(tracefp, "%d: proposed %d edges\n", sentenceno, edges_proposed);
        }
        if (parsefp) {
          fprintf(parsefp, "-inf\t(TOP)\n");
		  fflush(parsefp);
		}
      }

      chart_free(c, terms->n);			/* free the chart */
    }
    else { 					/* sentence too long */
      if (parsefp) {
        fprintf(parsefp, "too_long.\n");
		fflush(parsefp);
	  }
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
  /* printf("mmm_blocks_allocated = %ld\n", (long) mmm_blocks_allocated); */
  assert(mmm_blocks_allocated == 0);		
  exit(EXIT_SUCCESS);
}
