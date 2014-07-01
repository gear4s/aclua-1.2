enet/Makefile:
	cd enet; ./configure --enable-shared=no --enable-static=yes

libenet: enet/Makefile
	-$(MAKE) -C enet all

linux: libenet
	cd src/lua && $(MAKE)
	$(MAKE) -f Makefile.linux

linux-clean:
	$(MAKE) -f Makefile.linux clean

libenet-clean: enet/Makefile
	$(MAKE) -C enet clean

liblua-clean:
	cd src/lua && $(MAKE) clean

clean: libenet-clean liblua-clean linux-clean
