#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "blt.h"

static void split(char *s, void (*fun)(char *)) {
  for (char *p = s, *q = p;; q++) if (*q == ' ' || *q == '\0') {
    char *tmp = strndup(p, q - p);
    fun(tmp);
    free(tmp);
    if (*q == '\0') break;
    p = q + 1;
  }
}

int main() {
  BLT* blt = blt_new();
  void add(char *s) { blt_put(blt, s, s); }
  void del(char *s) { blt_delete(blt, s); }
  split("the quick brown fox jumps over the lazy dog", add);
  split("tee quiet brow fix jump overload l d", add);
  split("thee thigh though thumb", add);
  int cb(BLT_IT *it) { printf(" %s", it->key); return 1; }
  blt_allprefixed(blt, "t", cb);
  puts("");
  blt_allprefixed(blt, "th", cb);
  puts("");
  puts("forward:");
  for (BLT_IT *it = blt_first(blt); it; it = blt_next(blt, it)) {
    printf(" %s", it->key);
  }
  puts("");
  split("tee quiet brow fix jump overload l d", del);
  split("thee thigh though thumb", del);
  puts("reverse:");
  for (BLT_IT *it = blt_last(blt); it; it = blt_prev(blt, it)) {
    printf(" %s", it->key);
  }
  puts("");
  printf("%s\n", blt_ceil(blt, "dog")->key);
  printf("%s\n", blt_ceil(blt, "cat")->key);
  printf("%s\n", blt_ceil(blt, "fog")->key);
  printf("%s\n", blt_ceil(blt, "foz")->key);
  blt_clear(blt);
  return 0;
}
