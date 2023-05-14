all: collector masterThread generafile test

collector: collector.c
	gcc -Wall -Werror -o collector collector.c -g -lpthread

masterThread: masterThread.c workerThread.o senderThread.o common.o
	gcc -Wall -Werror -o farm masterThread.c common.o workerThread.o senderThread.o -g -lpthread -lglob

workerThread.o: workerThread.c
	gcc -Wall -Werror -c workerThread.c -g -lpthread

senderThread.o: senderThread.c
	gcc -Wall -Werror -c senderThread.c -g -lpthread

common.o: common.c
	gcc -Wall -Werror -c common.c -g -lpthread

generafile: generafile.c
	gcc -Wall -Werror -o generafile generafile.c -std=c99

test: test.sh
	chmod 701 test.sh

clean:
	rm collector masterThread workerThread.o
