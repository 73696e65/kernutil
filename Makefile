ARM_CC = $(shell xcrun --sdk iphoneos --find clang)
ARM_CFLAGS := -arch arm64
ARM_CFLAGS += -isysroot $(shell xcrun --sdk iphoneos --show-sdk-path)

X64_CC = clang

all: kernutil

kernutil-arm64: $(wildcard *.[ch])
	$(ARM_CC) $(ARM_CFLAGS) -o $@ $(wildcard *.c)
	jtool --sign --inplace --ent entitlement.xml $@

kernutil-x64: $(wildcard *.[ch])
	$(X64_CC) $(X64_CFLAGS) -o $@ $(wildcard *.c)

kernutil: kernutil-arm64 kernutil-x64
	lipo -create -output $@ $^

clean:
	rm -f *.o kernutil kernutil-arm64 kernutil-x64
