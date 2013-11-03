// Benchmarks map<string, int>.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <map>
#include <string>
#include <vector>

#define REP(i,n) for(int i=0;i<n;i++)

using namespace std;

static struct timespec bm_tp[2];

void bm_init() {
  clock_gettime(CLOCK_MONOTONIC, bm_tp);
}

void bm_report(const string& msg) {
  clock_gettime(CLOCK_MONOTONIC, bm_tp + 1);
  printf("%s: %ld.%09lds\n", msg.c_str(),
      bm_tp[1].tv_sec - bm_tp[0].tv_sec - (bm_tp[1].tv_nsec < bm_tp[0].tv_nsec),
      bm_tp[1].tv_nsec - bm_tp[0].tv_nsec
          + (bm_tp[1].tv_nsec < bm_tp[0].tv_nsec) * 1000000000L);
  clock_gettime(CLOCK_MONOTONIC, bm_tp);
}

int main() {
  vector<string> key;
  int m = 0;
  for (;;) {
    char *s = 0;
    size_t n;
    ssize_t len = getline(&s, &n, stdin);
    if (feof(stdin)) break;
    if (len == -1) perror("getline"), exit(1);
    if (s[len - 1] == '\n') s[len - 1] = 0;
    key.push_back(s);
    m++;
  }
  // Randomize order of array.
  for (int i = m-1; i>1; i--) {
    int j = random() % i;
    string tmp = key[i];
    key[i] = key[j];
    key[j] = tmp;
  }

  map<string, int> smap;

  bm_init();
  REP(i, m) smap[key[i]] = i;
  bm_report("map insert");
  REP(i, m) if (i != smap[key[i]]) {
    fprintf(stderr, "BUG!\n");
    exit(1);
  }
  bm_report("map get");
  int count = 0;
  for (map<string, int>::iterator it = smap.begin(); it != smap.end(); it++) count++;
  if (count != m) {
    fprintf(stderr, "BUG!\n");
    exit(1);
  }
  bm_report("map iterate");
}
