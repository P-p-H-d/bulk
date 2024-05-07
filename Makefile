all:
	$(CC) -O3 -march=native bulk.c common.c -DMAX_PREFETCH=16  -o bulk16.exe
	$(CC) -O3 -march=native bulk.c common.c -DMAX_PREFETCH=32  -o bulk32.exe
	$(CC) -O3 -march=native bulk.c common.c -DMAX_PREFETCH=64  -o bulk64.exe
	$(CC) -O3 -march=native bulk.c common.c -DMAX_PREFETCH=128 -o bulk128.exe
	$(CC) -O3 -march=native bulk.c common.c -DMAX_PREFETCH=256 -o bulk256.exe
