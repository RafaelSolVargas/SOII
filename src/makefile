# EPOS Source Makefile

include	../makedefs

SUBDIRS := utility architecture machine api setup boot system init network

all:		$(SUBDIRS)

$(SUBDIRS):	FORCE
		(cd $@ && $(MAKE))

clean:
		make MAKE:="$(MAKECLEAN)" $(SUBDIRS)

FORCE:
