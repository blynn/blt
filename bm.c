// Simple benchmark library.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

void bm_read_keys(void (*cb)(char **key, int m)) {
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
  cb(key, m);
}
