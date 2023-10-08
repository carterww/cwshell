CC=gcc
CFLAGS=-I./include
EXENAME=shell

OBJS=$(obj/shell.o obj/history.o obj/io.o obj/job.o)

default: obj obj/shell.o obj/history.o obj/io.o obj/job.o
	$(CC) -o $(EXENAME) obj/shell.o obj/history.o obj/io.o obj/job.o $(CFLAGS)

obj/%.o: src/%.c
	$(CC) -c $< -o $@ $(CFLAGS)

obj:
	mkdir obj

clean:
	rm -rf obj $(EXENAME)
