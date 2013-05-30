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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct blt_leaf_s {
  void *key, *data;
};
typedef struct blt_leaf_s *blt_leaf_ptr;

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

void blt_put(BLT *blt, void *key, void *data) {
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
  while (1&(intptr_t)p) {
    blt_node_ptr q = (void *) (p - 1);
    int dir = 0;  // Arbitrary; when q->byte >= keylen, either kid works.
    if (q->byte < keylen) dir = (1 + (q->mask | ((char *) key)[q->byte])) >> 8;
    p = q->kid[dir];
  }
  // Compare keys.
  char *c = key, *pc = ((blt_leaf_ptr) p)->key;
  for (;;) {
    // XOR the current bytes being compared.
    char x = *c ^ *pc;
    if (x) {
      // Allocate a new node.
      // Find crit bit using bit twiddling tricks.
      blt_node_ptr n = malloc(sizeof(*n));
      n->byte = c - (char *) key;
      while (x&(x-1)) x &= x-1;
      n->mask = ~x;
      blt_leaf_ptr leaf = malloc(sizeof(*leaf));
      leaf->key = strdup(key);
      leaf->data = data;
      int ndir = (1 + (n->mask | ((char *) key)[n->byte])) >> 8;
      n->kid[ndir] = leaf;

      // Insert the new node.
      // Walk down the tree until we hit an external node or a node
      // whose crit bit is higher.
      void **p0 = &blt->root;
      for (;;) {
        char *p = *p0;
        if (!(1&(intptr_t)p)) break;
        blt_node_ptr q = (void *) (p - 1);
        if (n->byte < q->byte ||
            (n->byte == q->byte && n->mask < q->mask)) break;
        p0 = q->kid + ((1 + (q->mask | ((char *) key)[q->byte])) >> 8);
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
  if (1&(intptr_t)p) {
    blt_node_ptr q = (void *) ((char *)p - 1);
    blt_dump(blt, q->kid[0]);
    blt_dump(blt, q->kid[1]);
    return;
  }
  printf("  %s\n", (char *) ((blt_leaf_ptr) p)->key);
}

int main() {
  BLT* blt = blt_new();
  char sentence[] = "the quick brown fox jumps over the lazy dog";
  char *s = sentence;
  char *s1 = s+1;
  char *end = s + sizeof(sentence);
  for (;;) {
    if (*s1 == ' ') *s1 = '\0';
    if (!*s1) {
      blt_put(blt, s, 0);
      s = s1 + 1;
      if (s == end) break;
      s1 = s + 1;
    } else {
      s1++;
    }
  }
  blt_dump(blt, blt->root);
  return 0;
}
