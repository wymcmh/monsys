
CC  := gcc
CXX := g++
AR  := ar


CFLAGS  := -g -Wall
LDFLAGS :=
ARFLAGS := -r

TARGET  := libbase.a

CFILES := zlog.c \
	zcodec.c

CXXFILES := 

OBJFILES := $(CFILES:%.c=%.o) $(CXXFILES:%.cc=%.o)
DEPFILES := $(OBJFILES:%.o=%.d)

$(TARGET) : $(OBJFILES)
	rm -f $@
	$(AR) $(ARFLAGS) $@ $^

%.d : %.c
	rm -f $@
	$(CC) $(CFLAGS) -MM -o $@ -c $<

%.d : %.cc
	rm -f $@
	$(CXX) $(CFLAGS) -MM -o $@ -c $<

%.o : %.c
	rm -f $@
	$(CC) $(CFLAGS) -o $@ -c $<

%.o : %.cc
	rm -f $@
	$(CXX) $(CFLAGS) -o $@ -c $<

include $(DEPFILES)

.PHONY : build clean test

all: $(TARGET)

ts: main.cc $(TARGET)
	g++ -L. -lbase $< -o $@

clean:
	rm -f $(OBJFILES) $(DEPFILES)

test: build
	./$(TARGET)


