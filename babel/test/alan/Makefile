# Test babel with Alan (3)

include ../Makefile.boilerplate

.PHONY: $(CURDIR)
$(CURDIR):
	echo Valid: > output
	../../babel -ifid valid.a3c >> output
	echo Invalid: >> output
	../../babel -ifid invalid.a3c >> output
	echo V3 without UUID >> output
	../../babel -ifid withoutUUID.a3c >> output
	$(VERIFY)

clean:
	-rm -f output
