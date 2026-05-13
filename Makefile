RAYLIB_TARGET=raylib-5.5
RAYLIB_PLATFORM=linux_amd64
RAYLIB_PACKAGE=$(RAYLIB_TARGET)_$(RAYLIB_PLATFORM)

debug: build
	./bin/morte

build:
	mkdir -p bin
	gcc -Wall -o bin/morte cmorte/morte.c -std=c99 -I./include -L./lib -lraylib -lm

init:
	curl -L https://github.com/raysan5/raylib/releases/download/5.5/$(RAYLIB_PACKAGE).tar.gz -o  $(RAYLIB_PACKAGE).tar.gz
	tar -xzf $(RAYLIB_PACKAGE).tar.gz
	mkdir -p include/raylib/
	cp $(RAYLIB_PACKAGE)/include/* include/raylib/
	mkdir lib/
	cp $(RAYLIB_PACKAGE)/lib/libraylib.a lib/
	rm -r $(RAYLIB_PACKAGE)
	rm -r $(RAYLIB_PACKAGE).tar.gz
