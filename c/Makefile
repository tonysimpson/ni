
# G = debug, O = optimized, V = verbose, X = all tests,
# H = heavy mem check
mode = O


# OPTS    = -DPSYCO_DEBUG=1 -DBIG_BUFFER_SIZE=0x100000 -DVERBOSE_LEVEL=0 -DCODE_DUMP=1 -DHEAVY_MEM_CHECK=0
OPTS_G    = -DPSYCO_DEBUG=1 -DVERBOSE_LEVEL=1 -DHEAVY_MEM_CHECK=0 -DCODE_DUMP=1
C_FLAGS_G = -g

OPTS_O    = -DPSYCO_DEBUG=0 -DVERBOSE_LEVEL=0 -DHEAVY_MEM_CHECK=0 -DCODE_DUMP=0
C_FLAGS_O = -O3

OPTS_OG   = -DPSYCO_DEBUG=0 -DVERBOSE_LEVEL=3 -DHEAVY_MEM_CHECK=0 -DCODE_DUMP=0
C_FLAGS_OG= -O3 -g

OPTS_V    = -DPSYCO_DEBUG=1 -DVERBOSE_LEVEL=3 -DHEAVY_MEM_CHECK=0 -DCODE_DUMP=1
C_FLAGS_V = -g

OPTS_X    = -DPSYCO_DEBUG=1 -DVERBOSE_LEVEL=4 -DHEAVY_MEM_CHECK=0 -DCODE_DUMP=3
C_FLAGS_X = -g

OPTS_H    = -DPSYCO_DEBUG=1 -DVERBOSE_LEVEL=1 -DHEAVY_MEM_CHECK=1 -DCODE_DUMP=1
C_FLAGS_H = -g

OPTS_P    = -DPSYCO_DEBUG=1 -DVERBOSE_LEVEL=1 -DHEAVY_MEM_CHECK=0 -DCODE_DUMP=1 -DVERBOSE_STATS=1
C_FLAGS_P = -g


OPTS     = ${OPTS_${mode}}
C_FLAGS  = -DNDEBUG ${C_FLAGS_${mode}} -Wall -Wstrict-prototypes -fPIC ${OPTS} ${incl}
LD_FLAGS = -shared
OUTDIR   = ../build/lib.linux-i686-${PY_VER}/psyco
OUTPUT   = ${OUTDIR}/_psyco.so

ifdef cvs
 cvshome = ${HOME}/cvs/2.2
 python  = ${cvshome}/dist/src/python
else
 python  = python
endif


PY_VER  := $(shell $(python) -c "import sys; print '%d.%d' % sys.version_info[:2]")
PY_VER_EX := $(shell $(python) -V 2>&1)
incl    := $(shell $(python) mkincl.py)

BUILD_VERSION = Compiling mode ${mode} for ${PY_VER_EX}

main: build_ext build_py

dyn: ${OUTDIR}/dyn-tag build_py

build_ext: build_py
	@echo -----------------------------------
	@echo ${BUILD_VERSION}
	@echo -----------------------------------
	gcc ${LD_FLAGS} ${C_FLAGS} -DALL_STATIC=1 psyco.c -o ${OUTPUT}
	echo ${BUILD_VERSION} > ${OUTDIR}/build-version

build_py:
	cd ..; $(python) setup.py build_py

include dyn-Makefile

%.o: %.c
	gcc ${C_FLAGS} -DALL_STATIC=0 -c $< -o $@

${OUTDIR}/dyn-tag: ${O_FILES}
	gcc ${LD_FLAGS} $^ -o ${OUTPUT}
	echo ${BUILD_VERSION} > ${OUTDIR}/build-version
	echo dyn-tag >> ${OUTDIR}/build-version
	touch $@

clean:
	-rm -f ${O_FILES}

depend:
	python files.py
	python depend.py
