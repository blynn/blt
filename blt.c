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
//   - When following child pointers, rather than p->kid + predicate(),
//     we prefer predicate() ? p->kid + 1 : p->kid, as this is faster on my
//     system. We can thus relax predicate(): instead of returning 0 or 1,
//     it's fine if it simply returns 0 or nonzero. This means we can store
//     the plain bitmask instead of its inversion, and check the bit with
//     a single AND.

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "blt.h"

// Returns the byte where each bit is 1 except for the bit corresponding to
// the leading bit of x.
static inline uint8_t to_mask(uint8_t x) {
  // SWAR trick that sets every bit after the leading bit to 1.
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  // Zero all the bits after the leading bit then invert.
  return x & ~(x >> 1);
  if (0) {
    // Alternative that performs better when there are few set bits.
    // Zero all bits except leading bit with a bit-twiddling trick.
    while (x&(x-1)) x &= x-1;
    // Invert.
    return 255 - x;
  }
}

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

static inline blt_node_ptr follow(blt_node_ptr p, char *key) {
  return key[p->byte] & p->mask ? p->kid + 1 : p->kid;
}

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

BLT_IT *blt_next(BLT *blt, BLT_IT *it) {
  blt_node_ptr p = blt->root, other = 0;
  while (p->is_internal) {
    if (!(it->key[p->byte] & p->mask)) {
      other = p->kid + 1;
      p = p->kid;
    } else {
      p = p->kid + 1;
    }
  }
  assert(!strcmp(((BLT_IT *)p)->key, it->key));
  return blt_firstlast(other, 0);
}

BLT_IT *blt_prev(BLT *blt, BLT_IT *it) {
  blt_node_ptr p = blt->root, other = 0;
  while (p->is_internal) {
    if (it->key[p->byte] & p->mask) {
      other = p->kid;
      p = p->kid + 1;
    } else {
      p = p->kid;
    }
  }
  assert(!strcmp(((BLT_IT *)p)->key, it->key));
  return blt_firstlast(other, 1);
}

// Walk down the tree as if the key is there.
static inline BLT_IT *confident_get(BLT *blt, char *key) {
  if (blt->empty) return 0;
  blt_node_ptr p = blt->root;
  int keylen = strlen(key);
  while (p->is_internal) {
    // When p->byte >= keylen, key is absent, but we must return something.
    // Either kid works; we pick 0 each time.
    p = p->byte < keylen && (key[p->byte] & p->mask) ? p->kid + 1 : p->kid;
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
      // Walk down the tree until we hit an external node or a node
      // whose crit bit is higher.
      blt_node_ptr p = blt->root, other = 0;
      while (p->is_internal) {
        if ((byte << 8) + p->mask < (p->byte << 8) + x) break;
        int dir = !!(p->mask & key[p->byte]);
        blt_node_ptr q = p->kid;
        if (dir == way) other = q + 1 - way;
        p = q + dir;
      }
      int ndir = !!(x & key[byte]);
      if (ndir == way) other = p;
      return blt_firstlast(other, way);
    }
    if (!*c) return (BLT_IT *)p;
  }
}

BLT_IT *blt_ceil (BLT *blt, char *key) { return blt_ceilfloor(blt, key, 0); }
BLT_IT *blt_floor(BLT *blt, char *key) { return blt_ceilfloor(blt, key, 1); }

BLT_IT *blt_setp(BLT *blt, char *key, int *is_new) {
  BLT_IT *p = confident_get(blt, key);
  if (!p) {  // Empty tree case.
    blt->empty = 0;
    BLT_IT *leaf = (BLT_IT *) blt->root;
    leaf->key = strdup(key);
    leaf->data = 0;
    if (is_new) *is_new = 1;
    return leaf;
  }
  // Compare keys.
  for(char *c = key, *pc = p->key;; c++, pc++) {
    // XOR the current bytes being compared.
    uint8_t x = *c ^ *pc;
    if (x) {
      // Allocate 2 adjacent nodes and copy the leaf into the appropriate side.
      blt_node_ptr n = malloc(2 * sizeof(*n));
      x = to_mask(x);
      BLT_IT *leaf = (BLT_IT *)n;
      blt_node_ptr other = n;
      if (*c & x) leaf++; else other++;

      leaf->key = strdup(key);
      leaf->data = 0;

      // Find the first node in the path whose critbit is higher than ours,
      // or the external node.
      int byte = c - key;
      blt_node_ptr p = (blt_node_ptr) blt->root;
      while(p->is_internal) {
        if ((byte << 8) + p->mask < (p->byte << 8) + x) break;
        p = follow(p, key);
      }

      // Copy the node's contents to the other side of our 2 new adjacent nodes,
      // then replace it with our critbit and pointer to the new nodes.
      *other = *p;
      p->byte = byte;
      p->mask = x;
      p->kid = n;
      p->is_internal = 1;
      if (is_new) *is_new = 1;
      return leaf;
    }
    if (!*c) {
      if (is_new) *is_new = 0;
      return p;
    }
  }
}

BLT_IT *blt_set(BLT *blt, char *key) { return blt_setp(blt, key, 0); }

BLT_IT *blt_put(BLT *blt, char *key, void *data) {
  BLT_IT *it = blt_set(blt, key);
  it->data = data;
  return it;
}

int blt_put_if_absent(BLT *blt, char *key, void *data) {
  int is_new;
  BLT_IT *it = blt_setp(blt, key, &is_new);
  if (is_new) it->data = data;
  return !is_new;
}

int blt_delete(BLT *blt, char *key) {
  if (blt->empty) return 0;
  int keylen = strlen(key);
  blt_node_ptr p = blt->root, p0 = 0;
  while (p->is_internal) {
    if (p->byte > keylen) return 0;
    p0 = p;
    p = follow(p, key);
  }
  BLT_IT *leaf = (BLT_IT *)p;
  if (strcmp(key, leaf->key)) return 0;
  free(leaf->key);
  if (!p0) {
    blt->empty = 1;
    return 1;
  }
  blt_node_ptr q = p0->kid;
  *p0 = *(p == q ? q + 1 : q);
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
      p = follow(p, key);
      top = p;
    }
  }
  if (strncmp(key, ((BLT_IT *)p)->key, keylen)) return 1;
  int traverse(blt_node_ptr p) {
    if (p->is_internal) {
      int status = traverse(p->kid);
      if (status != 1) return status;
      status = traverse(p->kid + 1);
      if (status != 1) return status;
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
    p = follow(p, key);
  }
  BLT_IT *r = (BLT_IT *)p;
  return strcmp(key, r->key) ? 0 : r;
}

int blt_empty(BLT *blt) {
  return blt->empty;
}

int blt_size(BLT *blt) {
  int r = 0;
  void f(BLT_IT *it) { r++; }
  blt_forall(blt, f);
  return r;
}
