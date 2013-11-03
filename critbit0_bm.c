// Benchmarks Adam Langley's critbit library.
//
// Requires commenting out the extern declaration in critbit.h.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "bm.h"
#include "critbit.h"

#define REP(i,n) for(int i=0;i<n;i++)

void f(char **key, int m) {
  critbit0_tree tr[1] = {0};

  int count = 0;
  bm_init();
  REP(i, m) critbit0_insert(tr, key[i]);
  bm_report("critbit0 insert");
  REP(i, m) if (!critbit0_contains(tr, key[i])) {
    fprintf(stderr, "BUG!\n");
    exit(1);
  }
  bm_report("critbit0 get");
  int inc(const char* ignore0, void* ignore1) {
    count++;
    return 1;
  }
  critbit0_allprefixed(tr, "", inc, 0);
  if (count != m) {
    fprintf(stderr, "BUG!\n");
    exit(1);
  }
  bm_report("critbit0 allprefixed");
  REP(i, m) critbit0_delete(tr, key[i]);
  bm_report("critbit0 delete");
}

int main() {
  bm_read_keys(f);
  return 0;
}
