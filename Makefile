default:
	(cd mserv; ./configure)
	(cd mserv; make DEFS=${DEFS})
	(cd mservcli; ./configure)
	(cd mservcli; make DEFS=${DEFS})
	(cd mservutils; ./configure)
	(cd mservutils; make DEFS=${DEFS})

install:
	(cd mservcli; make install)
	(cd mservutils; make install)
	(cd mserv; make install)

clean:
	(cd mserv; make clean)
	(cd mservcli; make clean)
	(cd mservutils; make clean)

cleaner:
	(cd mserv; make cleaner)
	(cd mservcli; make cleaner)
	(cd mservutils; make cleaner)
