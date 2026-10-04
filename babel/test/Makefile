# test-html is not part of "all": the html module is not registered in
# modules.h, so HTML files are deliberately left unclaimed here.
all: test-adrift test-alan test-hugo test-twine test-bronze test-sensory test-quest test-marker


.PHONY: test-adrift
test-adrift:
	$(MAKE) -C adrift

.PHONY: test-alan
test-alan:
	$(MAKE) -C alan

.PHONY: test-hugo
test-hugo:
	$(MAKE) -C hugo

.PHONY: test-twine
test-twine:
	$(MAKE) -C twine

.PHONY: test-html
test-html:
	$(MAKE) -C html

.PHONY: test-bronze
test-bronze:
	$(MAKE) -C bronze

.PHONY: test-sensory
test-sensory:
	$(MAKE) -C sensory

.PHONY: test-quest
test-quest:
	$(MAKE) -C quest

.PHONY: test-marker
test-marker:
	$(MAKE) -C marker

clean:
	$(MAKE) -C adrift clean
	$(MAKE) -C alan clean
	$(MAKE) -C hugo clean
	$(MAKE) -C twine clean
	$(MAKE) -C html clean
	$(MAKE) -C bronze clean
	$(MAKE) -C sensory clean
	$(MAKE) -C quest clean
	$(MAKE) -C marker clean
