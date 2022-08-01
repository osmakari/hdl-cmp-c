CFLAGS = -g -lm

build: src/*.c src/*.h
	mkdir -p ./bin
	gcc src/*.c $(CFLAGS) -o bin/hdl-cmp

install: build
	@echo "Installing hdl-cmp..."
	cp ./bin/hdl-cmp /usr/bin/hdl-cmp