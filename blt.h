struct BLT;
typedef struct BLT BLT;
struct BLT_IT {
  char *key;
  void *data;
};
typedef struct BLT_IT BLT_IT;
BLT *blt_new();
BLT_IT *blt_get(BLT *blt, char *key);
void blt_put(BLT *blt, char *key, void *data);
int blt_delete(BLT *blt, char *key);
int blt_allprefixed(BLT *blt, char *key, int (*fun)(BLT_IT *));
BLT_IT *blt_first(BLT *blt);
BLT_IT *blt_last (BLT *blt);
BLT_IT *blt_next(BLT *blt, BLT_IT *it);
BLT_IT *blt_prev(BLT *blt, BLT_IT *it);
BLT_IT *blt_ceil (BLT *blt, char *key);
BLT_IT *blt_floor(BLT *blt, char *key);
size_t blt_overhead(BLT *blt);
