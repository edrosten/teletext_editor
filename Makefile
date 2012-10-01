CXXFLAGS=-std=c++0x -D_GLIBCXX_DEBUG -g -ggdb -Wall -Wextra -Wno-missing-field-initializers
LDFLAGS=-lcvd_debug -lfltk -lfltk_gl

readfont: readfont.o
	$(CXX) -o $@ $< $(LDFLAGS)
