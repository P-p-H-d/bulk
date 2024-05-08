all:run-test.old run-test.new

run-test.old:test.c common.c ../m-dict.h
	$(CC) -O3 -Wall -DNDEBUG -g -I. -I.. $< -o $@

run-test.new:test.c common.c ../m-dict.h
	$(CC) -O3 -Wall -DNDEBUG -g -I. -I.. -DBULK_MODE $< -o $@

clean:
	rm -fr run-test.old run-test.new *~

