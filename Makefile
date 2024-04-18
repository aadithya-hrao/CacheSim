.PHONY: all test debug run clean
all: compile run
test: debug run
compile:
	gcc -fopenmp -g -o cache_sim cache_sim_omp.c
debug:
	gcc -fopenmp -g -DDEBUG -o cache_sim cache_sim_omp.c
run:
	./cache_sim
clean:
	rm -f cache_sim
