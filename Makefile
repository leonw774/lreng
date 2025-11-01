CFLAGS = -I include/ -Wall -Wextra
DEFAULT_FLAGS = -O3
PROFILE_FLAGS = -pg -O3 -D IS_PROFILE -D PROFILE_REPEAT_NUM=100 \
	-fno-pie -no-pie
DEBUG_FLAGS = -g -D ENABLE_DEBUG_LOG \
	# -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer
DEBUG_MORE_FLAGS = -D ENABLE_DEBUG_LOG_MORE \
	-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer
MEMCHECK_FLAGS = -include memcheck/memcheck.h \
	-D ENABLE_DEBUG_LOG -D ENABLE_DEBUG_LOG_MORE \
	-Wno-implicit-function-declaration -Wno-unused-function -Wno-unused-variable
TEST_DIR = tests

SHARED_SRC = src/**/*.c
TEST_SRC = $(wildcard $(TEST_DIR)/*.c)
MAIN_SRC = src/*.c

TEST_TARGET = $(patsubst %.c, %.out, $(TEST_SRC))
MAIN_TARGET = lreng

.PHONY: lreng debug memcheck clean

release: CFLAGS += $(DEFAULT_FLAGS)
release: $(MAIN_TARGET)

profile: CFLAGS += $(PROFILE_FLAGS)
profile: $(MAIN_TARGET)

debug: CFLAGS += $(DEBUG_FLAGS)
debug: $(MAIN_TARGET) $(TEST_TARGET)

debug_more: CFLAGS += $(DEBUG_FLAGS) ${DEBUG_MORE_FLAGS}
debug_more: $(MAIN_TARGET) $(TEST_TARGET)

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
# Web Playground
# ================================

WEB_TARGET = webplayground/lreng.js

web:
	rm -r webplayground/lreng.* || true
	emcc $(SHARED_SRC) $(MAIN_SRC) \
		-I include/ -D IS_WASM -O3 -sSTACK_SIZE=5MB -o $(WEB_TARGET) \
		-s "EXPORTED_RUNTIME_METHODS=['FS','callMain']"
	cp README.md webplayground/README.md

clean_web:
	rm -r webplayground/lreng.* || true
