# Test babel basic functionality with Bronze.zblorb

include ../Makefile.boilerplate

.PHONY: $(CURDIR)
$(CURDIR):
	../../babel -ifid Bronze.zblorb > output
	../../babel -format Bronze.zblorb >> output
	../../babel -identify Bronze.zblorb >> output
	../../babel -complete Bronze.zblorb sparse.iFiction >> output
	diff 1810847C-0DC7-44D5-94EF-313A3E7AF257.iFiction expected-c.iFiction >> output
	../../babel -ifiction Bronze.zblorb >> output
	diff 1810847C-0DC7-44D5-94EF-313A3E7AF257.iFiction expected.iFiction >> output
	../../babel -cover Bronze.zblorb >> output
	cmp 1810847C-0DC7-44D5-94EF-313A3E7AF257.jpg expected.jpg >> output
	$(VERIFY)

clean:
	-rm -f output 1810847C-0DC7-44D5-94EF-313A3E7AF257.*
