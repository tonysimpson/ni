##############################################################################
#
###  Fix the Python include path for your system.
#
# PY_INCLUDE = /home/arigo/cvs/python/dist/src/Include
PY_INCLUDE = /usr/include/python2.2
# PY_INCLUDE = /users/arigo/include/python2.2
# PY_INCLUDE = /home/arigo/include/python2.1
#
#
CC         = gcc
C_FLAGS    = -g -Wall -Wstrict-prototypes  # -O3 -DDISABLE_DEBUG
LINK_FLAGS = -shared
#
##############################################################################

HEADERS1   = psyco.h      codemanager.h  dispatcher.h   processor.h    \
	     vcompiler.h  mergepoints.h  encoding.h     pycencoding.h  \
	     linuxmemchk.h  selective.h

SOURCES1   = psyco.c      codemanager.c  dispatcher.c   processor.c    \
	     vcompiler.c  mergepoints.c  pycencoding.c                 \
	     linuxmemchk.c  selective.c

OBJECTS1   = psyco.o      codemanager.o  dispatcher.o   processor.o    \
	     vcompiler.o  mergepoints.o  pycencoding.o                 \
	     linuxmemchk.o  selective.o

HEADERS2   = Python/pycompiler.h    Python/pycheader.h     \
	     Python/pbltinmodule.h

SOURCES2   = Python/pycompiler.c    \
	     Python/pbltinmodule.c

OBJECTS2   = Python/pycompiler.o    \
	     Python/pbltinmodule.o

HEADERS3   = Objects/pabstract.h     \
	     Objects/pclassobject.h    \
	     Objects/pdescrobject.h    \
	     Objects/pdictobject.h    \
	     Objects/pfuncobject.h    \
	     Objects/pintobject.h    \
	     Objects/pfloatobject.h	\
	     Objects/piterobject.h    \
	     Objects/plistobject.h    \
	     Objects/plongobject.h    \
	     Objects/pmethodobject.h    \
	     Objects/pobject.h    \
	     Objects/pstringobject.h    \
	     Objects/pstructmember.h    \
	     Objects/psycofuncobject.h    \
	     Objects/ptupleobject.h

SOURCES3   = Objects/pabstract.c     \
	     Objects/pclassobject.c    \
	     Objects/pdescrobject.c    \
	     Objects/pdictobject.c    \
	     Objects/pfuncobject.c    \
	     Objects/pintobject.c    \
	     Objects/pfloatobject.c	\
	     Objects/piterobject.c    \
	     Objects/plistobject.c    \
	     Objects/plongobject.c    \
	     Objects/pmethodobject.c    \
	     Objects/pobject.c    \
	     Objects/pstringobject.c    \
	     Objects/pstructmember.c    \
	     Objects/psycofuncobject.c    \
	     Objects/ptupleobject.c

OBJECTS3   = Objects/pabstract.o     \
	     Objects/pclassobject.o    \
	     Objects/pdescrobject.o    \
	     Objects/pdictobject.o    \
	     Objects/pfuncobject.o    \
	     Objects/pintobject.o    \
	     Objects/pfloatobject.o	\
	     Objects/piterobject.o    \
	     Objects/plistobject.o    \
	     Objects/plongobject.o    \
	     Objects/pmethodobject.o    \
	     Objects/pobject.o    \
	     Objects/pstringobject.o    \
	     Objects/pstructmember.o    \
	     Objects/psycofuncobject.o    \
	     Objects/ptupleobject.o

EXTRAS = linuxmemchk.h  linuxmemchk.c  hack.c  Makefile
DIST_FILES1  = ${HEADERS1} ${SOURCES1} ${EXTRAS}
DIST_FILES2  = ${HEADERS2} ${SOURCES2}
DIST_FILES3  = ${HEADERS3} ${SOURCES3}

hack:  ${HEADERS1} ${HEADERS2} ${HEADERS3} ${SOURCES1} ${SOURCES2} ${SOURCES3} hack.c
	${CC} ${C_FLAGS} hack.c -o _psycomodule.so -I${PY_INCLUDE} ${LINK_FLAGS}

_psycomodule.so:  ${OBJECTS1} ${OBJECTS2} ${OBJECTS3}
	${CC} ${LINK_FLAGS} ${OBJECTS1} ${OBJECTS2} ${OBJECTS3} -o _psycomodule.so

O3:  ${HEADERS1} ${HEADERS2} ${HEADERS3} ${SOURCES1} ${SOURCES2} ${SOURCES3} hack.c
	${CC} ${C_FLAGS} -O3 -DDISABLE_DEBUG hack.c -o _psycomodule.so -I${PY_INCLUDE} ${LINK_FLAGS}

${OBJECTS1}: %.o: %.c ${HEADERS1} ${HEADERS2}
	${CC} ${C_FLAGS} -c $< -o $@ -I${PY_INCLUDE}

${OBJECTS2}: %.o: %.c ${HEADERS1} ${HEADERS2} ${HEADERS3}
	${CC} ${C_FLAGS} -c $< -o $@ -I${PY_INCLUDE}

${OBJECTS3}: %.o: %.c ${HEADERS1} ${HEADERS2} ${HEADERS3}
	${CC} ${C_FLAGS} -c $< -o $@ -I${PY_INCLUDE}


clean:
	rm -f ${OBJECTS1} ${OBJECTS2} ${OBJECTS3}
	rm -f -- *~ .*~ *.pyc *.pyo core dist_files? Python/*~ Objects/*~
	rm -f -- ../*~ ../.*~
	rm -f -- ../py-utils/*~ ../py-utils/.*~ ../py-utils/*.pyc ../py-utils/*.pyo
	rm -f -- ../doc/*~ ../doc/.*~ ../doc/psyco.aux ../doc/psyco.dvi ../doc/psyco.log ../doc/psyco.ps ../doc/texput.log
	rm -fr ../doc/psyco psyco3

copy: copywindows copyfloppy

copywindows: clean
	mount /windows
	tar zcf /windows/windows/Bureau/psyco3.tgz *
	umount /windows

copyfloppy: clean
	mount /floppy
	tar zcf /floppy/psyco3.tgz *
	umount /floppy

dist_files: Makefile
	echo ${DIST_FILES1} > dist_files1
	echo ${DIST_FILES2} > dist_files2
	echo ${DIST_FILES3} > dist_files3
