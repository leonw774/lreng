CFLAGS = -O2
MAIN_DEPS = *.c
UTIL_DEPS = utils/*.c

all: lreng bigint_test

debug: CFLAGS = -g
debug: all

bigint_test: ${UTIL_DEPS} test_program/bigint_test.c
	gcc -g -o bigint_test $^

lreng: ${UTIL_DEPS} ${MAIN_DEPS} lreng.c
	gcc ${CFLAGS} -o $@ $^;

clean:
	rm lreng bigint_test
