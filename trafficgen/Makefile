# Makefile for poisson traffic generator                                                                                             

CC      := gcc
TARGET  := onlineboutique-trafficgen  
SRC     := onlineboutique-trafficgen.c 

CFLAGS  := -O2 -Wall -Wextra -pthread
LDFLAGS := -lm

.PHONY: all debug clean run

all: $(TARGET) nf-trafficgen

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

nf-trafficgen: nf-trafficgen.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

debug: CFLAGS := -O0 -g -Wall -Wextra -pthread
debug: clean $(TARGET)

run: $(TARGET)
	./$(TARGET) localhost 80 / 10 100 20

clean:
	rm -f $(TARGET) nf-trafficgen *~
