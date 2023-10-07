CC=gcc
CFLAGS=-I./include
EXENAME=shell


default:
	$(CC) -o $(EXENAME) src/*.c $(CFLAGS)

clean:
	rm $(EXENAME)
