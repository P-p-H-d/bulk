all:run-test.old run-test.new run-test.up8 run-test.up16 run-test.up32

run-test.old:test.c common.c ../m-dict.h
	$(CC) -O3 -Wall -DNDEBUG -g -I. -I.. $< -o $@

run-test.new:test.c common.c ../m-dict.h
	$(CC) -O3 -Wall -DNDEBUG -g -I. -I.. -DBULK_MODE=1 $< -o $@

run-test.up16:test.c common.c ../m-dict.h
	$(CC) -O3 -Wall -DNDEBUG -g -I. -I.. -DBULK_MODE=2 -DMAX_PREFETCH=16 $< -o $@

run-test.up8:test.c common.c ../m-dict.h
	$(CC) -O3 -Wall -DNDEBUG -g -I. -I.. -DBULK_MODE=2 -DMAX_PREFETCH=8 $< -o $@

run-test.up32:test.c common.c ../m-dict.h
	$(CC) -O3 -Wall -DNDEBUG -g -I. -I.. -DBULK_MODE=2 -DMAX_PREFETCH=32 $< -o $@

clean:
	rm -fr run-test.old run-test.new run-test.up *~

