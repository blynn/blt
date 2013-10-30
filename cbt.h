// Crit-bit trees and linked list.
// No parent pointers.
//
// Uses pointer casting and different structs instead of unions.
// In a trie, internal nodes never become external nodes, and vice versa.
//
// Removing linked list code and data saves a little.

#define __CBT_H__

struct cbt_s;
typedef struct cbt_s *cbt_t;

struct cbt_leaf_s;
// Iterator.
typedef struct cbt_leaf_s *cbt_it;

// Never mix keys from different types of trees.

// Default: ASCIIZ keys.
cbt_t cbt_new(void);

// "u" mode: all keys are the same length but can contain any data.
// e.g. SHA1 hashes.
cbt_t cbt_new_u(int len);

// "enc" mode: First 2 bytes encode length of remaining data. First byte
// is the least significant.
cbt_t cbt_new_enc();

void cbt_delete(cbt_t cbt);

void *cbt_get_at(cbt_t cbt, const void *key);
cbt_it cbt_put_at(cbt_t cbt, void *data, const void *key);

int cbt_size(cbt_t cbt);

static inline int cbt_is_off(cbt_it it) { return !it; }

cbt_it cbt_first(cbt_t cbt);
cbt_it cbt_last(cbt_t cbt);
cbt_it cbt_next(cbt_it it);
void cbt_put(cbt_it it, void *data);
void *cbt_get(cbt_it it);
char *cbt_key(cbt_it it);

cbt_it cbt_at(cbt_t cbt, const void *key);
int cbt_has(cbt_t cbt, const void *key);

void cbt_forall(cbt_t cbt, void (*fn)(cbt_it));
void cbt_forall_at(cbt_t cbt, void (*fn)(void *data, const void *key));

void *cbt_remove(cbt_t cbt, const void *key);
void cbt_remove_all(cbt_t cbt);
void cbt_remove_all_with(cbt_t cbt, void (*fn)(void *data, const void *key));

static inline void cbt_clear_with(cbt_t cbt,
    void (*fn)(void *data, const void *key)) {
  cbt_remove_all_with(cbt, fn);
}

cbt_it cbt_put_with(cbt_t cbt, void *(*fn)(void *), const void *key);

// Finds or creates an entry with the given key and writes it to *it.
// Returns 1 if a new cbt_it was created.
int cbt_insert(cbt_it *it, cbt_t cbt, const void *key);

size_t cbt_overhead(cbt_t cbt);
