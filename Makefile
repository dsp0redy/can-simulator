.PHONY: all clean build rebuild

all:

clean:
	rm -rf ./build

build:
	$(MAKE) -C ./app/ build-app

rebuild: clean build
	
