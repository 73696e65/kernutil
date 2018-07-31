# kernutil

This is a very simple PoC of kernel read/write instrument for iOS and macOS. To use it, you need to obtain tfp0 (from HSP4, but this could be easily changed) and a valid memory address (or KASLR leak). There are much better and sophisticated options, like [https://github.com/bazad/memctl](https://github.com/bazad/memctl) or [https://github.com/Siguza/ios-kern-utils](https://github.com/Siguza/ios-kern-utils]). My goal was to keep it simple and readable and when I will need more features, I will probably add them. The tool was tested on iOS 11.1.2 and macOS 10.13.3, it should be b/f compatible in a reasonable range.

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

### Examining MACF policies (macOS):

```
# check how many MAC policies are loaded and where are the entries located
$ sudo ./kernutil -m read -l 0x8000000 -s _mac_policy_list -w 4444448 -c 1
[0xffffff8008bfcb38]: 0x00000004
[0xffffff8008bfcb3c]: 0x00000200
[0xffffff8008bfcb40]: 0x00000003
[0xffffff8008bfcb44]: 0x00000004
[0xffffff8008bfcb48]: 0x00000001
[0xffffff8008bfcb4c]: 0x00000004
[0xffffff8008bfcb50]: 0xffffff800e779000

# read the policy entries
$ sudo ./kernutil -m read -a 0xffffff800e779000 -c 5 -w 8
[0xffffff800e779000]: 0xffffff7f892664b8
[0xffffff800e779008]: 0xffffff7f8928e0c0
[0xffffff800e779010]: 0xffffff7f895b1110
[0xffffff800e779018]: 0xffffff7f8a619010
[0xffffff800e779020]: 0x0000000000000000

# 1)
$ sudo ./kernutil -m read -a 0xffffff7f892664b8 -c 1 -w ss888
[0xffffff7f892664b8]: 0xffffff7f89262f13 => AMFI
[0xffffff7f892664c0]: 0xffffff7f89262f18 => Apple Mobile File Integrity
[0xffffff7f892664c8]: 0xffffff7f892656d0
[0xffffff7f892664d0]: 0x0000000000000001
[0xffffff7f892664d8]: 0xffffff7f89265a40

# 2)
$ sudo ./kernutil -m read -a 0xffffff7f8928e0c0 -c 1 -w ss888
[0xffffff7f8928e0c0]: 0xffffff7f892882cd => Sandbox
[0xffffff7f8928e0c8]: 0xffffff7f8928850a => Seatbelt sandbox policy
[0xffffff7f8928e0d0]: 0xffffff7f8928e110
[0xffffff7f8928e0d8]: 0x0000000000000001
[0xffffff7f8928e0e0]: 0xffffff7f8928e118

# 3)
$ sudo ./kernutil -m read -a 0xffffff7f895b1110 -c 1 -w ss888
[0xffffff7f895b1110]: 0xffffff7f895b0c06 => Quarantine
[0xffffff7f895b1118]: 0xffffff7f895b0c7c => Quarantine policy
[0xffffff7f895b1120]: 0xffffff7f895b1160
[0xffffff7f895b1128]: 0x0000000000000001
[0xffffff7f895b1130]: 0xffffff7f895b1168

# 4)
$ sudo ./kernutil -m read -a 0xffffff7f8a619010 -c 1 -w ss888
[0xffffff7f8a619010]: 0xffffff7f8a618fd4 => TMSafetyNet
[0xffffff7f8a619018]: 0xffffff7f8a618fe0 => Safety net for Time Machine
[0xffffff7f8a619020]: 0xffffff7f8a619060
[0xffffff7f8a619028]: 0x0000000000000001
[0xffffff7f8a619030]: 0xffffff7f8a619068

# creating mac_policy_ops.txt for the recent kernel
$ sed -n '/struct mac_policy_ops {/,/}/ p' ~/latest-xnu/security/mac_policy.h | sed '/^\s*$/d;1d;$d' > files/xnu-4570.41.2_mac_policy_ops.txt

$ wc -l files/xnu-4570.41.2_mac_policy_ops.txt
     335 files/xnu-4570.41.2_mac_policy_ops.txt

# reading the enabled operations for AMFI
$ paste <(sudo ./kernutil -m read -a 0xffffff7f89265a40 -c 335 -w 8) files/xnu-4570.41.2_mac_policy_ops.txt | grep -v 0x0000000000000000
[0xffffff7f89265a70]: 0xffffff7f892602e4        mpo_cred_check_label_update_execve_t    *mpo_cred_check_label_update_execve;
[0xffffff7f89265a98]: 0xffffff7f892602ef        mpo_cred_label_associate_t      *mpo_cred_label_associate;
[0xffffff7f89265aa8]: 0xffffff7f8926033c        mpo_cred_label_destroy_t        *mpo_cred_label_destroy;
[0xffffff7f89265ac0]: 0xffffff7f892603cc        mpo_cred_label_init_t           *mpo_cred_label_init;
[0xffffff7f89265ad0]: 0xffffff7f8925ee4a        mpo_cred_label_update_execve_t      *mpo_cred_label_update_execve;
[0xffffff7f89265b60]: 0xffffff7f8925ec6e        mpo_file_check_mmap_t           *mpo_file_check_mmap;
[0xffffff7f89265c40]: 0xffffff7f8926107b        mpo_file_check_library_validation_t     *mpo_file_check_library_validation;
[0xffffff7f89265de0]: 0xffffff7f892610d1        mpo_policy_initbsd_t            *mpo_policy_initbsd;
[0xffffff7f89265df8]: 0xffffff7f89260403        mpo_proc_check_inherit_ipc_ports_t  *mpo_proc_check_inherit_ipc_ports;
[0xffffff7f89265e40]: 0xffffff7f8925e84e        mpo_exc_action_check_exception_send_t   *mpo_exc_action_check_exception_send;
[0xffffff7f89265e48]: 0xffffff7f8925e674        mpo_exc_action_label_associate_t    *mpo_exc_action_label_associate;
[0xffffff7f89265e50]: 0xffffff7f8925e698        mpo_exc_action_label_populate_t     *mpo_exc_action_label_populate;
[0xffffff7f89265e58]: 0xffffff7f8925e67a        mpo_exc_action_label_destroy_t      *mpo_exc_action_label_destroy;
[0xffffff7f89265e60]: 0xffffff7f8925e632        mpo_exc_action_label_init_t     *mpo_exc_action_label_init;
[0xffffff7f89265e68]: 0xffffff7f8925e7fc        mpo_exc_action_label_update_t       *mpo_exc_action_label_update;
[0xffffff7f892663c0]: 0xffffff7f8926040b        mpo_vnode_check_signature_t     *mpo_vnode_check_signature;
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
