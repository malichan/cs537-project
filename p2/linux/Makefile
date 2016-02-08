whoosh: whoosh.o
	gcc -o whoosh whoosh.o -lm

whoosh.o: whoosh.c
	gcc -O -Wall -c whoosh.c

clean:
	rm -f whoosh.o whoosh

test:
	~cs537-1/ta/tests/2a/runtests -c
	rm -f *.log