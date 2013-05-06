CC = gcc -std=c99 -Wall -m64 -fopenmp -O3 -DNDEBUG
#CC = gcc -std=c99 -Wall -m64 -g
RM = rm -f
CP = cp

VERSION = $(shell svnversion)

ALL = compute convert filter fourier gradient kernel measure reserve transfer

all : $(ALL)

#-------------------------------------------------------------------------------

compute: compute.o img.o err.o
	$(CC) -o $@ $^ -lm

convert: convert.o img.o err.o
	$(CC) -o $@ $^ -ltiff -lm

filter: filter.o img.o err.o
	$(CC) -o $@ $^ -lm

fourier: fourier.o img.o err.o fft.o
	$(CC) -o $@ $^ -lm

gradient: gradient.o img.o err.o
	$(CC) -o $@ $^ -ltiff -lm

kernel: kernel.o img.o err.o
	$(CC) -o $@ $^ -lm

measure: measure.o img.o err.o
	$(CC) -o $@ $^ -lm

reserve: reserve.o img.o err.o
	$(CC) -o $@ $^ -lm

transfer: transfer.o img.o err.o
	$(CC) -o $@ $^ -lm

#-------------------------------------------------------------------------------

%.o: %.c Makefile
	$(CC) -c $<

clean:
	$(RM) $(ALL) *.o

dist:
	mkdir                gigo-$(VERSION)
	mkdir                gigo-$(VERSION)/etc

	$(CP) README.md      gigo-$(VERSION)
	$(CP) Makefile       gigo-$(VERSION)
	$(CP) compute.c      gigo-$(VERSION)
	$(CP) convert.c      gigo-$(VERSION)
	$(CP) err.c          gigo-$(VERSION)
	$(CP) err.h          gigo-$(VERSION)
	$(CP) etc.h          gigo-$(VERSION)
	$(CP) fft.c          gigo-$(VERSION)
	$(CP) fft.h          gigo-$(VERSION)
	$(CP) filter.c       gigo-$(VERSION)
	$(CP) fourier.c      gigo-$(VERSION)
	$(CP) icc.h          gigo-$(VERSION)
	$(CP) img.c          gigo-$(VERSION)
	$(CP) img.h          gigo-$(VERSION)
	$(CP) kernel.c       gigo-$(VERSION)
	$(CP) measure.c       gigo-$(VERSION)
	$(CP) reserve.c      gigo-$(VERSION)
	$(CP) transfer.c     gigo-$(VERSION)
	$(CP) etc/fft12.png  gigo-$(VERSION)/etc
	$(CP) etc/fft12s.png gigo-$(VERSION)/etc
	$(CP) etc/fft13.png  gigo-$(VERSION)/etc
	$(CP) etc/fft14.png  gigo-$(VERSION)/etc
	$(CP) etc/fft16.png  gigo-$(VERSION)/etc
	$(CP) etc/COPYING    gigo-$(VERSION)/etc

	zip -r gigo-$(VERSION) gigo-$(VERSION)

#-------------------------------------------------------------------------------

fft.o : fft.c fft.h etc.h
img.o : img.c img.h etc.h
err.o : err.c err.h
