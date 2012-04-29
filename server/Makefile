TARGET  := afserver
SRCS    := main.cpp ringb.cpp
OBJS    := ${SRCS:.cpp=.o} 
DEPS    := ${SRCS:.cpp=.dep} 
XDEPS   := $(wildcard ${DEPS}) 
UNAME   := $(shell uname -o)

CC      = g++
CCFLAGS = -O2 -Wall -Werror -ggdb
LIBS    = -lSDLmain -lSDL_image -lSDL -lSDL_gfx -lSDL_ttf -lfreetype -ltiff -lpng -ljpeg -lz -lFLAC 

.PHONY: all clean distclean 

# Use dependencies inside git tree for mingw32 build environments
ifeq ($(UNAME), Msys)
LDFLAGS =  -L../deps/mingw32/
LIBS    := -static -lmingw32 $(LIBS) -mwindows -lm -luser32 -lgdi32 -lwinmm -lws2_32 -lmingw32
CCFLAGS += -I../deps/mingw32/include 
endif

all:: ${TARGET} 

ifneq (${XDEPS},) 
include ${XDEPS} 
endif 

${TARGET}: ${OBJS} 
	${CC} ${LDFLAGS} -o $@ $^ ${LIBS} 

${OBJS}: %.o: %.cpp %.dep 
	${CC} ${CCFLAGS} -o $@ -c $< 

${DEPS}: %.dep: %.cpp Makefile 
	${CC} ${CCFLAGS} -MM $< > $@ 

clean::
	-rm -f *~ *.o *.dep ${TARGET} ${TARGET}.exe

distclean:: clean
