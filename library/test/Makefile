PROGRAM = bin/linux/test
OBJPATH = obj/linux
LIBS = -lpmath -lrt
CFLAGS = -s -O3 -I ../library/include
CC = gcc
RM = rm -fr

all: $(PROGRAM)

$(PROGRAM): $(OBJPATH)/main.o
	$(CC) $(LIBS) $(OBJPATH)/main.o -o $(PROGRAM) 

$(OBJPATH)/main.o: main.c
	$(CC) $(CFLAGS) -c -o $(OBJPATH)/main.o main.c

clean:
	$(RM) $(OBJPATH)/*.o $(PROGRAM)

run: $(PROGRAM)
	./$(PROGRAM)