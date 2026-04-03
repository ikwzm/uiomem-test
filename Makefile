
TESTS = uiomem-file-test       \
        uiomem-ioctl-test      \
        uiomem-throughput-test \
        $(END_LIST)

DELETE     := -rm
CC         := gcc
CC_OPTIONS := -O2 -DUSE_UIOMEM_IOCTL

all: $(TESTS)

clean:
	$(DELETE) $(TESTS)

uiomem-ioctl-test: uiomem-ioctl-test.c
	$(CC) $(CC_OPTIONS) -o $@ $<

uiomem-file-test:  uiomem-file-test.c  uiomem.c uiomem.h 
	$(CC) $(CC_OPTIONS) -o $@ $< uiomem.c

uiomem-throughput-test:  uiomem-throughput-test.c uiomem.c uiomem.h 
	$(CC) $(CC_OPTIONS) -o $@ $< uiomem.c

