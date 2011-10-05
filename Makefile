BIN := gig2sfz
SRCS := main.cpp

-include Makefile.local

# =======

default: $(BIN)

$(BIN): $(SRCS)
	$(CXX) $^ -o $@ $(CFLAGS) -lgig

.PHONY: test
test: $(BIN)
	$(BIN) wurlitzer_ep200.gig
	vim "Wurlitzer EP200.sfz"

