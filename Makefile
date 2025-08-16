EXE_NAME=softcover$(EXE_SUFFIX)
EXE_SRC_DIR=src/platforms/$(PLATFORM_NAME)/
EXE_PATH=$(BUILD_DIR)$(EXE_NAME)

LIB_NAME=app$(LIB_SUFFIX)
LIB_SRC_DIR=src/app/
LIB_PATH=$(BUILD_DIR)$(LIB_NAME)

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

default:
	$(MAKE) $(EXE_PATH)
	$(MAKE) $(LIB_PATH)

$(EXE_PATH):
	mkdir -p $(BUILD_DIR)
	gcc -o $(EXE_PATH) $(EXE_SRC_DIR)*.c $(FLAGS) $(EXE_INCLUDES)

$(LIB_PATH):
	mkdir -p $(BUILD_DIR)
	gcc -shared -o $(LIB_PATH) -fPIC $(LIB_SRC_DIR)*.c $(FLAGS) $(LIB_INCLUDES) 
                                     
strict:
	$(FLAGS)=$(FLAGS_STRICT)
	$(MAKE) default
                    
debug:
	$(FLAGS)=$(FLAGS_DEBUG)
	$(MAKE) default

clean:
	rm -rf $(BUILD_DIR)

andrun:
	$(MAKE) default
	$(MAKE) run

run:
	$(RUN_CMD)

gdb:
	gdb $(RUN_CMD)

valgrind:
	valgrind -s --leak-check=yes --track-origins=yes $(RUN_CMD)
