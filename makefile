CFLAGS = -I include/ -O2
TEST_FLAGS = -I include/ -g
MEM_CHECK_FLAGS = -include mem_check/mem_check.h -Wno-implicit-function-declaration

TEST_C = src/**/*.c 
MAIN_C = src/*.c src/**/*.c
MEM_CHECK_C = mem_check/*.c

.PHONY: all debug mem_check
all: lreng tests/bigint.out tests/number.out

debug:
	$(MAKE) all CFLAGS="$(TEST_FLAGS)"

mem_check:
	$(MAKE) all \
		MAIN_C="$(MAIN_C) $(MEM_CHECK_C)" \
		TEST_C="$(TEST_C) $(MEM_CHECK_C)" \
		CFLAGS="$(TEST_FLAGS) $(MEM_CHECK_FLAGS)"

tests/bigint.out: $(TEST_C) tests/bigint.c
	gcc $(CFLAGS) -o $@ $^

tests/number.out: $(TEST_C) tests/number.c
	gcc $(CFLAGS) -o $@ $^

lreng: $(MAIN_C)
	gcc $(CFLAGS) -o $@ $^

clean:
	rm lreng tests/bigint.out tests/number.out || true
