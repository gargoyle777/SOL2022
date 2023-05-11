all: collector masterThread generafile test

collector: collector.c workerThread.o
	gcc -o collector collector.c -g -lpthread

masterThread: masterThread.c workerThread.o
	gcc -o farm masterThread.c common.o workerThread.o senderThread.o -g -lpthread

workerThread.o: workerThread.c senderThread.o
	gcc -c workerThread.c common.o -g -lpthread

senderThread.o: senderThread.c
	gcc -c senderThread.c common.o -g -lpthread

common.o: common.c
	gcc -c common.c -g -lpthread

generafile: generafile.c
	gcc -o generafile generafile.c -std=c99

test: test.sh
	chmod 701 test.sh

clean:
	rm collector masterThread workerThread.o
