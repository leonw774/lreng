CFLAGS = -I include/ -O2
TEST_FLAGS = -I include/ -g
INCLUDES = include/*.h
TEST_C = src/**/*.c
MAIN_C = src/*.c src/**/*.c
TEST_TARGET = tests/programs/bigint.out tests/programs/number.out

all: lreng test

test: ${TEST_TARGET}

debug: CFLAGS = ${TEST_FLAGS}
debug: all

tests/programs/bigint.out: ${INCLUDES} ${TEST_C} tests/programs/bigint.c
	gcc ${TEST_FLAGS} -o $@ ${TEST_C} tests/programs/bigint.c

tests/programs/number.out: ${INCLUDES} ${TEST_C} tests/programs/number.c
	gcc ${TEST_FLAGS} -o $@ ${TEST_C} tests/programs/number.c

lreng: ${MAIN_C} ${DEPS}
	gcc ${CFLAGS} -o $@ $^

clean:
	rm lreng ${TEST_TARGET}
