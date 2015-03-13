SHELL = /bin/sh

SUBDIRS = atsstreamapp atstest tslib libstructures h264bitstream logging

.PHONY: clean subdirs $(SUBDIRS)

all: subdirs
subdirs: $(SUBDIRS)

logging:
	$(MAKE) -C $@

h264bitstream:
	if [ ! -f $@/configure ]; then \
		cd $@; ./autogen.sh; cd -; \
  fi; \
	if [ ! -f $@/Makefile ]; then \
		cd $@; ./configure --enable-static=yes --enable-shared=no; cd -; \
	fi
	$(MAKE) -C $@

libstructures: 
	$(MAKE) -C $@

tslib: libstructures h264bitstream logging
	$(MAKE) -C $@

atstest: tslib
	$(MAKE) -C $@

atsstreamapp: logging tslib
	$(MAKE) -C $@

clean: 
	for dir in $(SUBDIRS); do \
		echo "Cleaning $$dir..."; \
		$(MAKE) -C $$dir clean; \
	done


