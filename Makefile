enet/Makefile:
	cd enet; ./configure --enable-shared=no --enable-static=yes

libenet: enet/Makefile
	-$(MAKE) -C enet all

mingw:
	cd src\\lua && $(MAKE) mingw
	$(MAKE) -f Makefile.mingw

linux: libenet
	cd src/lua && $(MAKE) linux
	$(MAKE) -f Makefile.linux

linux-clean:
	$(MAKE) -f Makefile.linux clean

mingw-clean:
	$(MAKE) -f Makefile.mingw clean

libenet-clean: enet/Makefile
	$(MAKE) -C enet clean

liblua-clean:
	cd src/lua && $(MAKE) clean

clean: libenet-clean liblua-clean linux-clean
