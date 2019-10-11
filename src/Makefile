.PHONY: all
all:
	$(MAKE) -C mclient all
	$(MAKE) -C server all
	$(MAKE) -C worker all
	-$(MAKE) -C gpuworker all
	$(MAKE) -C query all

.PHONY: clean
clean:
	$(MAKE) -C mclient clean
	$(MAKE) -C server clean
	$(MAKE) -C worker clean
	$(MAKE) -C gpuworker clean
	$(MAKE) -C query clean
