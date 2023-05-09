all: collector masterThread generafile test

collector: collector.c workerThread.o
	gcc -o collector collector.c workerThread.o -g -lpthread

masterThread: masterThread.c workerThread.o
	gcc -o farm masterThread.c workerThread.o -g -lpthread

senderThread.o: senderThread.c
	gcc -c senderThread.c -g -lpthread

workerThread.o: workerThread.c
	gcc -c workerThread.c -g -lpthread

generafile: generafile.c
	gcc -o generafile generafile.c -std=c99

test: test.sh
	chmod 701 test.sh

clean:
	rm collector masterThread workerThread.o
