all: collector masterThread

collector: collector.c workerThread.o
    gcc -o collector collector.c workerThread.o -lpthread

masterThread: masterThread.c workerThread.o
    gcc -o masterThread masterThread.c workerThread.o -lpthread

workerThread.o: workerThread.c
    gcc -c workerThread.c

clean:
    rm collector masterThread workerThread.o
