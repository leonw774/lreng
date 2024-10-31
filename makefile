CFLAGS = -O2
MAIN_DEPS = *.c
UTIL_DEPS = objects/*.c utils/*.c

all: lreng test

test: test_program/bigint_test.out test_program/number_test.out

debug: CFLAGS = -g
debug: all

test_program/bigint_test: ${UTIL_DEPS} test_program/bigint_test.c
	gcc -g -o $@ $^

test_program/number_test: ${UTIL_DEPS} test_program/number_test.c
	gcc -g -o $@ $^

lreng: ${UTIL_DEPS} ${MAIN_DEPS} lreng.c
	gcc ${CFLAGS} -o $@ $^;

clean:
	rm lreng bigint_test
