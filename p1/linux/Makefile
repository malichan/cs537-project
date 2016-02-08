fastsort: fastsort.o
	gcc -o fastsort fastsort.o -lm

fastsort.o: fastsort.c
	gcc -O -Wall -c fastsort.c

clean:
	rm -f fastsort.o fastsort

test:
	~cs537-1/ta/tests/1a/runtests -c
	rm -f *.log

contest:
	~cs537-1/ta/tests/1a-contest/runtests -c
	rm -f *.log
