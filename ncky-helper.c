/* ncky-helper.c
 *
 * In order to speed up ncky.c I have written a specialized
 * memory allocator which can free all the blocks associated
 * with a chart at once.
 *
 * I have also modified the hash code so that it uses this
 * new allocator
 */

/* here is the memory allocator
 */


#define NUNITS(N)    (1 + (N-1)/sizeof(align))

typedef double align;

/* blocksize is the number of align-sized units in the memory
 * block grabbed from malloc.  This setting will give approximately
 * 4Mb-sized blocks.  I subtract 8 align-sized units from exactly
 * 4Mb so that the block with my and the system's overhead will
 * probably still fit in a real 4Mb-size unit
 */

#define BLOCKSIZE    (1048576-8)

struct blockalloc {
  align           data[BLOCKSIZE];
  size_t          top;
  struct blockalloc *next;
};

static struct blockalloc *current_block;

void
blockalloc_free_all(void)
{
  struct blockalloc *p = current_block;

  while (p) {
    struct blockalloc *q = p->next;
    FREE(p);
    p = q;
  }

  current_block = NULL;
}

void *
blockalloc_malloc(size_t n_char)
{
  struct blockalloc *p = current_block;
  size_t n = NUNITS(n_char);

  assert(n<=BLOCKSIZE);

  if (p == NULL || p->top < n) {
    p = MALLOC(sizeof(struct blockalloc));
    p->next = current_block;
    p->top = BLOCKSIZE;
    current_block = p;
  }

  p->top -= n;
  return (void *) &p->data[p->top];
}

/* now here is the hash code
 */

/* hash-templates.h
 *
 * Hash table code templates.
 *
 * This file defines two macros, which in turn define hash functions
 * of arbitrary objects.
 *
 * HASH_CODE(HASH, HASH_KEY, HASH_VALUE, KEY_HASH, KEY_NEQ, KEY_COPY,  \
 *		  KEY_FREE, NULL_VALUE, VALUE_FREE)
 *   defines a hash package whose function names have the prefix HASH,
 *   and where
 *       HASH_KEY is the type of hash keys
 *       HASH_VALUE is the type of hash values
 *       KEY_HASH is a hash function for the keys
 *       KEY_NEQ returns 0 iff its pair of key arguments are equal
 *       KEY_COPY returns a copy of a key
 *       KEY_FREE frees a key
 *       NULL_VALUE is value returned for an undefined key
 *       VALUE_FREE frees a value
 *
 * HASH_CODE_ADD has the same arguments as HASH_CODE, but defines
 * functions that assume that values can be added in addition to
 * those of HASH_CODE
 *
 * These functions can be `expanded' by macros if desired.
 * IDENTITY and NO_OP are provided as possible expansions.
 */
 
typedef struct sihashcc_cell {	
  si_index	   key;		
  struct sihashcc_cell *next;	
  chart_cell	   value;	
} * sihashcc_cell_ptr;		
				
typedef struct sihashcc_table {	/* hash table of pointers */ 	
  sihashcc_cell_ptr *table;					
  size_t 	    tablesize;	/* size of table */		
  size_t 	    size; 	/* no of elts in hash table */	
} * sihashcc;							
	


sihashcc make_sihashcc(size_t initial_size)	
{	
  sihashcc ht;	
  ht = (sihashcc) MALLOC(sizeof(struct sihashcc_table));	
  ht->tablesize = preferred_size(initial_size);	
  ht->size = 0; 	
  ht->table = (sihashcc_cell_ptr *)	
    CALLOC(ht->tablesize, (size_t) sizeof(sihashcc_cell_ptr));	
  return ht;	
}   	
        
static void sihashcc_resize(sihashcc ht)	
{	
  sihashcc_cell_ptr *oldtable = ht->table;	
  size_t oldtablesize = ht->tablesize;	
  size_t tablesize = preferred_size(ht->size);	
  size_t i;	
  sihashcc_cell_ptr	p, nextp;	
  	
  ht->tablesize = tablesize;	
  ht->table = (sihashcc_cell_ptr *) 	
    CALLOC(tablesize, (size_t) sizeof(sihashcc_cell_ptr));	
  	
  for (i=0; i<oldtablesize; i++)	
    for (p = oldtable[i]; p; p = nextp) {	
      nextp = p->next;	
      p->next = ht->table[p->key%tablesize];	
      ht->table[p->key%tablesize] = p;	
    }	
  	
  FREE(oldtable);	
}	
	
sihashcc_cell_ptr sihashcc_find(sihashcc ht, const si_index key) 	
{ 	
  size_t keymod = key%ht->tablesize;	
  sihashcc_cell_ptr p = ht->table[keymod];	
  	
  while (p && p->key != key)
    p = p->next;	
	
  if (p) 	
    return p;	
  else {	
    if (ht->size++ >= 3*ht->tablesize) {	
      sihashcc_resize(ht);	
      keymod = key%ht->tablesize;	
    }	
    p = MALLOC_CHART(sizeof(struct sihashcc_cell));	
    p->key = key;	
    p->value = NULL;	
    p->next = ht->table[keymod];	
    ht->table[keymod] = p;	
    return p;			
}}	
  	
chart_cell *sihashcc_valuep(sihashcc ht, const si_index key)      	
{	
  return(&(sihashcc_find(ht, key)->value));	
}	
	
chart_cell sihashcc_ref(const sihashcc ht, const si_index key)	
{	
  sihashcc_cell_ptr p = ht->table[key%ht->tablesize];	
	
  while (p && key != p->key)
    p = p->next;	
	
  return p ? p->value : NULL;	
}	
	
chart_cell sihashcc_set(sihashcc ht, si_index key, chart_cell value)	
{	
  sihashcc_cell_ptr p = sihashcc_find(ht, key);	
  chart_cell oldvalue = p->value;		
  p->value = value;				
  return oldvalue;				
}	
		
void free_sihashcc(sihashcc ht)	
{	
  FREE(ht->table);	
  FREE(ht);	
}	
