CC=clang
CFLAGS=-Wall -g

BINS=notjustcats
OUTPUTDIR=./recovered_files

all: $(BINS)

notjustcats: notjustcats.c
	$(CC) $(CFLAGS) -o notjustcats notjustcats.c

run: notjustcats.c
	./notjustcats random.img $(OUTPUTDIR)

clean:
	rm $(BINS)
	rm -r $(OUTPUTDIR)
