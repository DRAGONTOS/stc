NAME = stc
PREFIX = /usr

all:
	$(MAKE) clear
	$(MAKE) release

release:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Debug -S . -B ./build -G Ninja
	cmake --build ./build
	chmod -R 777 ./build

clear:
	rm -rf build

install.core:
	@if [ ! -f ./build/stc ]; then echo -en "You need to run $(MAKE) all first.\n" && exit 1; fi
	@echo -en "!NOTE: Please note make install does not compile stc and only installs the already built files."
	mkdir -p ${PREFIX}/bin
	cp -f ./build/stc ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/stc

install: install.core

uninstall:
