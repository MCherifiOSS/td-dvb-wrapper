obj-m += td-dvb-frontend.o

install:
	install -d $(DESTDIR)/include/hardware/linux/dvb
	cp -a frontend.h $(DESTDIR)/include/hardware/linux/dvb/
	cp -a version.h $(DESTDIR)/include/hardware/linux/dvb/
