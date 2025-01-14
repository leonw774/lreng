CFLAGS = -I include/
DEFAULT_FLAGS = -O2
PROFILE_FLAGS = -fno-pie -no-pie -pg -O1 -D IS_PROFILE
DEBUG_FLAGS = -g -D ENABLE_DEBUG
MEMCHECK_FLAGS = -include memcheck/memcheck.h -Wno-implicit-function-declaration

TEST_DIR = tests

SHARED_SRC = src/**/*.c
TEST_SRC = $(wildcard $(TEST_DIR)/*.c) 
MAIN_SRC = src/*.c

TEST_TARGET = $(patsubst %.c, %.out, $(TEST_SRC))
MAIN_TARGET = lreng

.PHONY: lreng debug memcheck clean

main: CFLAGS += $(DEFAULT_FLAGS)
main: $(MAIN_TARGET)

profile: CFLAGS += $(PROFILE_FLAGS)
profile: $(MAIN_TARGET)

debug: CFLAGS += $(DEBUG_FLAGS)
debug: $(MAIN_TARGET) $(TEST_TARGET)

memcheck: CFLAGS += $(DEBUG_FLAGS) $(MEMCHECK_FLAGS)
memcheck: $(MAIN_TARGET) $(TEST_TARGET)

clean:
	rm $(MAIN_TARGET) $(TEST_TARGET) || true

# The rule for main target (./lreng)
$(MAIN_TARGET): $(MAIN_SRC) $(SHARED_SRC)
	gcc $(CFLAGS) -o $@ $^

# The rules of test targets
%.out: %.c $(SHARED_SRC)
	gcc $(CFLAGS) -o $@ $^

# ================================
# Web Aseembly
# ================================

WASM_TARGET = webplayground/lreng.js

wasm:
	rm -r webplayground/lreng.*
	emcc $(SHARED_SRC) $(MAIN_SRC) -I include/ -D IS_WASM -o $(WASM_TARGET) \
		-s "EXPORTED_RUNTIME_METHODS=['FS','callMain']" 

clean_wasm:
	rm -r webplayground/lreng.* || true

# ================================
# Others
# ================================

merge:
	cat include/dynarr.h include/operators.h include/token.h include/tree.h \
	    include/errormsg.h include/bigint.h include/number.h include/objects.h \
		include/frame.h include/builtin_funcs.h include/lreng.h \
		src/utils/*.c src/objects/*.c src/*.c \
	| sed -e '1s/^/#include<string.h>\n#include<ctype.h>\n#include<getopt.h>\n/' \
		-e '1s/^/#include<stdio.h>\n#include<stdlib.h>\n#include<stdint.h>\n/' \
		-e '/^#include ["<>a-z_.]*/d' > lreng.c

dumpasm:
	objdump -aS ./lreng >> lreng.asm
