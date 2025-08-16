EXE_NAME=softcover$(EXE_SUFFIX)
EXE_SRC_DIR=src/platforms/$(PLATFORM_NAME)/
EXE_INCLUDES=
LIB_NAME=app$(LIB_SUFFIX)
LIB_SRC_DIR=src/app/
LIB_INCLUDES=
BUILD_DIR=build/
FLAGS_DEFAULT=
FLAGS_STRICT=$(FLAGS_DEFAULT) -std=c99 -Wall -pedantic -Wextra
FLAGS_DEBUG=$(FLAGS_STRICT) -g -o0
RUN_CMD=./$(EXE_NAME) $(LIB_NAME) $(ARGS)

EXE_SUFFIX=.o
LIB_SUFFIX=.so
PLATFORM_NAME=test
FLAGS=$(FLAGS_DEFAULT)
ARGS=

default:
	$(MAKE) $(BUILD_DIR)$(EXE_NAME)
	$(MAKE) $(BUILD_DIR)$(LIB_NAME)

$(BUILD_DIR)$(EXE_NAME):
	mkdir -p $(BUILD_DIR)
	gcc -o $(BUILD_DIR)$(EXE_NAME) $(EXE_SRC_DIR)*.c $(FLAGS) $(EXE_INCLUDES)

$(BUILD_DIR)$(LIB_NAME):
	mkdir -p $(BUILD_DIR)
	gcc -shared -o $(BUILD_DIR)$(LIB_NAME) -fPIC $(LIB_SRC_DIR)*.c $(FLAGS) $(LIB_INCLUDES) 
                                     
strict:
	$(FLAGS)=$(FLAGS_STRICT)
	$(MAKE) default
                    
debug:
	$(FLAGS)=$(FLAGS_DEBUG)
	$(MAKE) default

clean:
	rm -rf $(BUILD_DIR)

.ONESHELL:
andrun:
	$(MAKE) default
	cd $(BUILD_DIR)
	$(RUN_CMD)

run:
	cd $(BUILD_DIR)
	$(RUN_CMD)
gdb:
	cd $(BUILD_DIR)
	gdb $(RUN_CMD)

valgrind:
	cd $(BUILD_DIR)
	valgrind -s --leak-check=yes --track-origins=yes $(RUN_CMD)
