#
# make          -- build everything
# make test     -- run unit tests
# make install  -- install to ~/.local (algc in ~/.local/bin)
# make clean    -- remove build files
#
# To reconfigure for Debug build:
#   make clean; make debug; make
#

all:    build
	$(MAKE) -Cbuild $@

test:   build
	$(MAKE) -Cbuild unit_tests
	ctest --test-dir build/tests

install: all
	cmake --install build --prefix $(HOME)/.local

clean:
	rm -rf build

build:
	mkdir $@
	cmake -B$@ -DCMAKE_BUILD_TYPE=RelWithDebInfo

debug:
	mkdir build
	cmake -Bbuild -DCMAKE_BUILD_TYPE=Debug

.PHONY: all test install clean debug
