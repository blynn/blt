// Less trivial implementation of crit-bit trees.
// Uses tagged pointers, and bit twiddling tricks.
// See http://www.imperialviolet.org/binary/critbit.pdf
//
// Differences:
//   - We fold strcmp into the crit-bit finder. If there is no crit-bit
//     then we have a match.
//   - During lookup, if the key is shorter than the position of the
//     crit bit in the current node, the path we take is irrelevant.
//     Ideally, we'd take one of the shortest paths to a leaf (the only
//     purpose is to get at a string so we can find the true crit bit),
//     but for simplicity we always follow the left child.
//     Our code skips a tiny bit of computation by assigning
//     direction = 0 rather than c = 0 plus some bit twiddling.
//   - While walking down the tree when inserting a new key (after we've
//     figured out the crit bit), we're guaranteed that the byte number
//     of the current node is less than the key length, so there's no
//     need for special-case code to handle keys shorter than the crit
//     bit.
//   - We combine a couple of comparisons. Instead of byte0 < byte1 and then
//     mask0 < mask1 if they are equal, we simplify to:
//       (byte0 << 8) + mask0 < (byte1 << 8) + mask1
//   - Deletion:

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static inline int has_tag(void *p) { return 1 & (intptr_t)p; }
static inline void *untag(void *p) { return ((char *)p) - 1; }
// Bit twidding: zero all bits except leading bit, then invert.
// Storing the crit bit in this mask form simplifies decide() to 
static inline uint8_t to_mask(uint8_t x) {
  while (x&(x-1)) x &= x-1;
  return 255 - x;
}
static inline int decide(uint8_t c, uint8_t m) { return (1 + (m | c)) >> 8; }

struct blt_leaf_s {
  char *key;
  void *data;
};
typedef struct blt_leaf_s *blt_leaf_ptr;
typedef struct blt_leaf_s BLT_it;

struct blt_node_s {
  void *kid[2];
  unsigned int byte:24;  // Byte # of difference.
  unsigned int mask:8;   // ~mask = the crit bit within the byte.
};
typedef struct blt_node_s *blt_node_ptr;
// The blt_node_ptr pointers are tagged: LSB = 1 means internal node,
// LSB = 0 means external.

struct blt_s {
  int count;
  void *root;
};
typedef struct blt_s BLT;

BLT *blt_new() {
  BLT *blt = malloc(sizeof(*blt));
  blt->count = 0;
  blt->root = 0;
  return blt;
}

BLT_it *blt_firstlast(void *p, int dir) {
  if (!p) return NULL;
  while (has_tag(p)) p = ((blt_node_ptr)untag(p))->kid[dir];
  return p;
}

BLT_it *blt_first(BLT *blt) { return blt_firstlast(blt->root, 0); }
BLT_it *blt_last (BLT *blt) { return blt_firstlast(blt->root, 1); }

// 'it' must be an element of 'blt'.
BLT_it *blt_nextprev(BLT *blt, BLT_it *it, int way) {
  char *p = blt->root;
  void *other = 0;
  while (has_tag(p)) {
    blt_node_ptr q = untag(p);
    int dir = decide(q->mask, it->key[q->byte]);
    if (dir == way) other = q->kid[1 - way];
    p = q->kid[dir];
  }
  assert(!strcmp(((BLT_it *)p)->key, it->key));
  return blt_firstlast(other, way);
}

BLT_it *blt_next(BLT *blt, BLT_it *it) { return blt_nextprev(blt, it, 0); }
BLT_it *blt_prev(BLT *blt, BLT_it *it) { return blt_nextprev(blt, it, 1); }

// Walk down the tree as if the key is there.
static inline blt_leaf_ptr confident_get(BLT *blt, char *key) {
  char *p = blt->root;
  if (!p) return NULL;
  int keylen = strlen(key);
  while (has_tag(p)) {
    blt_node_ptr q = untag(p);
    // When q->byte >= keylen, key is absent, but we must return something.
    // Either kid works; we pick 0 each time.
    p = q->kid[q->byte < keylen ? decide(key[q->byte], q->mask) : 0];
  }
  return (void *)p;
}

BLT_it *blt_ceilfloor(BLT *blt, char *key, int way) {
  blt_leaf_ptr p = confident_get(blt, key);
  if (!p) return 0;
  // Compare keys.
  for(char *c = key, *pc = p->key;; c++, pc++) {
    // XOR the current bytes being compared.
    uint8_t x = *c ^ *pc;
    if (x) {
      int byte = c - key;
      x = to_mask(x);
      int ndir = decide(x, key[byte]);
      void *other = 0;
      // Walk down the tree until we hit an external node or a node
      // whose crit bit is higher.
      void **p0 = &blt->root;
      for (;;) {
        char *p = *p0;
        if (!has_tag(p)) break;
        blt_node_ptr q = untag(p);
        if ((byte << 8) + x < (q->byte << 8) + q->mask) break;
        int dir = decide(q->mask, key[q->byte]);
        if (dir == way) other = q->kid[1 - way];
        p0 = q->kid + dir;
      }
      if (ndir == way) other = *p0;
      return blt_firstlast(other, way);
    }
    if (!*c) return (BLT_it *)p;
  }
}

BLT_it *blt_ceil (BLT *blt, char *key) { return blt_ceilfloor(blt, key, 0); }
BLT_it *blt_floor(BLT *blt, char *key) { return blt_ceilfloor(blt, key, 1); }

void blt_put(BLT *blt, char *key, void *data) {
  blt->count++;
  blt_leaf_ptr p = confident_get(blt, key);
  if (!p) {  // Empty tree case.
    p = malloc(sizeof(*p));
    blt->root = p;
    p->key = strdup(key);
    p->data = data;
    return;
  }
  // Compare keys.
  for(char *c = key, *pc = p->key;; c++, pc++) {
    // XOR the current bytes being compared.
    uint8_t x = *c ^ *pc;
    if (x) {
      // Allocate a new node.
      // Find crit bit using bit twiddling tricks.
      blt_node_ptr n = malloc(sizeof(*n));
      n->byte = c - key;
      n->mask = to_mask(x);
      blt_leaf_ptr leaf = malloc(sizeof(*leaf));
      int ndir = decide(key[n->byte], n->mask);
      n->kid[ndir] = leaf;
      leaf->key = strdup(key);
      leaf->data = data;

      // Insert the new node.
      // Walk down the tree until we hit an external node or a node
      // whose crit bit is higher.
      void **p0 = &blt->root;
      for (;;) {
        char *p = *p0;
        if (!has_tag(p)) break;
        blt_node_ptr q = untag(p);
        if ((n->byte << 8) + n->mask < (q->byte << 8) + q->mask) break;
        p0 = q->kid + decide(key[q->byte], q->mask);
      }
      n->kid[1 - ndir] = *p0;
      *p0 = 1 + (char *)n;
      return;
    }
    if (!*c) {
      ((blt_leaf_ptr) p)->data = data;
      return;
    }
  }
}

int blt_delete(BLT *blt, char *key) {
  char *p = blt->root;
  if (!p) return 0;
  int keylen = strlen(key);
  void **p0 = &blt->root, **q0 = 0;
  blt_node_ptr q;
  int dir;
  while (has_tag(p)) {
    q = untag(p);
    if (q->byte > keylen) return 0;
    dir = decide(q->mask, key[q->byte]);
    q0 = p0;
    p0 = q->kid + dir;
    p = *p0;
  }
  blt_leaf_ptr leaf = (blt_leaf_ptr)p;
  if (strcmp(key, leaf->key)) return 0;
  free(leaf->key);
  free(leaf);
  if (!q0) {
    blt->root = 0;
    return 1;
  }
  *q0 = q->kid[1 - dir];
  free(q);
  return 1;
}

void blt_dump(BLT* blt, void *p) {
  if (!p) return;
  if (has_tag(p)) {
    blt_node_ptr q = untag(p);
    blt_dump(blt, q->kid[0]);
    blt_dump(blt, q->kid[1]);
    return;
  }
  printf("  %s\n", (char *) ((blt_leaf_ptr) p)->key);
}

static void split(char *s, void (*fun)(char *)) {
  for (char *p = s, *q = p;; q++) if (*q == ' ' || *q == '\0') {
    char *tmp = strndup(p, q - p);
    fun(tmp);
    free(tmp);
    if (*q == '\0') break;
    p = q + 1;
  }
}

int blt_allprefixed(BLT *blt, char *key, int (*fun)(BLT_it *)) {
  char *p = blt->root;
  if (!p) return 1;
  char *top = p;
  int keylen = strlen(key);
  while (has_tag(p)) {
    blt_node_ptr q = untag(p);
    if (q->byte >= keylen) {
      p = q->kid[0];
    } else {
      p = q->kid[decide(q->mask, key[q->byte])];
      top = p;
    }
  }
  if (strncmp(key, ((blt_leaf_ptr)p)->key, keylen)) return 1;
  int traverse(char *p) {
    if (has_tag(p)) {
      blt_node_ptr q = untag(p);
      for (int dir = 0; dir < 2; dir++) {
        int status = traverse(q->kid[dir]);
        switch(status) {
        case 1: break;
        case 0: return 0;
        default: return status;
        }
      }
      return 1;
    }
    return fun((BLT_it *)p);
  }
  return traverse(top);
}

int main() {
  BLT* blt = blt_new();
  void add(char *s) { blt_put(blt, s, s); }
  void del(char *s) { blt_delete(blt, s); }
  split("the quick brown fox jumps over the lazy dog", add);
  split("tee quiet brow fix jump overload l d", add);
  split("thee thigh though thumb", add);
  int cb(BLT_it *it) { printf(" %s", it->key); return 1; }
  blt_allprefixed(blt, "t", cb);
  puts("");
  blt_allprefixed(blt, "th", cb);
  puts("");
  puts("forward:");
  for (BLT_it *it = blt_first(blt); it; it = blt_next(blt, it)) {
    printf(" %s", it->key);
  }
  puts("");
  split("tee quiet brow fix jump overload l d", del);
  split("thee thigh though thumb", del);
  puts("reverse:");
  for (BLT_it *it = blt_last(blt); it; it = blt_prev(blt, it)) {
    printf(" %s", it->key);
  }
  puts("");
  printf("%s\n", blt_ceil(blt, "dog")->key);
  printf("%s\n", blt_ceil(blt, "cat")->key);
  printf("%s\n", blt_ceil(blt, "fog")->key);
  printf("%s\n", blt_ceil(blt, "foz")->key);
  return 0;
}
