CC=gcc
CFLAGS=-Wall -Werror -Wvla -std=gnu11 -fsanitize=address
PFLAGS=-fprofile-arcs -ftest-coverage
DFLAGS=-g
HEADERS=server.h
SRC=server.c functions.c
SRC_TEST=test.c

procchat: $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) $(DFLAGS) $(SRC) -o $@

test:
	$(CC) $(CFLAGS) $(DFLAGS) $(SRC_TEST) -o $@
	./procchat &
	echo "TESTING: The test takes about three minutes"
	./test > test_log.out
	diff expected_log.out test_log.out
	pkill procchat
	echo "11 Testcases success"
	
	
	
clean:
	rm -f procchat
	rm -f test

