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
#override CPPFLAGS+=-I/usr/include/gtk-3.0/ -I/usr/include/glib-2.0/
# gtk library
#LIBS += -lgtk+-3.0
# gtk library dependencies
#LIBS += -lglib

#override CPPFLAGS+=`pkg-config --cflags --libs glib-3.0`
override CPPFLAGS+=$(shell pkg-config gtk+-3.0 --cflags)
#LIBS +=$(shell pkg-config gtk+-3.0 --libs)

OBJS = optimizer_ui.o

include $(top_srcdir)/src/backend/common.mk
