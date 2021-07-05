.DEFAULT_GOAL := libuvl
CC=gcc
PORT = 9090
n_threads_testing = 4

libuvl: 
	${CC} -Wall -luv -pthread -O3 -g -DNDEBUG  -lpthread server.c utils.c -o rheagal-core.out 
	./rheagal-core.out ${PORT}
clean:
	@echo "Cleaning up..."
	rm *.out
test: 
	python3 sample-client.py localhost ${PORT} -n ${n_threads_testing}