#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "blt.h"

#define EXPECT(_x_) if (_x_); else fprintf(stderr, "%s:%d: FAIL\n", __FILE__, __LINE__)
#define FAIL() fprintf(stderr, "%s:%d: ABORT\n", __FILE__, __LINE__), exit(1)
#define F(i, n) for(int i = 0; i < n; i++)

static void split(char *s, void (*fun)(char *)) {
  if (!*s) return;
  for (char *p = s, *q = p;; q++) if (*q == ' ' || *q == '\0') {
    char *tmp = strndup(p, q - p);
    fun(tmp);
    free(tmp);
    if (*q == '\0') break;
    p = q + 1;
  }
}

struct arr_s {
  void **p;
  int max, n;
};
typedef struct arr_s *arr_t;

arr_t arr_new() {
  arr_t r = malloc(sizeof(*r));
  r->n = 0;
  r->max = 8;
  r->p = malloc(sizeof(*r->p) * r->max);
  return r;
}

void arr_add(arr_t r, void *p) {
  if (r->n == r->max) r->p = realloc(r->p, sizeof(*r->p) * (r->max *= 2));
  r->p[r->n++] = p;
}

BLT *make_blt(char *line) {
  BLT* blt = blt_new();
  split(line, ({ void _(char *s) { blt_put(blt, s, 0); }_; }));
  return blt;
}

arr_t make_arr(char *line) {
  arr_t a = arr_new();
  split(line, ({ void _(char *s) { arr_add(a, strdup(s)); }_; }));
  return a;
}

void nuke_arr(arr_t a) {
  F(i, a->n) free(a->p[i]);
  free(a->p);
  free(a);
}

void test_traverse(char *line) {
  BLT* blt = blt_new();
  arr_t a = arr_new();
  split(line, ({ void _(char *s) {
    blt_put(blt, s, 0);
    arr_add(a, strdup(s));
  }_; }));
  qsort(a->p, a->n, sizeof(*a->p), ({ int _(const void *p, const void *q) {
    return strcmp(*(char **)p, *(char **)q);
  }_; }));
  int n = 0, count = 0;
  // Check blt_forall() retrieves keys in order.
  blt_forall(blt, ({void _(BLT_IT *it){
    while (n + 1 < a->n && !strcmp(a->p[n], a->p[n + 1])) n++;
    if (n == a->n) FAIL();
    EXPECT(!strcmp(it->key, a->p[n++]));
    count++;
  }_;}));
  EXPECT(count == blt_size(blt));
  EXPECT(n == a->n);

  // Check blt_first() and blt_next().
  n = count = 0;
  for (BLT_IT *it = blt_first(blt); it; it = blt_next(blt, it)) {
    while (n + 1 < a->n && !strcmp(a->p[n], a->p[n + 1])) n++;
    if (n == a->n) FAIL();
    EXPECT(!strcmp(it->key, a->p[n++]));
    count++;
  }
  EXPECT(count == blt_size(blt));
  EXPECT(n == a->n);

  // Check blt_last() and blt_prev().
  n = a->n - 1; count = 0;
  for (BLT_IT *it = blt_last(blt); it; it = blt_prev(blt, it)) {
    while (n - 1 >= 0 && !strcmp(a->p[n], a->p[n - 1])) n--;
    if (n == -1) FAIL();
    EXPECT(!strcmp(it->key, a->p[n--]));
    count++;
  }
  EXPECT(count == blt_size(blt));
  EXPECT(n == -1);

  nuke_arr(a);
  blt_clear(blt);
}

void check_prefix(BLT* blt, char *prefix, char *want) {
  arr_t a = make_arr(want);
  int n = 0;
  blt_allprefixed(blt, prefix, ({int _(BLT_IT *it){
    while (n + 1 < a->n && !strcmp(a->p[n], a->p[n + 1])) n++;
    if (n == a->n) FAIL();
    EXPECT(!strcmp(it->key, a->p[n++]));
    return 1;
  }_;}));
  EXPECT(n == a->n);
  nuke_arr(a);
}

int main() {
  test_traverse("");
  test_traverse("one-string");
  test_traverse("two strings");
  test_traverse("red string blue string");
  test_traverse("the quick brown fox jumps over the lazy dog");
  test_traverse("  2 spaces   means  empty   strings   are tested");

  // Generate strings like "a aaa a aa aaa".
  int n = 32;
  char s[n * 16];
  srand(time(0));
  char *c = s;
  F(i, n) {
    if (i) *c++ = ' ';
    int len = rand() % 12 + 1;
    F(j, len) *c++ = 'a';
  }
  *c = 0;
  test_traverse(s);

  // Generate strings containing random nonzero chars.
  c = s;
  F(i, n) {
    if (i) *c++ = ' ';
    int len = rand() % 12 + 1;
    // This could be a space. That's fine.
    F(j, len) *c++ = (char) (rand() % 255 + 1);
  }
  *c = 0;
  test_traverse(s);

  BLT *blt = make_blt("a aardvark b ben blink bliss blt blynn");
  check_prefix(blt, "b", "b ben blink bliss blt blynn");
  check_prefix(blt, "bl", "blink bliss blt blynn");
  check_prefix(blt, "bli", "blink bliss");
  check_prefix(blt, "a", "a aardvark");
  check_prefix(blt, "aa", "aardvark");
  check_prefix(blt, "c", "");
  EXPECT(!strcmp(blt_ceil(blt, "blink")->key, "blink"));
  EXPECT(!strcmp(blt_ceil(blt, "blink182")->key, "bliss"));
  EXPECT(!strcmp(blt_floor(blt, "blink")->key, "blink"));
  EXPECT(!strcmp(blt_floor(blt, "blink182")->key, "blink"));
  blt_clear(blt);

  return 0;
}
