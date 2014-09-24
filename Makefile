SHELL = /bin/sh

SUBDIRS = tslib libstructures h264bitstream 

logging:
	$(MAKE) -C logging

$(SUDIRS): logging
	$(MAKE) -C $@

subdirs: $(SUBDIRS)

subdirs-clean:
	for dir in $(SUBDIRS) logging; do \
        echo "Cleaning $$dir..."; \
		$(MAKE) -C $$dir clean; \
	done

all: subdirs
default: all

clean: subdirs-clean

.PHONY: subdirs $(SUBDIRS) logging subdirs-clean
