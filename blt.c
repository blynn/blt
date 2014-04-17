// See http://www.imperialviolet.org/binary/critbit.pdf
//
// Differences:
//   - We only have one child pointer. We allocate sibling nodes at the same
//     time; they lie in adjacent blocks of memory. The child pointer points
//     to the left child. There is no waste because every node is either
//     external or has two children.
//   - We fold strcmp into the crit-bit finder. If there is no crit-bit
//     then we have a match.
//   - During lookup, if the key is shorter than the position of the
//     crit bit in the current node, the path we take is irrelevant.
//     Ideally, we'd take one of the shortest paths to a leaf (the only
//     purpose is to get at a string so we can find the true crit bit),
//     but for simplicity we always follow the left child.
//     Our code skips a tiny bit of computation by assigning
//     direction = 0 rather than c = 0 plus some bit twiddling.
//   - Insertion: while walking down the tree (after we've figured out the crit
//     bit), we're guaranteed that the byte number of the current node is less
//     than the key length, so there's no need for special-case code to handle
//     keys shorter than the crit bit.
//   - We combine a couple of comparisons. Instead of byte0 < byte1 and then
//     mask0 < mask1 if they are equal, we simplify to:
//       (byte0 << 8) + mask0 < (byte1 << 8) + mask1
//   - Deletion: we can return early if the key length is shorter than
//     the current node's critical bit, as this implies the key is absent.

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "blt.h"

// Returns the byte where each bit is 1 except for the bit corresponding to
// the leading bit of x.
// Storing the crit bit in this mask form simplifies decide().
static inline uint8_t to_mask(uint8_t x) {
  // SWAR trick that sets every bit after the leading bit to 1.
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  // Zero all the bits after the leading bit then invert.
  return (x & ~(x >> 1)) ^ 255;
  if (0) {
    // Alternative that performs better when there are few set bits.
    // Zero all bits except leading bit with a bit-twiddling trick.
    while (x&(x-1)) x &= x-1;
    // Invert.
    return 255 - x;
  }
}
static inline int decide(uint8_t c, uint8_t m) { return (1 + (m | c)) >> 8; }

// An internal node. Leaf nodes are described by BLT_IT.
struct blt_node_s {
  unsigned int byte:32;     // Byte # of difference.
  unsigned int mask:8;      // ~mask = the crit bit within the byte.
  unsigned int padding:23;
  // The following bit corresponds to the last bit of the pointer to the key
  // in the external node, which is always zero due to malloc alignment.
  unsigned int is_internal:1;
  struct blt_node_s *kid;
};
typedef struct blt_node_s *blt_node_ptr;

struct BLT {
  struct blt_node_s root[1];
  int empty;
};

BLT *blt_new() {
  BLT *blt = malloc(sizeof(*blt));
  blt->empty = 1;
  return blt;
}

void blt_clear(BLT *blt) {
  void free_node(blt_node_ptr p) {
    if (!p->is_internal) {
      free(((BLT_IT *) p)->key);
      return;
    }
    blt_node_ptr q = p->kid;
    free_node(q);
    free_node(q + 1);
    free(q);
  }
  if (!blt->empty) free_node(blt->root);
  free(blt);
}

size_t blt_overhead(BLT *blt) {
  size_t n = sizeof(BLT);
  if (blt->empty) return n;
  void add(blt_node_ptr p) {
    if (p->is_internal) {
      n += 2 * sizeof(struct blt_node_s);
      add(p->kid);
      add(p->kid + 1);
    }
  }
  add(blt->root);
  return n;
}

void blt_dump(BLT* blt, blt_node_ptr p) {
  if (blt->empty) return;
  if (p->is_internal) {
    blt_dump(blt, p->kid);
    blt_dump(blt, p->kid + 1);
    return;
  }
  printf("  %s\n", (char *) ((BLT_IT *) p)->key);
}

static BLT_IT *blt_firstlast(blt_node_ptr p, int dir) {
  if (!p) return 0;
  while (p->is_internal) p = ((blt_node_ptr)p->kid) + dir;
  return (BLT_IT *)p;
}

BLT_IT *blt_first(BLT *blt) {
  return blt->empty ? 0 : blt_firstlast(blt->root, 0);
}

BLT_IT *blt_last (BLT *blt) {
  return blt->empty ? 0 : blt_firstlast(blt->root, 1);
}

// 'it' must be an element of 'blt'.
BLT_IT *blt_nextprev(BLT *blt, BLT_IT *it, int way) {
  blt_node_ptr p = blt->root, other = 0;
  while (p->is_internal) {
    blt_node_ptr q = p->kid;
    int dir = decide(p->mask, it->key[p->byte]);
    if (dir == way) other = q + 1 - way;
    p = q + dir;
  }
  assert(!strcmp(((BLT_IT *)p)->key, it->key));
  return blt_firstlast(other, way);
}

BLT_IT *blt_next(BLT *blt, BLT_IT *it) { return blt_nextprev(blt, it, 0); }
BLT_IT *blt_prev(BLT *blt, BLT_IT *it) { return blt_nextprev(blt, it, 1); }

// Walk down the tree as if the key is there.
static inline BLT_IT *confident_get(BLT *blt, char *key) {
  if (blt->empty) return 0;
  blt_node_ptr p = blt->root;
  int keylen = strlen(key);
  while (p->is_internal) {
    // When p->byte >= keylen, key is absent, but we must return something.
    // Either kid works; we pick 0 each time.
    p = p->kid + (p->byte < keylen && decide(key[p->byte], p->mask));
  }
  return (void *)p;
}

BLT_IT *blt_ceilfloor(BLT *blt, char *key, int way) {
  BLT_IT *p = confident_get(blt, key);
  if (!p) return 0;
  // Compare keys.
  for(char *c = key, *pc = p->key;; c++, pc++) {
    // XOR the current bytes being compared.
    uint8_t x = *c ^ *pc;
    if (x) {
      int byte = c - key;
      x = to_mask(x);
      int ndir = decide(x, key[byte]);
      // Walk down the tree until we hit an external node or a node
      // whose crit bit is higher.
      blt_node_ptr p = blt->root, other = 0;
      while (p->is_internal) {
        if ((byte << 8) + x < (p->byte << 8) + p->mask) break;
        int dir = decide(p->mask, key[p->byte]);
        blt_node_ptr q = p->kid;
        if (dir == way) other = q + 1 - way;
        p = q + dir;
      }
      if (ndir == way) other = p;
      return blt_firstlast(other, way);
    }
    if (!*c) return (BLT_IT *)p;
  }
}

BLT_IT *blt_ceil (BLT *blt, char *key) { return blt_ceilfloor(blt, key, 0); }
BLT_IT *blt_floor(BLT *blt, char *key) { return blt_ceilfloor(blt, key, 1); }

void *blt_put_with(BLT *blt, char *key, void *data,
                   void *(*already_present_cb)(BLT_IT *)) {
  BLT_IT *p = confident_get(blt, key);
  if (!p) {  // Empty tree case.
    blt->empty = 0;
    BLT_IT *leaf = (BLT_IT *) blt->root;
    leaf->key = strdup(key);
    leaf->data = data;
    return 0;
  }
  // Compare keys.
  for(char *c = key, *pc = p->key;; c++, pc++) {
    // XOR the current bytes being compared.
    uint8_t x = *c ^ *pc;
    if (x) {
      // Allocate 2 adjacent nodes and copy the leaf into the appropriate side.
      blt_node_ptr n = malloc(2 * sizeof(*n));
      x = to_mask(x);
      int ndir = decide(*c, x);
      BLT_IT *leaf = (BLT_IT *) (n + ndir);
      leaf->key = strdup(key);
      leaf->data = data;

      // Find the first node in the path whose critbit is higher than ours,
      // or the external node.
      int byte = c - key;
      blt_node_ptr p = (blt_node_ptr) blt->root;
      while(p->is_internal) {
        if ((byte << 8) + x < (p->byte << 8) + p->mask) break;
        p = p->kid + decide(key[p->byte], p->mask);
      }

      // Copy the node's contents to the other side of our 2 new adjacent nodes,
      // then replace it with our critbit and pointer to the new nodes.
      n[1 - ndir] = *p;
      p->byte = byte;
      p->mask = x;
      p->kid = n;
      p->is_internal = 1;
      return 0;
    }
    if (!*c) return already_present_cb(p);
  }
}

void *blt_put(BLT *blt, char *key, void *data) {
  void *f(BLT_IT *it) {
    void *orig = it->data;
    it->data = data;
    return orig;
  }
  return blt_put_with(blt, key, data, f);
}

int blt_put_if_absent(BLT *blt, char *key, void *data) {
  void *f(BLT_IT *it) { return (void *) 1; }
  return blt_put_with(blt, key, data, f) != 0;
}

int blt_delete(BLT *blt, char *key) {
  if (blt->empty) return 0;
  int keylen = strlen(key);
  blt_node_ptr p = blt->root, q0 = 0, q = 0;
  int dir;
  while (p->is_internal) {
    q = p->kid;
    if (p->byte > keylen) return 0;
    dir = decide(p->mask, key[p->byte]);
    q0 = p;
    p = q + dir;
  }
  BLT_IT *leaf = (BLT_IT *)p;
  if (strcmp(key, leaf->key)) return 0;
  free(leaf->key);
  if (!q) {
    blt->empty = 1;
    return 1;
  }
  *q0 = q[1 - dir];
  free(q);
  return 1;
}

int blt_allprefixed(BLT *blt, char *key, int (*fun)(BLT_IT *)) {
  if (blt->empty) return 1;
  blt_node_ptr p = blt->root, top = p;
  int keylen = strlen(key);
  while (p->is_internal) {
    if (p->byte >= keylen) {
      p = p->kid;
    } else {
      p = p->kid + decide(p->mask, key[p->byte]);
      top = p;
    }
  }
  if (strncmp(key, ((BLT_IT *)p)->key, keylen)) return 1;
  int traverse(blt_node_ptr p) {
    if (p->is_internal) {
      for (int dir = 0; dir < 2; dir++) {
        int status = traverse(p->kid + dir);
        switch(status) {
        case 1: break;
        case 0: return 0;
        default: return status;
        }
      }
      return 1;
    }
    return fun((BLT_IT *)p);
  }
  return traverse(top);
}

BLT_IT *blt_get(BLT *blt, char *key) {
  if (blt->empty) return 0;
  blt_node_ptr p = blt->root;
  int keylen = strlen(key);
  while (p->is_internal) {
    // We could shave off a few percent by skipping checks like the
    // following, but buffer overreads are bad form.
    if (p->byte > keylen) return 0;
    p = p->kid + decide(key[p->byte], p->mask);
  }
  BLT_IT *r = (BLT_IT *)p;
  return strcmp(key, r->key) ? 0 : r;
}
