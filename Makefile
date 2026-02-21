CXX      = g++
CXXFLAGS = -std=c++20 -O3 -march=native -flto -funroll-loops \
	-Wall -Wextra \
	-I./include

LDFLAGS  = -flto -lpthread

SRC_DIR  = src
OBJ_DIR  = obj
BIN      = server

SRCS     = $(wildcard $(SRC_DIR)/*.cpp)
OBJS     = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

debug: CXXFLAGS = -std=c++20 -O0 -g3 -fsanitize=address,undefined \
	-Wall -Wextra \
	-I./include
debug: LDFLAGS  = -lpthread -fsanitize=address,undefined
debug: $(BIN)

clean:
	rm -rf $(OBJ_DIR) $(BIN)

.PHONY: all debug clean
