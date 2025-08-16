EXE_NAME=softcover$(EXE_SUFFIX)
EXE_SRC_DIR=src/platforms/$(PLATFORM_NAME)/
EXE_PATH=$(BUILD_DIR)$(EXE_NAME)

LIB_NAME=app$(LIB_SUFFIX)
LIB_SRC_DIR=src/app/
LIB_PATH=$(BUILD_DIR)$(LIB_NAME)

COMMON_INCLUDES=-Isrc/common
EXE_INCLUDES=
LIB_INCLUDES=
FLAGS_DEFAULT=-DLIB_NAME=\"./$(LIB_NAME)\"
FLAGS_STRICT=$(FLAGS_DEFAULT) -std=c99 -Wall -pedantic -Wextra
FLAGS_DEBUG=$(FLAGS_STRICT) -g -o0
BUILD_DIR=build/
RUN_CMD=$(EXE_PATH) $(LIB_PATH) $(ARGS)

EXE_SUFFIX=.o
LIB_SUFFIX=.so
PLATFORM_NAME=test
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
	gcc -o $(EXE_PATH) $(EXE_SRC_DIR)*.c $(FLAGS) $(COMMON_INCLUDES) $(EXE_INCLUDES)
	touch $(EXE_PATH)

$(LIB_PATH): $(LIB_SRC_DIR)*.c
	mkdir -p $(BUILD_DIR)
	gcc -shared -o $(LIB_PATH) -fPIC $(LIB_SRC_DIR)*.c $(FLAGS) $(COMMON_INCLUDES) $(LIB_INCLUDES) 
	touch $(LIB_PATH)
                                     
.PHONY: strict
strict:
	$(FLAGS)=$(FLAGS_STRICT)
	$(MAKE) default
                    
.PHONY: debug
debug:
	$(FLAGS)=$(FLAGS_DEBUG)
	$(MAKE) default

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
	gdb $(RUN_CMD)

.PHONY: valgrind
valgrind:
	valgrind -s --leak-check=yes --track-origins=yes $(RUN_CMD)
