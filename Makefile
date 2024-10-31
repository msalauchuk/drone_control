SUBDIRS = con2redis/src/ control_center/ drone/ 

all:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir ; \
	done

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done