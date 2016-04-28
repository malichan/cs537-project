.PHONY: all
all : fscheck

fscheck : fscheck.c
	gcc fscheck.c -Wall -Werror -O -o fscheck

.PHONY: clean
clean :
	rm -f fscheck

.PHONY: test
test :
	~cs537-1/ta/tests/5a/runtests -c
	rm -f *.log