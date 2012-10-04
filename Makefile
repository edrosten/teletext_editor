CXX=g++-4.8-svn
CXXFLAGS=-std=c++0x -D_GLIBCXX_DEBUG -g -ggdb -Wall -Wextra -Wno-missing-field-initializers
LDFLAGS=-lcvd_debug -lfltk -lfltk_gl

all:bitmaps readfont


.PHONY: bitmaps

XBMS=$(shell ls resources/*.xbm)
PNGS=$(subst xbm,png,$(XBMS))


bitmaps:$(PNGS)

readfont: readfont.o 
	$(CXX) -o $@ $< $(LDFLAGS)

resources/%.png:resources/%.xbm
	convert $< $@
