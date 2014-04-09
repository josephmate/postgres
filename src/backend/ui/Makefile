#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for backend/ui
#
# IDENTIFICATION
#    src/backend/ui/Makefile
#
#-------------------------------------------------------------------------

subdir = src/backend/ui
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

# gtk header files and their dependencies
override CPPFLAGS+=$(shell pkg-config gtk+-3.0 --cflags)

OBJS = optimizer_ui.o cstr_out.o join_path_helper.o

include $(top_srcdir)/src/backend/common.mk
