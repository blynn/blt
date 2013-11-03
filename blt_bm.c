// Benchmark BLT. For example:
//
//   $ blt_bm < /usr/share/dict/words

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "bm.h"
#include "blt.h"

#define REP(i,n) for(int i=0;i<n;i++)

void f(char **key, int m) {
  BLT *blt = blt_new();

  int count = 0;
  bm_init();
  REP(i, m) blt_put(blt, key[i], (void *) (intptr_t) i);
  bm_report("BLT insert");
  REP(i, m) if (i != (intptr_t) blt_get(blt, key[i])->data) {
    fprintf(stderr, "BUG!\n");
    exit(1);
  }
  bm_report("BLT get");
  for (BLT_IT *it = blt_first(blt); it; it = blt_next(blt, it)) count++;
  if (count != m) {
    fprintf(stderr, "BUG!\n");
    exit(1);
  }
  bm_report("BLT iterate");
  count = 0;
  int f(BLT_IT *ignore) {
     count++;
     return 1;
  }
  blt_allprefixed(blt, "", f);
  if (count != m) {
    fprintf(stderr, "BUG!\n");
    exit(1);
  }
  bm_report("BLT allprefixed");
  printf("BLT overhead: %lu bytes\n", blt_overhead(blt));
  bm_init();
  REP(i, m) blt_delete(blt, key[i]);
  bm_report("BLT delete");
}

int main() {
  bm_read_keys(f);
  return 0;
}
