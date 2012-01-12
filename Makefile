
# The following flags are for Linux.  
# These are primarily for debugging and optimization
# No flags are really necessary.

CC = gcc
CFLAGS = -O6 $(GCCFLAGS) -finline-functions -fomit-frame-pointer -ffast-math -fstrict-aliasing -Wall
LDFLAGS = -lm

# Debugging
#
#CFLAGS = -g -finline-functions -Wall
#LDFLAGS = -g -lm

# Profiling
#
# CFLAGS = -pg -O6 $(GCCFLAGS) -finline-functions -ffast-math -fstrict-aliasing -Wall
# LDFLAGS = -pg 

all: llncky lncky ncky cky count-local-trees tree-preterms # ckyp count-trees count-parses

f22.pts.yld:
	zcat ~/research/Penn2/f22.txt.gz | tree-preterms > f22.pts.yld

f22.prs: cky count-local-trees f22.pts.yld
	zcat ~/research/Penn2/f2-21.txt.gz | count-local-trees | cky f22.pts.yld 100 > f22.prs

tmp.prs: ncky tmp.yld
	time cky tmp.yld 100 f2-21.lt > tmp.prs

edge-counts.out: ncky ../f2-21.lt ../f22.yld
	ncky ../f22.yld 0 ../f2-21.lt | tee edge-counts.out


clean:
	rm -f *.o *.tcov *.d *.out core count-parses ncky f2-21.txt.gz f2-21.lt f22.txt f22.pts.yld


llncky: llncky.o hash-string.o mmm.o tree.o ledge.o llgrammar.o vindex.o #-lm

lncky: lncky.o hash-string.o mmm.o tree.o ledge.o lgrammar.o vindex.o #-lm

ncky: ncky.o hash-string.o mmm.o tree.o ledge.o grammar.o vindex.o #-lm

cky: cky.o hash-string.o mmm.o tree.o ledge.o grammar.o vindex.o #-lm

ckyp: ckyp.o hash-string.o mmm.o tree.o ledge.o grammar.o -lm

count-pblts: count-pblts.o mmm.o hash-string.o vindex.o tree.o

count-tdblts: count-tdblts.o mmm.o hash-string.o vindex.o tree.o

count-local-trees: count-local-trees.o mmm.o hash-string.o vindex.o tree.o

count-trees: count-trees.o mmm.o hash-string.o vindex.o tree.o

count-blts: count-blts.o mmm.o hash-string.o vindex.o tree.o

count-parses: count-parses.o hash-string.o mmm.o tree.o ledge.o grammar.o vindex.o -lm

tree-preterms: tree-preterms.o mmm.o hash-string.o tree.o

f2-21.txt.gz:
	ln -s /home/mj/research/Penn2/f2-21.txt.gz .

f22.txt:
	zcat /home/mj/research/Penn2/f22.txt.gz > f22.txt

f2-21.lt: count-local-trees f2-21.txt.gz
	zcat f2-21.txt.gz | count-local-trees > f2-21.lt

f2-21.llt: count-trees f2-21.txt.gz
	zcat f2-21.txt.gz | count-trees > f2-21.llt

f2-21.blt: count-blts f2-21.txt
	count-blts < f2-21.txt > f2-21.blt

f2-21.tlt: count-tdblts f2-21.txt
	count-tdblts < f2-21.txt > f2-21.tlt

f2-21.pblt: count-pblts f2-21.txt
	count-pblts < f2-21.txt > f2-21.pblt


cky.o: cky.c local-trees.h hash-string.h hash-templates.h hash.h mmm.h \
	tree.h vindex.h ledge.h grammar.h vindex.h

lncky.o: ncky.c ncky-helper.c local-trees.h hash-string.h hash-templates.h \
	hash.h mmm.h tree.h vindex.h ledge.h lgrammar.h vindex.h

ncky.o: ncky.c ncky-helper.c local-trees.h hash-string.h hash-templates.h \
	hash.h mmm.h tree.h vindex.h ledge.h grammar.h vindex.h

blockalloc.o: blockalloc.c mmm.h

ckyp.o: ckyp.c local-trees.h hash-string.h hash-templates.h hash.h mmm.h \
	tree.h vindex.h ledge.h grammar.h

count-tdlts.o: count-tdblts.c local-trees.h mmm.h hash-string.h hash.h \
	vindex.h tree.h

count-pblts.o: count-pblts.c local-trees.h mmm.h hash-string.h hash.h \
	vindex.h tree.h

count-blts.o: count-blts.c local-trees.h mmm.h hash-string.h hash.h \
	vindex.h tree.h

count-local-trees.o: count-local-trees.c local-trees.h mmm.h \
	hash-string.h hash.h vindex.h tree.h

grammar.o: grammar.c local-trees.h hash-string.h hash.h hash-templates.h \
	 grammar.h mmm.h

lgrammar.o: lgrammar.c local-trees.h hash-string.h hash.h hash-templates.h \
	 lgrammar.h mmm.h

llgrammar.o: llgrammar.c local-trees.h hash-string.h hash.h hash-templates.h \
	 lgrammar.h mmm.h

tree.o: tree.c tree.h hash-string.h hash.h mmm.h local-trees.h

vindex.o: vindex.c vindex.h mmm.h hash.h hash-templates.h

hash-string.o : hash.c hash-string.h hash.h hash-templates.h mmm.h

mmm.o : mmm.c mmm.h 

