all: collector masterThread

collector: collector.c workerThread.o
	gcc -o collector collector.c workerThread.o -lpthread

masterThread: masterThread.c workerThread.o
	gcc -o farm masterThread.c workerThread.o -lpthread

workerThread.o: workerThread.c
	gcc -c workerThread.c -lpthread

clean:
	rm collector masterThread workerThread.o
