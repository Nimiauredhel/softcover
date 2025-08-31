EXE_NAME=softcover$(EXE_SUFFIX)
EXE_SOURCES=src/platforms/$(PLATFORM_NAME)/*.c
EXE_PATH=$(BUILD_DIR)$(EXE_NAME)

LIB_NAME=app$(LIB_SUFFIX)
LIB_SOURCES=src/app/*.c
LIB_ASSETS=assets/*
LIB_PATH=$(BUILD_DIR)$(LIB_NAME)

COMMON_SOURCES=src/common/*.c
COMMON_INCLUDES=-Isrc/common
EXE_INCLUDES=-lncurses -lportaudio
LIB_INCLUDES=-lm
FLAGS_DEFAULT= -std=c99 -Wall -pedantic -Wextra
FLAGS_DEBUG= -g -o0
DEFINES=-D_GNU_SOURCE -DLIB_NAME=\"./$(LIB_NAME)\"
BUILD_DIR=build/
ASSETS_DIR=assets/
RUN_CMD=./$(EXE_NAME) $(ARGS)

EXE_SUFFIX=.bin
LIB_SUFFIX=.so
PLATFORM_NAME=linux-terminal
FLAGS=$(FLAGS_DEFAULT)
ARGS=

.PHONY: default
default:
	$(MAKE) platform
	$(MAKE) app
	cp -r $(ASSETS_DIR)* $(BUILD_DIR)

.PHONY: platform
platform: $(EXE_PATH)

.PHONY: app
app: $(LIB_PATH)

$(EXE_PATH): $(EXE_SOURCES)
	mkdir -p $(BUILD_DIR)
	rm -f compile_commands.json
	bear -- gcc $(EXE_SOURCES) $(COMMON_SOURCES) $(COMMON_INCLUDES) $(EXE_INCLUDES) $(DEFINES) $(FLAGS) -o $(EXE_PATH)
	mv compile_commands.json $(BUILD_DIR)platform_compile_commands.json
	jq -s add $(BUILD_DIR)*.json > compile_commands.json

$(LIB_PATH): $(LIB_SOURCES) $(LIB_ASSETS)
	mkdir -p $(BUILD_DIR)
	rm -f compile_commands.json
	bear -- gcc $(LIB_SOURCES) $(COMMON_SOURCES) $(COMMON_INCLUDES) $(LIB_INCLUDES) $(DEFINES) $(FLAGS) -fPIC -shared -o $(LIB_PATH)
	mv compile_commands.json $(BUILD_DIR)app_compile_commands.json
	jq -s add $(BUILD_DIR)*.json > compile_commands.json
	touch $(LIB_PATH).mod
                                     
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

.ONESHELL:
.PHONY: run
run:
	cd $(BUILD_DIR)
	$(RUN_CMD)

.PHONY: gdb
gdb:
	cd $(BUILD_DIR)
	gdb --args $(RUN_CMD)

.PHONY: valgrind
valgrind:
	cd $(BUILD_DIR)
	valgrind -s --leak-check=yes --track-origins=yes $(RUN_CMD) 2> valgrind_output
