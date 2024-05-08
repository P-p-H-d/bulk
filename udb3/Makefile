all:run-test.old run-test.new run-test.up

run-test.old:test.c common.c ../m-dict.h
	$(CC) -O3 -Wall -DNDEBUG -g -I. -I.. $< -o $@

run-test.new:test.c common.c ../m-dict.h
	$(CC) -O3 -Wall -DNDEBUG -g -I. -I.. -DBULK_MODE=1 $< -o $@

run-test.up:test.c common.c ../m-dict.h
	$(CC) -O3 -Wall -DNDEBUG -g -I. -I.. -DBULK_MODE=2 $< -o $@

clean:
	rm -fr run-test.old run-test.new run-test.up *~

