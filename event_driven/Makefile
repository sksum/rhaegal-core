.DEFAULT_GOAL := sequential
CC=gcc
PORT = 9090
threaded = TRUE
n_threads = 5
n_threads_testing = 10

sequential: 
	${CC} -Wall -pthread server.c utils.c thpool.c -o rheagal-core.out 
	./rheagal-core.out ${PORT} ${threaded} ${n_threads}
clean:
	@echo "Cleaning up..."
	rm *.out
test: 
	python3 sample-client.py localhost ${PORT} -n ${n_threads_testing}