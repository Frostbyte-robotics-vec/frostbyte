################################################################################
######################### User configurable parameters #########################
# filename extensions
CEXTS:=c
ASMEXTS:=s S
CXXEXTS:=cpp c++ cc

# probably shouldn't modify these, but you may need them below
ROOT=.
FWDIR:=$(ROOT)/firmware

# Zielroboter: make ROBOT=stacker | make ROBOT=runner (kein Standardwert, damit nie versehentlich der falsche gebaut wird)
ROBOT?=
ifneq ($(MAKECMDGOALS),clean)
  ifeq ($(filter $(ROBOT),stacker runner),)
    $(error ROBOT muss stacker oder runner sein, z. B. make ROBOT=stacker)
  endif
endif

# Je Roboter eigene Objekt- und Abhaengigkeitsverzeichnisse, sonst wuerden Objekte des anderen Roboters wiederverwendet.
BINDIR=$(ROOT)/bin/$(ROBOT)
DEPDIR:=.d/$(ROBOT)
SRCDIR=$(ROOT)/src
INCDIR=$(ROOT)/include

WARNFLAGS+=
ROBOT_DEFINE:=$(if $(filter stacker,$(ROBOT)),FROSTBYTE_ROBOT_STACKER,FROSTBYTE_ROBOT_RUNNER)
# -iquote$(ROOT): config/ liegt ausserhalb von include/
EXTRA_CFLAGS=-D$(ROBOT_DEFINE)
# Release-Stand (L-03, docs/release.md): Version aus version.txt, kurzer Commit-Hash, dirty bei ungesicherten Aenderungen.
# Ohne git bleibt der Commit leer (Anzeige "-"). Die Defines aendern sich ohne Quelltext-Aenderung: fuer Releases vorher make clean.
FB_VERSION:=$(strip $(shell cat version.txt 2>/dev/null))
FB_COMMIT:=$(strip $(shell git rev-parse --short HEAD 2>/dev/null))
FB_DIRTY:=$(if $(strip $(shell git status --porcelain --untracked-files=no 2>/dev/null)),1,0)
BUILD_DEFINES=-DFB_VERSION='"$(FB_VERSION)"' -DFB_COMMIT='"$(FB_COMMIT)"' -DFB_DIRTY=$(FB_DIRTY)
EXTRA_CXXFLAGS=-D$(ROBOT_DEFINE) $(BUILD_DEFINES) -iquote$(ROOT) -iquote$(ROOT)/lib

# Set to 1 to enable hot/cold linking
# Frostbyte: 0, weil pros upload die Ausgabe-Pfade aus project.pros liest und BINDIR je Roboter verschieden ist (ADR-003)
USE_PACKAGE:=0

# Add libraries you do not wish to include in the cold image here
# EXCLUDE_COLD_LIBRARIES:= $(FWDIR)/your_library.a
EXCLUDE_COLD_LIBRARIES:= 

# Set this to 1 to add additional rules to compile your project as a PROS library template
IS_LIBRARY:=0
# TODO: CHANGE THIS! 
# Be sure that your header files are in the include directory inside of a folder with the
# same name as what you set LIBNAME to below.
LIBNAME:=libbest
VERSION:=1.0.0
# EXCLUDE_SRC_FROM_LIB= $(SRCDIR)/unpublishedfile.c
# this line excludes opcontrol.c and similar files
EXCLUDE_SRC_FROM_LIB+=$(foreach file, $(SRCDIR)/main,$(foreach cext,$(CEXTS),$(file).$(cext)) $(foreach cxxext,$(CXXEXTS),$(file).$(cxxext)))

# files that get distributed to every user (beyond your source archive) - add
# whatever files you want here. This line is configured to add all header files
# that are in the directory include/LIBNAME
TEMPLATE_FILES=$(INCDIR)/$(LIBNAME)/*.h $(INCDIR)/$(LIBNAME)/*.hpp

.DEFAULT_GOAL=quick

################################################################################
################################################################################
########## Nothing below this line should be edited by typical users ###########
-include ./common.mk
