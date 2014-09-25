SHELL = /bin/sh

SUBDIRS = tslib libstructures h264bitstream logging

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
		cd $@; ./configure; cd -; \
	fi
	$(MAKE) -C $@

libstructures: 
	$(MAKE) -C $@

tslib: libstructures h264bitstream logging
	$(MAKE) -C $@

clean: 
	for dir in $(SUBDIRS); do \
		echo "Cleaning $$dir..."; \
		$(MAKE) -C $$dir clean; \
	done


