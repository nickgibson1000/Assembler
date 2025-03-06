#
# Makefile for asx20 assembler for vmx20
#

CC = gcc
CFLAGS = -g -Wall -std=c99

YACC = bison

LEX = flex

asx20: scan.o main.o parse.o message.o assemble.o symtab.o
	$(CC) $(CFLAGS) scan.o main.o parse.o message.o assemble.o symtab.o -o asx20

scan.o: y.tab.h defs.h

scan.c:  scan.l
	$(LEX) scan.l
	mv lex.yy.c scan.c

y.tab.h parse.c: parse.y
	$(YACC) -d -y parse.y
	mv y.tab.c parse.c
	$(CC) $(CFLAGS) -c parse.c

main.o: 

parse.o: defs.h 

message.o: 

assemble.o: defs.h symtab.h 

symtab.o: symtab.h

lexdbg: scan.l y.tab.h
	$(LEX) scan.l
	$(CC) -DDEBUG lex.yy.c -lfl -o lexdbg
	rm lex.yy.c

y.output: parse.y
	$(YACC) -v -y parse.y

clean:
	-rm -f *.o parse.c scan.c y.tab.h lexdbg
	-rm -f asx20 y.output

