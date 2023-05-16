all: collector masterThread generafile test

collector: collector.c common.o
	gcc -Wall -Werror -o collector collector.c -common.o -g -lpthread

masterThread: masterThread.c workerThread.o senderThread.o common.o
	gcc -Wall -Werror -o farm masterThread.c common.o workerThread.o senderThread.o -g -lpthread

workerThread.o: workerThread.c common.o
	gcc -Wall -Werror -c workerThread.c -common.o -g -lpthread

senderThread.o: senderThread.c common.o
	gcc -Wall -Werror -c senderThread.c -common.o -g -lpthread

common.o: common.c
	gcc -Wall -Werror -c common.c -g -lpthread

generafile: generafile.c
	gcc -Wall -Werror -o generafile generafile.c -std=c99

test: test.sh
	chmod 701 test.sh

clean:
	rm collector masterThread workerThread.o
