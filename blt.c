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

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static inline int has_tag(void *p) { return 1 & (intptr_t)p; }
static inline void *untag(void *p) { return ((char *)p) - 1; }

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
    int dir = (1 + (q->mask | it->key[q->byte])) >> 8;
    if (dir == way) other = q->kid[1 - way];
    p = q->kid[dir];
  }
  assert(!strcmp(((BLT_it *)p)->key, it->key));
  return blt_firstlast(other, way);
}

BLT_it *blt_next(BLT *blt, BLT_it *it) { return blt_nextprev(blt, it, 0); }
BLT_it *blt_prev(BLT *blt, BLT_it *it) { return blt_nextprev(blt, it, 1); }

BLT_it *blt_ceilfloor(BLT *blt, char *key, int way) {
  // Walk down the tree as if the key is there.
  char *p = blt->root;
  if (!p) return NULL;
  int keylen = strlen(key);
  while (has_tag(p)) {
    blt_node_ptr q = untag(p);
    int dir = 0;  // Arbitrary; when q->byte >= keylen, either kid works.
    if (q->byte < keylen) dir = (1 + (q->mask | key[q->byte])) >> 8;
    p = q->kid[dir];
  }
  // Compare keys.
  char *c = key, *pc = ((blt_leaf_ptr) p)->key;
  for (;;) {
    // XOR the current bytes being compared.
    unsigned char x = *c ^ *pc;
    if (x) {
      int byte = c - key;
      while (x&(x-1)) x &= x-1;
      x = ~x;
      int ndir = (1 + (x | key[byte])) >> 8;
      void *other = 0;
      // Walk down the tree until we hit an external node or a node
      // whose crit bit is higher.
      void **p0 = &blt->root;
      for (;;) {
        char *p = *p0;
        if (!has_tag(p)) break;
        blt_node_ptr q = untag(p);
        if (byte < q->byte ||
            (byte == q->byte && x < q->mask)) break;
        int dir = (1 + (q->mask | key[q->byte])) >> 8;
        if (dir == way) {
          other = q->kid[1 - way];
        }
        p0 = q->kid + dir;
      }
      if (ndir == way) {
        other = *p0;
      }
      return blt_firstlast(other, way);
    }
    if (!*c) {
      return (BLT_it *)p;
    }
    c++, pc++;
  }
}

BLT_it *blt_ceil (BLT *blt, char *key) { return blt_ceilfloor(blt, key, 0); }
BLT_it *blt_floor(BLT *blt, char *key) { return blt_ceilfloor(blt, key, 1); }

void blt_put(BLT *blt, char *key, void *data) {
  blt->count++;
  if (!blt->root) {  // Empty tree case.
    blt_leaf_ptr p = malloc(sizeof(*p));
    blt->root = p;
    p->key = strdup(key);
    p->data = data;
    return;
  }
  // Walk down the tree as if the key is there.
  char *p = blt->root;
  int keylen = strlen(key);
  while (has_tag(p)) {
    blt_node_ptr q = untag(p);
    int dir = 0;  // Arbitrary; when q->byte >= keylen, either kid works.
    if (q->byte < keylen) dir = (1 + (q->mask | key[q->byte])) >> 8;
    p = q->kid[dir];
  }
  // Compare keys.
  char *c = key, *pc = ((blt_leaf_ptr) p)->key;
  for (;;) {
    // XOR the current bytes being compared.
    unsigned char x = *c ^ *pc;
    if (x) {
      // Allocate a new node.
      // Find crit bit using bit twiddling tricks.
      blt_node_ptr n = malloc(sizeof(*n));
      n->byte = c - key;
      while (x&(x-1)) x &= x-1;
      n->mask = ~x;
      blt_leaf_ptr leaf = malloc(sizeof(*leaf));
      leaf->key = strdup(key);
      leaf->data = data;
      int ndir = (1 + (n->mask | key[n->byte])) >> 8;
      n->kid[ndir] = leaf;

      // Insert the new node.
      // Walk down the tree until we hit an external node or a node
      // whose crit bit is higher.
      void **p0 = &blt->root;
      for (;;) {
        char *p = *p0;
        if (!has_tag(p)) break;
        blt_node_ptr q = untag(p);
        if (n->byte < q->byte ||
            (n->byte == q->byte && n->mask < q->mask)) break;
        p0 = q->kid + ((1 + (q->mask | key[q->byte])) >> 8);
      }
      n->kid[1 - ndir] = *p0;
      *p0 = 1 + (char *)n;
      return;
    }
    if (!*c) {
      ((blt_leaf_ptr) p)->data = data;
      return;
    }
    c++, pc++;
  }
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

int main() {
  BLT* blt = blt_new();
  char sentence[] = "the quick brown fox jumps over the lazy dog";
  for (char *s = sentence, *t = s; s != sentence + sizeof(sentence); t++) {
    if (*t == ' ') *t = '\0';
    if (!*t) blt_put(blt, s, 0), s = t + 1;
  }
  blt_dump(blt, blt->root);
  for (BLT_it *it = blt_first(blt); it; it = blt_next(blt, it)) {
    printf("it: %s\n", it->key);
  }
  for (BLT_it *it = blt_last(blt); it; it = blt_prev(blt, it)) {
    printf("it: %s\n", it->key);
  }
  printf("%s\n", blt_ceil(blt, "dog")->key);
  printf("%s\n", blt_ceil(blt, "cat")->key);
  printf("%s\n", blt_ceil(blt, "fog")->key);
  printf("%s\n", blt_ceil(blt, "foz")->key);
  return 0;
}
