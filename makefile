CFLAGS = -I include/ -O2
TEST_FLAGS = -I include/ -g
MEM_CHECK_FLAG = -include mem_check/mem_check.h -Wno-implicit-function-declaration

TEST_C = src/**/*.c mem_check/*.c
MAIN_C = src/*.c src/**/*.c mem_check/*.c

debug: CFLAGS = ${TEST_FLAGS}
debug: all

mem_check: CFLAGS = ${TEST_FLAGS} ${MEM_CHECK_FLAG}
mem_check: MAIN_C =  ${MAIN_C}
mem_check: all

all: lreng tests/bigint.out tests/number.out

tests/bigint.out: ${TEST_C} tests/bigint.c
	gcc ${CFLAGS} -o $@ ${TEST_C} tests/bigint.c

tests/number.out: ${TEST_C} tests/number.c
	gcc ${CFLAGS} -o $@ ${TEST_C} tests/number.c

lreng: ${MAIN_C}
	gcc ${CFLAGS} -o $@ $^

clean:
	rm lreng tests/bigint.out tests/number.out
