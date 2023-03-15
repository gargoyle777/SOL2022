all: collector masterThread generafile test

collector: collector.c workerThread.o
	gcc -o collector collector.c workerThread.o -lpthread

masterThread: masterThread.c workerThread.o
	gcc -o farm masterThread.c workerThread.o -lpthread

workerThread.o: workerThread.c
	gcc -c workerThread.c -lpthread

generafile: generafile.c
	gcc -o geenrafile generafile.c

test: test.sh
	chmod 701 test.sh

clean:
	rm collector masterThread workerThread.o
