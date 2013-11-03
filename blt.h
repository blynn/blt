// = Crit-bit trees =
//
// Usage:
//
//   // Create a new tree.
//   BLT* blt = blt_new();
//
//   // Insert a couple of keys.
//   blt_put(blt, "hello", pointer1);
//   blt_put(blt, "world", pointer2);
//
//   // Check they have the values we expect.
//   if (blt_get(blt, "hello")->data != pointer1 ||
//       blt_get(blt, "hello")->data != pointer2) exit(1);
//
//   // Delete the tree.
//   blt_clear(blt);

struct BLT;
typedef struct BLT BLT;
struct BLT_IT {
  char *key;
  void *data;
};
typedef struct BLT_IT BLT_IT;

// Creates a new tree.
BLT *blt_new();

// Destroys a tree.
void blt_clear(BLT *blt);

// Retrieves the leaf node at a given key.
// Returns NULL if there is no such key.
BLT_IT *blt_get(BLT *blt, char *key);

// Inserts a given key and data pair.
void blt_put(BLT *blt, char *key, void *data);

// Deletes a given key from the tree.
// Returns 1 if a key was deleted, and 0 otherwise.
int blt_delete(BLT *blt, char *key);

// For all leaf nodes with a given prefix, runs a given callback.
int blt_allprefixed(BLT *blt, char *key, int (*fun)(BLT_IT *));

// Returns the leaf node with the smallest key.
BLT_IT *blt_first(BLT *blt);

// Returns the leaf node with the largest key.
BLT_IT *blt_last (BLT *blt);

// Returns the leaf node with the next largest key.
BLT_IT *blt_next(BLT *blt, BLT_IT *it);

// Returns the leaf node with the next smallest key.
BLT_IT *blt_prev(BLT *blt, BLT_IT *it);

// If the given key is present, returns its leaf node.
// Otherwise returns the leaf node with the next largest key if it exists,
// and NULL otherwise.
BLT_IT *blt_ceil (BLT *blt, char *key);

// If the given key is present, returns its leaf node.
// Otherwise returns the leaf node with the next smallest key if it exists,
// and NULL otherwise.
BLT_IT *blt_floor(BLT *blt, char *key);

// Returns the number of bytes used by the tree, excluding memory taken by
// the bytes of the keys.
size_t blt_overhead(BLT *blt);
