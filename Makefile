
PROG = iocane
FLAGS = -lX11

${PROG}: ${PROG}.c
	@gcc -o ${PROG} ${PROG}.c ${FLAGS}
	@strip ${PROG}

man:
	@gzip -c ${PROG}.1 > ${PROG}.1.gz

clean:
	@rm -rf ${PROG} ${PROG}.tar.gz ${PROG}.1.gz

tarball: clean
	@tar -czf ${PROG}.tar.gz *

install: ${PROG} man
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@install -m755 ${PROG} ${DESTIR}${PREFIX}/bin/${PROG}
	@install -Dm666 ${PROG}.1.gz ${DESTDIR}${PREFIX}/man1/${PROG}.1.gz
	

