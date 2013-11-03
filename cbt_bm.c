// Benchmark CBT. For example:
//
//   $ cbt_bm < /usr/share/dict/words

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "bm.h"
#include "cbt.h"

#define REP(i,n) for(int i=0;i<n;i++)

void f(char **key, int m) {
  cbt_t cbt = cbt_new();

  int count = 0;
  bm_init();
  REP(i, m) cbt_put_at(cbt, (void *) (intptr_t) i, key[i]);
  bm_report("CBT insert");
  REP(i, m) if (i != (intptr_t) cbt_get_at(cbt, key[i])) {
    fprintf(stderr, "BUG!\n");
    exit(1);
  }
  bm_report("CBT get");
  for (cbt_it it = cbt_first(cbt); it; it = cbt_next(it)) count++;
  if (count != m) {
    fprintf(stderr, "BUG!\n");
    exit(1);
  }
  bm_report("CBT iterate");
  printf("CBT overhead: %lu bytes\n", cbt_overhead(cbt));
  bm_init();
  REP(i, m) cbt_remove(cbt, key[i]);
  bm_report("CBT delete");
}

int main() {
  bm_read_keys(f);
  return 0;
}
