#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "cbt.h"

#define NDEBUG
#include <assert.h>

struct cbt_node_s {
  short crit;
  struct cbt_node_s *left, *right;
};
typedef struct cbt_node_s cbt_node_t[1];
typedef struct cbt_node_s *cbt_node_ptr;

struct cbt_leaf_s {
  short crit;
  void *data;
  char *key;
  struct cbt_leaf_s *prev, *next;
};
typedef struct cbt_leaf_s cbt_leaf_t[1];
typedef struct cbt_leaf_s *cbt_leaf_ptr;

struct cbt_s {
  int count;
  cbt_node_ptr root;
  struct cbt_leaf_s *first, *last;
  void *(*dup)(cbt_t, const void *);
  int (*getlen)(cbt_t, const void *);
  int (*cmp)(cbt_t, const void *, const void *);
  int (*getcrit)(cbt_t, const void *, const void *);
  int len;
};

enum { EXT = -1 };

void cbt_node_free(cbt_node_ptr t) {
  if (!t) return;
  if (EXT == t->crit) {
    free(((cbt_leaf_ptr) t)->key);
  } else {
    cbt_node_free(t->left), cbt_node_free(t->right);
  }
  free(t);
}

static void cbt_init(cbt_t cbt) {
  cbt->count = 0;
  cbt->root = 0;
  cbt->first = cbt->last = 0;
}

static int getcrit(cbt_t unused, const void *key0, const void *key1) {
  const char *c0 = key0, *c1 = key1;
  int bit;

  while(*c0 == *c1) {
    if (!*c0) return 0;
    c0++, c1++;
  }

  char c = *c0 ^ *c1;
  for (bit = 7; !(c >> bit); bit--);
  // Subtract bit from 7 because we number them the other way.
  // Add 1 because we want to use the sign as an extra bit of information.
  // We'll subtract 1 from it later.
  int crit = ((c0 - (const char *) key0) << 3) + 7 - bit + 1;
  if ((*c0 >> bit) & 1) return crit;
  return -crit;
}

static int cmp(cbt_t unused, const void *key0, const void *key1) {
  return strcmp(key0, key1);
}

static int getlen(cbt_t unused, const void *key) {
  // The terminating NUL counts as part of the key, though when in doubt we
  // take the left branch so it works without the "+ 1".
  return strlen(key) + 1;
}

static void *dup(cbt_t unused, const void *key) { return strdup(key); }

cbt_t cbt_new(void) {
  cbt_t res = malloc(sizeof(*res));
  cbt_init(res);
  res->len = 0;
  res->cmp = cmp;
  res->dup = dup;
  res->getlen = getlen;
  res->getcrit = getcrit;
  return res;
}

static int getcrit_u(cbt_t cbt, const void *key0, const void *key1) {
  const char *cp0 = key0, *limit = key0 + cbt->len, *cp1 = key1;

  for(;;) {
    if (cp0 == limit) return 0;
    if (*cp0 != *cp1) break;
    cp0++, cp1++;
  }

  int bit;
  char c = *cp0 ^ *cp1;
  for (bit = 7; !(c >> bit); bit--);
  // Subtract bit from 7 because we number them the other way.
  // Add 1 because we want to use the sign as an extra bit of information.
  // We'll subtract 1 from it later.
  int crit = ((cp0 - (const char *) key0) << 3) + 7 - bit + 1;
  if ((*cp0 >> bit) & 1) return crit;
  return -crit;
}

static int cmp_u(cbt_t cbt, const void *key0, const void *key1) {
  return memcmp(key0, key1, cbt->len);
}

static void *dup_u(cbt_t cbt, const void *key) {
  void *res = malloc(cbt->len);
  memcpy(res, key, cbt->len);
  return res;
}

static int getlen_u(cbt_t cbt, const void *unused) { return cbt->len; }

cbt_t cbt_new_u(int len) {
  cbt_t res = malloc(sizeof(*res));
  cbt_init(res);
  res->len = len;
  res->cmp = cmp_u;
  res->getlen = getlen_u;
  res->dup = dup_u;
  res->getcrit = getcrit_u;
  return res;
}

static int getlen_enc(cbt_t unused, const void *key) {
  const uint8_t *u = (const uint8_t *) key;
  return *u + (u[1] << 8);
}

static int cmp_enc(cbt_t cbt, const void *key0, const void *key1) {
  int len = getlen_enc(0, key0);
  return getlen_enc(0, key1) != len ? 1 : memcmp(key0, key1, len + 2);
}

static void *dup_enc(cbt_t cbt, const void *key) {
  int len = getlen_enc(0, key) + 2;
  void *res = malloc(len);
  memcpy(res, key, len);
  return res;
}

static int getcrit_enc(cbt_t cbt, const void *key0, const void *key1) {
  int n = getlen_enc(0, key0), n1 = getlen_enc(0, key1);
  if (n > n1) n = n1;
  const char *cp0 = key0, *limit = key0 + n + 2, *cp1 = key1;

  for(;;) {
    if (cp0 == limit) return 0;
    if (*cp0 != *cp1) break;
    cp0++, cp1++;
  }

  int bit;
  char c = *cp0 ^ *cp1;
  for (bit = 7; !(c >> bit); bit--);
  // Subtract bit from 7 because we number them the other way.
  // Add 1 because we want to use the sign as an extra bit of information.
  // We'll subtract 1 from it later.
  int crit = ((cp0 - (const char *) key0) << 3) + 7 - bit + 1;
  return (*cp0 >> bit) & 1 ? crit : -crit;
}

cbt_t cbt_new_enc() {
  cbt_t res = malloc(sizeof(*res));
  cbt_init(res);
  res->len = 0;
  res->cmp = cmp_enc;
  res->getlen = getlen_enc;
  res->dup = dup_enc;
  res->getcrit = getcrit_enc;
  return res;
}

static void cbt_clear(cbt_t cbt) { cbt_node_free(cbt->root); }

void cbt_delete(cbt_t cbt) {
  cbt_clear(cbt);
  free(cbt);
}

int cbt_size(cbt_t cbt) { return cbt->count; }
cbt_it cbt_first(cbt_t cbt) { return cbt->first; }
cbt_it cbt_last(cbt_t cbt) { return cbt->last; }
cbt_it cbt_next(cbt_it it) { return it->next; }
void cbt_put(cbt_it it, void *data) { it->data = data; }
void *cbt_get(cbt_it it) { return it->data; }
char *cbt_key(cbt_it it) { return it->key; }

static int testbit(const void *key, int bit) {
  // The most significant bit is 0, and the least 7.
  return (1 << (7 - (bit & 7))) & ((const char *) key)[bit >> 3];
}

cbt_it cbt_at(cbt_t cbt, const void *key) {
  if (!cbt->root) return 0;
  int len = (cbt->getlen(cbt, key) << 3) - 1;
  cbt_node_ptr p = cbt->root;
  for (;;) {
    if (EXT == p->crit) break;
    if (len < p->crit) {
      do p = p->left; while (EXT != p->crit);
      break;
    }
    p = testbit(key, p->crit) ? p->right : p->left;
  }
  if (!cbt->cmp(cbt, ((cbt_leaf_ptr) p)->key, key)) return (cbt_leaf_ptr) p;
  return 0;
}

int cbt_has(cbt_t cbt, const void *key) { return cbt_at(cbt, key) != 0; }

void *cbt_get_at(cbt_t cbt, const void *key) {
  cbt_leaf_ptr p = cbt_at(cbt, key);
  if (!p) return 0;
  return p->data;
}

int cbt_insert_with(cbt_it *it, cbt_t cbt, void *(*fn)(void *), const void *key) {
  if (!cbt->root) {
    cbt_leaf_ptr leaf = malloc(sizeof(cbt_leaf_t));
    leaf->crit = EXT, leaf->data = fn(0), leaf->key = cbt->dup(cbt, key);
    cbt->root = (cbt_node_ptr) leaf;
    cbt->first = cbt->last = leaf;
    leaf->next = leaf->prev = 0;
    cbt->count++;
    return *it = leaf, 1;
  }

  cbt_node_ptr t = cbt->root;
  int keylen = (cbt->getlen(cbt, key) << 3) - 1;

  while (EXT != t->crit) {
    // If the key is shorter than the remaining keys on this subtree, we can
    // compare it against any of them (and are guaranteed the new node must be
    // inserted above this node). We simply let it follow the rightmost path.
    t = keylen < t->crit || testbit(key, t->crit) ? t->right : t->left;
  }

  cbt_leaf_ptr leaf = (cbt_leaf_ptr) t;
  int res = cbt->getcrit(cbt, key, leaf->key);
  if (!res) {
    leaf->data = fn(leaf->data);
    return *it = leaf, 0;
  }

  cbt->count++;
  cbt_leaf_ptr pleaf = malloc(sizeof(cbt_leaf_t));
  cbt_node_ptr pnode = malloc(sizeof(cbt_node_t));
  pleaf->crit = EXT, pleaf->data = fn(0), pleaf->key = cbt->dup(cbt, key);
  pnode->crit = abs(res) - 1;

  cbt_node_ptr t0 = 0, t1 = cbt->root;
  while(EXT != t1->crit && pnode->crit > t1->crit) {
    t0 = t1, t1 = testbit(key, t1->crit) ? t1->right : t1->left;
  }

  if (res > 0) {
    // Key is bigger, therefore it goes on the right.
    pnode->left = t1;
    pnode->right = (cbt_node_ptr) pleaf;
    // The rightmost child of the left subtree must be the predecessor.
    for (t = pnode->left; t->crit != EXT; t = t->right);
    cbt_leaf_ptr leaf = (cbt_leaf_ptr) t;
    pleaf->next = leaf->next;
    pleaf->prev = leaf;
    if (leaf->next) leaf->next->prev = pleaf;
    else cbt->last = pleaf;
    leaf->next = pleaf;
  } else {
    // Key is smaller, therefore it goes on the left.
    pnode->left = (cbt_node_ptr) pleaf;
    pnode->right = t1;
    // The leftmost child of the right subtree must be the successor.
    for (t = pnode->right; t->crit != EXT; t = t->left);
    cbt_leaf_ptr leaf = (cbt_leaf_ptr) t;
    pleaf->prev = leaf->prev;
    pleaf->next = leaf;
    if (leaf->prev) leaf->prev->next = pleaf;
    else cbt->first = pleaf;
    leaf->prev = pleaf;
  }

  if (!t0) {
    cbt->root = pnode;
  } else if (t0->left == t1) {
    t0->left = pnode;
  } else {
    t0->right = pnode;
  }
  return *it = pleaf, 1;
}

cbt_it cbt_put_with(cbt_t cbt, void *(*fn)(void *), const void *key) {
  cbt_it it;
  cbt_insert_with(&it, cbt, fn, key);
  return it;
}

cbt_it cbt_put_at(cbt_t cbt, void *data, const void *key) {
  void *returndata(void *p) { return data; }
  return cbt_put_with(cbt, returndata, key);
}

void *cbt_remove(cbt_t cbt, const void *key) {
  assert(cbt->root);
  assert(cbt_has(cbt, key));
  cbt_node_ptr t0 = 0, t00 = 0, t = cbt->root;
  while (EXT != t->crit) {
    assert((cbt->getlen(cbt, key) << 3) - 1 >= t->crit);
    t00 = t0, t0 = t, t = testbit(key, t->crit) ? t->right : t->left;
  }
  cbt->count--;
  cbt_leaf_ptr p = (cbt_leaf_ptr) t;
  if (!t0) {
    cbt->root = 0;
  } else {
    cbt_node_ptr sibling = t0->left == t ? t0->right : t0->left;
    if (!t00) {  // One-level down: reassign root.
      cbt->root = sibling;
    } else {  // Reassign grandparent.
      if (t00->left == t0) {
        t00->left = sibling;
      } else {
        t00->right = sibling;
      }
    }
    free(t0);
  }
  if (p->next) p->next->prev = p->prev;
  else cbt->last = p->prev;
  if (p->prev) p->prev->next = p->next;
  else cbt->first = p->next;
  free(p->key);
  void *data = p->data;
  free(p);
  return data;
}

static void clear_recurse(cbt_node_ptr t, void (*fn)(void *, const void *)) {
  if (EXT == t->crit) {
    cbt_leaf_ptr p = (cbt_leaf_ptr) t;
    if (fn) fn(p->data, p->key);
    free(p->key);
    free(p);
    return;
  }
  clear_recurse(t->left, fn);
  clear_recurse(t->right, fn);
  free(t);
}

void cbt_remove_all_with(cbt_t cbt, void (*fn)(void *data, const void *key)) {
  if (cbt->root) {
    clear_recurse(cbt->root, fn);
    cbt->root = 0;
    cbt->count = 0;
    cbt->first = cbt->last = 0;
  }
}

void cbt_remove_all(cbt_t cbt) {
  if (cbt->root) cbt_remove_all_with(cbt, 0);
}

void cbt_forall(cbt_t cbt, void (*fn)(cbt_it)) {
  cbt_leaf_ptr p;
  for (p = cbt->first; p; p = p->next) fn(p);
}

void cbt_forall_at(cbt_t cbt, void (*fn)(void *data, const void *key)) {
  cbt_leaf_ptr p;
  for (p = cbt->first; p; p = p->next) fn(p->data, p->key);
}

size_t cbt_overhead(cbt_t cbt) {
  size_t n = sizeof(struct cbt_s);
  if (!cbt->root) return n;
  void add(cbt_node_ptr p) {
    if (p->crit == EXT) {
      n += sizeof(struct cbt_leaf_s);
    } else {
      n += sizeof(struct cbt_node_s);
      add(p->left);
      add(p->right);
    }
  }
  add(cbt->root);
  return n;
}
