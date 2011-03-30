/* hash.c
 *
 * Hash tables
 *
 * make_hash: returns a new hash table
 * hash_ref: returns the object associated with a key
 * hash_set: inserts an object into a hash table
 * modified version of hash tables in "The C Programming Language", p 144
 */
 
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "mmm.h"
#include "hash.h"

/*
#define MALLOC		malloc
#define CALLOC	 	calloc
#define REALLOC 	realloc
#define FREE		free
*/

/* Preferred hash sizes - primes not close to a power of 2
 * size_t preferred_sizes[] = {7 43 277 1663 10007 60077 360497 2247673 13486051};
 */

#define preferred_size(size)		\
	(size < 21) ? 7 :		\
	(size < 129) ? 43 :		\
	(size < 831) ? 277 :		\
	(size < 4989) ? 1663 :		\
	(size < 30021) ? 10007 :	\
	(size < 180231) ? 60077 :	\
	(size < 1081491) ? 360497 :	\
	(size < 6743019) ? 2247673 :    \
	13486051

HASH make_ ## HASH(size_t initial_size)
{
  HASH ht;
  ht = (HASH) MALLOC(sizeof(struct HASH ## _table));
  ht->tablesize = preferred_size(initial_size);
  ht->size = 0; 
  ht->table = (HASH ## _cell_ptr *)
    CALLOC(ht->tablesize, (size_t) sizeof(HASH ## _cell_ptr));
  return ht;
}   

/* lookup() returns a pointer to the pointer to the hash cell for this key
 * or, if the key is not present in the table, where it ought to be inserted.
 */
static HASH ## _cell_ptr 
*lookup(const HASH ht, const HASH_KEY key, const size_t hashedkey)
{
  HASH ## _cell_ptr	*p  = ht->table + hashedkey%ht->tablesize;
  
  while ( *p && 
	  ((*p)->hashedkey != hashedkey || HASH_KEYNEQ(key, (*p)->key)) )
    p = &((*p)->next);
  
  return p;
}

static void resize_hash_table(HASH ht)
{
  HASH ## _cell_ptr *oldtable = ht->table;
  size_t oldtablesize = ht->tablesize;
  size_t tablesize = preferred_size(ht->size);
  size_t i;
  HASH ## _cell_ptr	p, nextp;
  
  ht->tablesize = tablesize;
  ht->table = (HASH ## _cell_ptr *) 
    CALLOC(tablesize, (size_t) sizeof(HASH ## _cell_ptr));
  
  for (i=0; i<oldtablesize; i++)
    for (p = oldtable[i]; p; p = nextp) {
      nextp = p->next;
      p->next = ht->table[p->hashedkey%tablesize];
      ht->table[p->hashedkey%tablesize] = p;
    }
  
  FREE(oldtable);
}

HASH_VALUE *HASH ## _ref(HASH ht, HASH_KEY key)
{
  HASH ## _cell_ptr *p = lookup(ht, key, KEY_HASH(key));
  return (*p) ? (*p)->data : NULL;
}

HASH_VALUE *HASH ## _set(HASH ht, HASH_KEY key, HASH_VALUE value)
{
  void *olddata = NULL_VALUE;
  size_t hashedkey = KEY_HASH(key);
  HASH ## _cell_ptr *p = lookup(ht, key, hashedkey);

  if (!*p) {
    if (ht->size++ >= 3*ht->tablesize) {
      resize_hash_table(ht);
      p = lookup(ht, key, hashedkey);
      assert(!*p);
    }
    *p = MALLOC(sizeof(struct HASH ## _cell));
    (*p)->next = NULL;
    (*p)->hashedkey = hashedkey;
    (*p)->key = KEY_COPY(key);
  }
  else 
    olddata = (*p)->data;
  
  (*p)->data = data;
  return olddata;
}

HASH_VALUE HASH ## _inc(HASH ht, HASH_KEY key, HASH_VALUE inc)
{
  size_t hashedkey = KEY_HASH(key);
  HASH ## _cell_ptr *p = lookup(ht, key, hashedkey);
  
  if (!*p) {
    if (ht->size++ >= 3*ht->tablesize) {
      resize_hash_table(ht);
      p = lookup(ht, key, hashedkey);
      assert(!*p);
    }
    *p = MALLOC(sizeof(struct HASH ## _cell));
    (*p)->next = NULL_VALUE;
    (*p)->hashedkey = hashedkey;
    (*p)->key = KEY_COPY(key);
    (*p)->data = NULL_VALUE;
  }

  (*p)->data += inc;
  return (*p)->data;
}

HASH_VALUE *HASH ## _delete(HASH ht, HASH_KEY key)
{
  HASH_VALUE olddata = NULL_VALUE;
  HASH ## _cell_ptr *p = lookup(ht, key, KEY_HASH(key));

  if (*p) {
    olddata = (*p)->data;
    *p = (*p)->next;
    KEY_FREE((*p)->key);
    FREE(*p);
    
    if (--ht->size < ht->tablesize/5)
      resize_hash_table(ht);
  }
  return olddata;
}

void free_ ## HASH(HASH ht)
{
  HASH ## _cell_ptr p, q;
  size_t  i;
  
  for (i = 0; i < ht->tablesize; i++)
    for (p = ht->table[i]; p; p = q) {
      q = p->next;
      KEY_FREE(p->key);
      #ifdef VALUE_FREE
        VALUE_FREE(p->data);
      #endif
      FREE(p);
    }
  FREE(ht->table);
  FREE(ht);
}

/***************************************************************************
 *                                                                         *
 *                           hash iteration                                *
 *                                                                         *
 ***************************************************************************/


HASH ## it HASH ## it_init(HASH ht)
{
  struct HASH ## it hit = { NULL, 0, ht, NULL, 0 };
  return HASH ## it_next(hit);
}

HASH ## it HASH ## it_next(hashlit hit0)
{
  if (hit0.next) {
    struct HASH  ## it hit = { 
      hit0.next->key, hit0.next->data, hit0.ht, 
      hit0.next->next, hit0.index };
    return hit;
  }
  else {
    size_t i = hit0.index;
    size_t tablesize = hit0.ht->tablesize;
    hashl_cell_ptr *table = hit0.ht->table;
    while (i < tablesize && !table[i])
      i++;
    if (i==tablesize) {
      struct hashlit hit = { 0, 0, 0, 0, 0 };
      return hit;
    }
    else {
      struct hashlit hit = {
	table[i]->key, table[i]->data, hit0.ht, table[i]->next, i+1 };
      return hit;
    }}}
	
int hashlit_ok(hashlit hit)
{
  return hit.ht!=NULL;
}


/***************************************************************************
 *                                                                         *
 *                           string stuff                                  *
 *                                                                         *
 ***************************************************************************/
 
/* strhash maps a character string to a hash index < 2^28
 * This is the fn hashpjw of Aho, Sethi and Ullman, p 436.
 */
 
size_t strhash(const char *s)
{
  const char *p;
  unsigned h = 0, g;
  for (p=s; *p != '\0'; p++) {
    h = (h << 4) + (*p);
    if (g = h&0xf0000000) {
      h = h ^ (g >> 24);
      h = h ^ g;
    }}
  return h;
}

char *mystrsave(const char *s)    /* make a duplicate of s; see p 143 */
{
  char *p;
  
  p = (char *) MALLOC(strlen(s)+1);	/* +1 for '\0' */
  strcpy(p, s);
  return p;
}

/***************************************************************************
 *                                                                         *
 *                      string index stuff                                 *
 *                                                                         *
 ***************************************************************************/

/* make_si:  make an si table */
si_t make_si(const size_t initial_size)
{
  key_hash keyhash = (key_hash) &strhash;
  key_neq  keyneq = (key_neq) &strcmp;
  key_copy keycopy = (key_copy) &mystrsave;
  key_free keyfree = (key_free) &FREE;

  MAKE_HASH_BODY(si_t, si_table, hashi_cell_ptr);
  ht->stringsize = 3*ht->tablesize;
  ht->strings = (char **) CALLOC(sizeof(char *), ht->stringsize);
  return ht;
}

/* si_string_index: returns the number associated with string str. */
si_index si_string_index(si_t si, char *string)
{
  size_t hashedstring = (*si->keyhash)(string);
  hashi_cell_ptr *p = (hashi_cell_ptr *) lookup((hash) si, string, hashedstring);
  
  if (!*p) {
    if (si->size >= si->stringsize) {
      resize_hash_table((hash) si);
      p = (hashi_cell_ptr *) lookup((hash) si, string, hashedstring);
      assert(!*p);
      si->stringsize = 3*si->tablesize;
      si->strings = (char **) REALLOC(si->strings, si->stringsize * sizeof(char *));
      assert(si->strings);
    }

    if (si->size > SI_INDEX_MAX) {
      fprintf(stderr, "si_string_index in hash.c; Too many indices, overflows si_index\n");
      exit(2);
    }

    *p = MALLOC(sizeof(struct hashi_cell));
    (*p)->next = NULL;
    (*p)->hashedkey = hashedstring;
    (*p)->key = (*(si->keycopy))(string);
    (*p)->data = si->size;
    si->strings[si->size++] = (*p)->key;
  }
  return((*p)->data);
}

char *si_index_string(si_t si, si_index index) 
{
  assert(index < si->size);
  return si->strings[index];
}

void si_free(si_t si)
{
  size_t i;
  hashi_cell_ptr p, q;
  
  for (i = 0; i < si->tablesize; i++)
    for ( p = si->table[i]; p; p = q) {
      q = p->next;
      (*si->keyfree)(p->key);
      FREE(p);
    }
  FREE(si->table);
  FREE(si->strings);
  FREE(si);
}

void si_display(si_t si, FILE *fp)
{
  size_t i;

  for (i=0; i<si->size; i++) 
    fprintf(fp, "%ld: %s\n", (long) i, si_index_string(si, i));
}
