BASENAME ?= $(shell basename $(CURDIR))

-include $(BASENAME).mk

# These are not actual bench; just here for testing; can remove later
foo: foo.o
throw: throw.o

ast: foo.c
	$(CC) $(CFLAGS) -fsanitize=thread -Xclang -ast-dump -fsyntax-only $^	
