CFLAGS = -O2
MAIN_DEPS = *.c
UTIL_DEPS = objects/*.c utils/*.c

all: lreng test

test: bigint_test number_test

debug: CFLAGS = -g
debug: all

bigint_test: ${UTIL_DEPS} test_program/bigint_test.c
	gcc -g -o bigint_test $^

number_test: ${UTIL_DEPS} test_program/number_test.c
	gcc -g -o number_test $^

lreng: ${UTIL_DEPS} ${MAIN_DEPS} lreng.c
	gcc ${CFLAGS} -o $@ $^;

clean:
	rm lreng bigint_test
