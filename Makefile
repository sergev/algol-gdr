#
# make          -- build everything
# make test     -- run unit tests
# make install  -- install binaries to /usr/local
# make clean    -- remove build files
# make goldens  -- regenerate expected outputs in tests/algol (needs dubna)
#
# To reconfigure for Debug build:
#   make clean; make debug; make
#

all:    build
	$(MAKE) -Cbuild $@

test:   build
	$(MAKE) -Cbuild unit_tests
	ctest --test-dir build/tests

goldens:
	scripts/make-goldens

install: build
	$(MAKE) -Cbuild $@

clean:
	rm -rf build

build:
	mkdir $@
	cmake -B$@ -DCMAKE_BUILD_TYPE=RelWithDebInfo

debug:
	mkdir build
	cmake -Bbuild -DCMAKE_BUILD_TYPE=Debug

.PHONY: all test install clean debug goldens
