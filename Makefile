all:
	$(CC) -O3 -march=native bulk.c common.c -o bulk.exe
