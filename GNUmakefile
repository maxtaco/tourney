
all: play tourney tamed

play: play.o
tourney: tourney.o
tamed: tamed.o

TAG = shopt
INCLUDES = -I/usr/local/include/sfslite/$(TAG) -I/usr/local/include
CFLAGS =  -fPIC -g -Wall -rdynamic -shared -DPIC -fno-omit-frame-pointer -O2
CXX = g++
SFSLIB = /usr/local/lib/sfslite/$(TAG)
LFLAGS = -Wl,-E -Wl,--rpath=$(SFSLIB) -L$(SFSLIB) \
        -ltame -lasync -lsfscrypt -larpc \
	-L/usr/local/lib -lgmp -ldl -lgcc
TAME = $(SFSLIB)/tame

.SUFFIXES: .C .T

.T.C:
	$(TAME) -o $@ $<
.C.o:
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<

play: play.o
	$(CXX) -o $@ $< $(LFLAGS)

tourney: tourney.o
	$(CXX) -o $@ $< $(LFLAGS)

tamed : tamed.o
	$(CXX) -o $@ $< $(LFLAGS)

clean:
	rm -f play *.o *.core ssp-*
