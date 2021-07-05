.DEFAULT_GOAL := sequential
CC=gcc
PORT = 9090
threaded = TRUE
n_threads = 5

sequential: 
	${CC} -Wall -pthread server.c utils.c -o rheagal-core.out 
	./rheagal-core.out ${PORT} ${threaded} 
clean:
	@echo "Cleaning up..."
	rm *.out
test: 
	python3 sample-client.py localhost ${PORT} -n ${n_threads}