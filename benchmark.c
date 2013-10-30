// Benchmark BLT against CBT crit-bit trees. For example:
//
//   $ benchmark < /usr/share/dict/words

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cbt.h"
#include "blt.h"

#define REP(i,n) for(int i=0;i<n;i++)

static struct timespec bm_tp[2];

void bm_init() {
  clock_gettime(CLOCK_MONOTONIC, bm_tp);
}

void bm_report(char *msg) {
  clock_gettime(CLOCK_MONOTONIC, bm_tp + 1);
  printf("%s: %ld.%09lds\n", msg,
      bm_tp[1].tv_sec - bm_tp[0].tv_sec - (bm_tp[1].tv_nsec < bm_tp[0].tv_nsec),
      bm_tp[1].tv_nsec - bm_tp[0].tv_nsec
          + (bm_tp[1].tv_nsec < bm_tp[0].tv_nsec) * 1000000000L);
  clock_gettime(CLOCK_MONOTONIC, bm_tp);
}

int main() {
  char **key;
  int max = 8, m = 0;
  key = malloc(sizeof(*key) * max);
  for (;;) {
    char *s = 0;
    size_t n;
    ssize_t len = getline(&s, &n, stdin);
    if (feof(stdin)) break;
    if (len == -1) perror("getline"), exit(1);
    if (s[len - 1] == '\n') s[len - 1] = 0;
    key[m++] = s;
    if (m == max) max *= 2, key = realloc(key, sizeof(*key) * max);
  }
  // Randomize order of array.
  for (int i = m-1; i>1; i--) {
    int j = random() % i;
    char *tmp = key[i];
    key[i] = key[j];
    key[j] = tmp;
  }

  BLT *blt = blt_new();
  cbt_t cbt = cbt_new();

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
  printf("BLT overhead: %lu bytes\n", blt_overhead(blt));

  bm_init();
  REP(i, m) cbt_put_at(cbt, (void *) (intptr_t) i, key[i]);
  bm_report("CBT insert");
  REP(i, m) if (i != (intptr_t) cbt_get_at(cbt, key[i])) {
    fprintf(stderr, "BUG!\n");
    exit(1);
  }
  bm_report("CBT get");
  count = 0;
  for (cbt_it it = cbt_first(cbt); it; it = cbt_next(it)) count++;
  if (count != m) {
    fprintf(stderr, "BUG!\n");
    exit(1);
  }
  bm_report("CBT iterate");
  printf("CBT overhead: %lu bytes\n", cbt_overhead(cbt));
}
