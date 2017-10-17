.PHONY:all
all:
	cd build/linux; \
	cmake -DSVNVERSION=$(bubi_version) -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_VERBOSE_MAKEFILE=ON ../../src; \
	make -j4

.PHONY:clean_all clean clean_build clean_3rd
clean_all:clean clean_build clean_3rd

clean:
	if [ -d ./lib ]; then rm -rf bin/* && cd lib/ && ls | grep -v http |grep -v ed25519 | xargs rm -rf; fi

clean_3rd:
	cd src/3rd && make clean_3rd && cd ../../

clean_build:
	rm -rf build/linux/*

.PHONY:install uninstall
install:
	cd build/linux && make install && make soft_link -f MakeSupplement

uninstall:
	cd build/linux && make uninstall -f MakeSupplement

