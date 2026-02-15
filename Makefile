run: build
	./bin/morte

build:
	mkdir -p bin
	gcc -o ./bin/morte cmorte/morte.c -std=c99 -I./include -L./lib -lraylib -lm
