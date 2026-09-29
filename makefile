CXX    := g++
CXXFLAGS := -std=c++17

# ---- Targets ----
TRACKER_TARGET := tracker_app
CLIENT_TARGET := client_app

# ---- Sources ----
TRACKER_SRCS := tracker/main.cpp tracker/utils.cpp tracker/data_structure.cpp tracker/performCommand.cpp tracker/commandUtils.cpp tracker/sync.cpp
CLIENT_SRCS := client/client.cpp

# ---- Objects ----
TRACKER_OBJS := $(TRACKER_SRCS:.cpp=.o)
CLIENT_OBJS := $(CLIENT_SRCS:.cpp=.o)

# ---- Build rules ----
.PHONY: all clean run

all: $(TRACKER_TARGET) $(CLIENT_TARGET)

$(TRACKER_TARGET): $(TRACKER_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(CLIENT_TARGET): $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ -lssl -lcrypto   # <-- added OpenSSL linking

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run-tracker: $(TRACKER_TARGET)
	./$(TRACKER_TARGET)

run-client: $(CLIENT_TARGET)
	./$(CLIENT_TARGET)

clean:
	rm -f $(TRACKER_TARGET) $(CLIENT_TARGET) $(TRACKER_OBJS) $(CLIENT_OBJS)
