all: test-alan test-twine test-bronze test-sensory


.PHONY: test-alan
test-alan:
	$(MAKE) -C alan

.PHONY: test-twine
test-twine:
	$(MAKE) -C twine

.PHONY: test-bronze
test-bronze:
	$(MAKE) -C bronze

.PHONY: test-sensory
test-sensory:
	$(MAKE) -C sensory

clean:
	$(MAKE) -C alan clean
	$(MAKE) -C twine clean
	$(MAKE) -C bronze clean
	$(MAKE) -C sensory clean
