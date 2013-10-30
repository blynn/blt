CFLAGS=--std=gnu99 -Wall -O3
blt_test: blt_test.c blt.c

benchmark: benchmark.c blt.c cbt.c
	$(CC) $(CFLAGS) -o $@ $^ -ltcmalloc
