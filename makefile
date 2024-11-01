CFLAGS = -I include/ -O2
TEST_FLAGS = -I include/ -g
INCLUDES = include/*.h
TEST_C = src/**/*.c
MAIN_C = src/*.c src/**/*.c
TEST_TARGET = test_program/bigint_test.out test_program/number_test.out

all: lreng test

test: ${TEST_TARGET}

debug: CFLAGS = ${TEST_FLAGS}
debug: all

test_program/bigint_test.out: ${INCLUDES} ${TEST_C} test_program/bigint_test.c
	gcc ${TEST_FLAGS} -o $@ ${TEST_C} test_program/bigint_test.c

test_program/number_test.out: ${INCLUDES} ${TEST_C} test_program/number_test.c
	gcc ${TEST_FLAGS} -o $@ ${TEST_C} test_program/number_test.c

lreng: ${MAIN_C} ${DEPS}
	gcc ${CFLAGS} -o $@ $^

clean:
	rm lreng ${TEST_TARGET}
