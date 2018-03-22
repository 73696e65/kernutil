# kernutil

This is a very simple PoC of kernel read/write instrument for iOS and macOS. To use it, you need to obtain tfp0 (from HSP4, but this could be easily changed) and a valid memory address (or KASLR leak). There are much better and sophisticated options, like [https://github.com/bazad/memctl](memctl) or [https://github.com/Siguza/ios-kern-utils](ios-kern-utils). My goal was to keep it simple and readable and when I will need more features, I will probably add them. The tool was tested on iOS 11.1.2 and macOS 10.13.3, it should be b/f compatible in a reasonable range.

## Building

```
make kernutil-x64    # x64 build
make kernutil-arm64  # arm64 build, probably you need jtool to sign the entitlements
make kernutil        # fat binary
```

## Examples

### R/W from/to BSS section:

```
$ sudo ./kernutil -m read -l 0x8000000 -a 0xffffff80001961a3 -c 10
[0xffffff80081961a3]  00 00 00 00 00 00 00 00 00 00                    ..........

$ python -c 'from sys import stdout; stdout.write("\x41" * 6)' | sudo ./kernutil -m write -l 0x8000000 -a 0xffffff80001961a3

$ sudo ./kernutil -m read -l 0x8000000 -a 0xffffff80001961a3 -c 10
[0xffffff80081961a3]  41 41 41 41 41 41 00 00 00 00                    AAAAAA....

$ echo -ne "\xc0\xde" > test.bin
$ sudo ./kernutil -m write -l 0x8000000 -a 0xffffff80001961a3 < test.bin

$ echo -ne "\xde\xad\xbe\xef\x13\x37" | sudo ./kernutil -m write -l 0x8000000 -a 0xffffff80001961a3
```

### Resolving symbols (macOS):

```
# check how many MAC policies are loaded and where are the entries located
$ sudo ./kernutil-x64 -m read -l 0xf000000 -s _mac_policy_list -w 4444448 -c 1
[0xffffff800fbfcb38]: 0x00000004
[0xffffff800fbfcb3c]: 0x00000200
[0xffffff800fbfcb40]: 0x00000003
[0xffffff800fbfcb44]: 0x00000004
[0xffffff800fbfcb48]: 0x00000001
[0xffffff800fbfcb4c]: 0x00000004
[0xffffff800fbfcb50]: 0xffffff80158a0000

# read the policy entries
$ sudo ./kernutil-x64 -m read -a 0xffffff80158a0000 -c 4 -w 8
[0xffffff80158a0000]: 0xffffff7f902664b8
[0xffffff80158a0008]: 0xffffff7f9028e0c0
[0xffffff80158a0010]: 0xffffff7f905c2110
[0xffffff80158a0018]: 0xffffff7f9162a010

# examine the first one
$ sudo ./kernutil-x64 -m read -a 0xffffff7f902664b8 -c 1 -w ss888
[0xffffff7f902664b8]: 0xffffff7f90262f13 => AMFI
[0xffffff7f902664c0]: 0xffffff7f90262f18 => Apple Mobile File Integrity
[0xffffff7f902664c8]: 0xffffff7f902656d0
[0xffffff7f902664d0]: 0x0000000000000001
[0xffffff7f902664d8]: 0xffffff7f90265a40

# examine the second one
$ sudo ./kernutil-x64 -m read -a 0xffffff7f9028e0c0 -c 1 -w ss888
[0xffffff7f9028e0c0]: 0xffffff7f902882cd => Sandbox
[0xffffff7f9028e0c8]: 0xffffff7f9028850a => Seatbelt sandbox policy
[0xffffff7f9028e0d0]: 0xffffff7f9028e110
[0xffffff7f9028e0d8]: 0x0000000000000001
[0xffffff7f9028e0e0]: 0xffffff7f9028e118
```

### Reading the `_sysent` structure with the custom array format:

```
$ sudo ./kernutil -m read -l 0x8000000 -a 0xFFFFFF8000C48090 -w :88422 -c 3
----------------------------------
[0xffffff8008c48090]: 0xffffff80087a64a0
[0xffffff8008c48098]: 0x0000000000000000
[0xffffff8008c480a0]: 0x00000001
[0xffffff8008c480a4]: 0x0000
[0xffffff8008c480a6]: 0x0000
----------------------------------
[0xffffff8008c480a8]: 0xffffff8008773c10
[0xffffff8008c480b0]: 0xffffff80084da510
[0xffffff8008c480b8]: 0x00000000
[0xffffff8008c480bc]: 0x0001
[0xffffff8008c480be]: 0x0004
----------------------------------
[0xffffff8008c480c0]: 0xffffff800877a5a0
[0xffffff8008c480c8]: 0x0000000000000000
[0xffffff8008c480d0]: 0x00000001
[0xffffff8008c480d4]: 0x0000
[0xffffff8008c480d6]: 0x0000
```
