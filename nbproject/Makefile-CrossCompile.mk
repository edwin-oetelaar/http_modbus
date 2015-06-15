#
# Generated Makefile - do not edit!
#
# Edit the Makefile in the project folder instead (../Makefile). Each target
# has a -pre and a -post target defined where you can add customized code.
#
# This makefile implements configuration specific macros and targets.


# Environment
MKDIR=mkdir
CP=cp
GREP=grep
NM=nm
CCADMIN=CCadmin
RANLIB=ranlib
CC=mipsel-linux-linux26-gcc
CCC=mipsel-linux-linux26-g++
CXX=mipsel-linux-linux26-g++
FC=gfortran
AS=mipsel-linux-linux26-as

# Macros
CND_PLATFORM=GNU_CROSS-Linux-x86
CND_DLIB_EXT=so
CND_CONF=CrossCompile
CND_DISTDIR=dist
CND_BUILDDIR=build

# Include project Makefile
include Makefile

# Object Directory
OBJECTDIR=${CND_BUILDDIR}/${CND_CONF}/${CND_PLATFORM}

# Object Files
OBJECTFILES= \
	${OBJECTDIR}/http_machine.o \
	${OBJECTDIR}/main.o \
	${OBJECTDIR}/not_used.c.o \
	${OBJECTDIR}/ringbuffer.o \
	${OBJECTDIR}/server.o


# C Compiler Flags
CFLAGS=

# CC Compiler Flags
CCFLAGS=
CXXFLAGS=

# Fortran Compiler Flags
FFLAGS=

# Assembler Flags
ASFLAGS=

# Link Libraries and Options
LDLIBSOPTIONS=

# Build Targets
.build-conf: ${BUILD_SUBPROJECTS}
	"${MAKE}"  -f nbproject/Makefile-${CND_CONF}.mk ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/h20_example

${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/h20_example: ${OBJECTFILES}
	${MKDIR} -p ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}
	mipsel-linux-linux26-gcc -o ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/h20_example ${OBJECTFILES} ${LDLIBSOPTIONS}

${OBJECTDIR}/http_machine.o: http_machine.c 
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -I../../opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/include -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/http_machine.o http_machine.c

${OBJECTDIR}/main.o: main.c 
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -I../../opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/include -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/main.o main.c

${OBJECTDIR}/not_used.c.o: not_used.c.c 
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -I../../opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/include -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/not_used.c.o not_used.c.c

${OBJECTDIR}/ringbuffer.o: ringbuffer.c 
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -I../../opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/include -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/ringbuffer.o ringbuffer.c

${OBJECTDIR}/server.o: server.c 
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -I../../opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/include -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/server.o server.c

# Subprojects
.build-subprojects:

# Clean Targets
.clean-conf: ${CLEAN_SUBPROJECTS}
	${RM} -r ${CND_BUILDDIR}/${CND_CONF}
	${RM} ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/h20_example

# Subprojects
.clean-subprojects:

# Enable dependency checking
.dep.inc: .depcheck-impl

include .dep.inc
