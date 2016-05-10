.PHONY: all
all : fscheck

fscheck : fscheck.c
	gcc fscheck.c -std=c99 -Wall -Werror -O -o fscheck

.PHONY: clean
clean :
	rm -rf fscheck *.o *.dSYM

.PHONY: test
test :
	~cs537-1/ta/tests/5a/runtests -c
	rm -f *.log