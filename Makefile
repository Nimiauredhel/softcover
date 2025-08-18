EXE_NAME=softcover$(EXE_SUFFIX)
EXE_SRC_DIR=src/platforms/$(PLATFORM_NAME)/
EXE_PATH=$(BUILD_DIR)$(EXE_NAME)

LIB_NAME=app$(LIB_SUFFIX)
LIB_SRC_DIR=src/app/
LIB_PATH=$(BUILD_DIR)$(LIB_NAME)

COMMON_INCLUDES=-Isrc/common
EXE_INCLUDES=-lncurses -lportaudio -lm
LIB_INCLUDES=
FLAGS_DEFAULT= -std=c99
FLAGS_STRICT= -Wall -pedantic -Wextra
FLAGS_DEBUG= -ggdb -o0
DEFINES=-D_GNU_SOURCE -DLIB_NAME=\"./$(LIB_NAME)\"
BUILD_DIR=build/
RUN_CMD=$(EXE_PATH) $(LIB_PATH) $(ARGS)

EXE_SUFFIX=.o
LIB_SUFFIX=.so
PLATFORM_NAME=ncurses
FLAGS=$(FLAGS_DEFAULT)
ARGS=

.PHONY: default
default:
	$(MAKE) platform
	$(MAKE) app

.PHONY: platform
platform: $(EXE_PATH)

.PHONY: app
app: $(LIB_PATH)

$(EXE_PATH): $(EXE_SRC_DIR)*.c
	mkdir -p $(BUILD_DIR)
	rm -f compile_commands.json
	bear -- gcc $(EXE_SRC_DIR)*.c $(COMMON_INCLUDES) $(EXE_INCLUDES) $(DEFINES) $(FLAGS) -o $(EXE_PATH)
	mv compile_commands.json $(BUILD_DIR)platform_compile_commands.json
	jq -s add $(BUILD_DIR)*.json > compile_commands.json

$(LIB_PATH): $(LIB_SRC_DIR)*.c
	mkdir -p $(BUILD_DIR)
	rm -f compile_commands.json
	bear -- gcc $(LIB_SRC_DIR)*.c $(COMMON_INCLUDES) $(LIB_INCLUDES) $(DEFINES) -fPIC $(FLAGS) -shared -o $(LIB_PATH)
	mv compile_commands.json $(BUILD_DIR)app_compile_commands.json
	jq -s add $(BUILD_DIR)*.json > compile_commands.json
                                     
.PHONY: strict
strict:
	$(MAKE) default FLAGS+=$(FLAGS_STRICT)
                    
.PHONY: debug
debug:
	$(MAKE) default FLAGS+=$(FLAGS_DEBUG)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: andrun
andrun:
	$(MAKE) default
	$(MAKE) run

.PHONY: run
run:
	$(RUN_CMD)

.PHONY: gdb
gdb:
	gdb --args $(RUN_CMD)

.PHONY: valgrind
valgrind:
	valgrind -s --leak-check=yes --track-origins=yes $(RUN_CMD)
