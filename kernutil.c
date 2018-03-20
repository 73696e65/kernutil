#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/uio.h>

#include <mach/mach.h>      // kern_return_t

#include "helper.h"

#define KERNEL_PATH "/System/Library/Kernels/kernel"

#define m_READ     (1<<1)
#define m_WRITE    (1<<2)

#define m_SYMBOL   (1<<3)
#define m_ADDRESS  (1<<4)

#define m_CUSTOM   (1<<5)

/* Standard Usage Info */
static void usage(char *argv0)
{
    LOG("Usage: "
        "%s -m <read|write> <-a address|-s symbol> [-l kslide] [-i size] [-c count]\n"
        "    -m <mode>      read or write (from|to) the kernel memory.\n"
        "    -s <symbol>    symbol used for the specified mode.\n"
        "    -a <address>   address used for the specified mode, in the hex format.\n"
        "    -l <offset>    add kslide (or arbitrary positive offset) to this address.\n"
        "    -c <count>     count of bytes or numbers (for -i) to read. default: 8\n"
        "    -w             use custom format for printing unsigned numbers interpretation.\n"
        "                   width could be: 1 (byte), 2 (word) 4 (dword) or 8 (qword).\n"
        "                   example: -w \"1448\". to print the delimiter (for arrays),\n"
        "                   use ':' as the first character.\n"
        "    -v             verbose mode.\n\n"
        "Examples for reading:\n"
        "    sudo %s -m read -l 0x8000000 -s _allproc -w 8 -c 20\n"
        "    sudo %s -m read -l 0x8000000 -a 0xffffff80001961a3 -c 10\n"
        "    sudo %s -m read -l 0x8000000 -a 0xFFFFFF8000C48090 -w :88422 -c 100\n"
        "    sudo %s -v -m read -l 0x8000000 -s _mac_policy_list -w 4444448 -c 1\n"
        "\n"
        "Examples for writing:\n"
        "    echo -ne \"\\xde\\xad\\xbe\\xef\\x13\\x37\" | sudo %s -m write -l 0x8000000 -a 0xffffff80001961a3\n"
        "    echo -ne \"\\xc0\\xde\" > test.bin; sudo %s -m write -l 0x8000000 -a 0xffffff80001961a3 < test.bin\n"
        "    python -c 'from sys import stdout; stdout.write(\"\\x41\" * 6)' | sudo %s -m write -l 0x8000000 -a 0xffffff80001961a3\n"
        "\n"
        , argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0);
    exit(1);
}

int main(int argc, char *argv[])
{

    int opt;

    uint8_t  mode      = 0;
    uint64_t kaddr     = 0;
    uint8_t  verbose   = 0;
    uint8_t  *format   = NULL;

    size_t ptr_size    = 0;
    size_t format_size = 0;
    size_t delimiter   = 0;
    size_t count       = 8;

    kernel_map_t *km     = NULL;

    while ((opt = getopt(argc, argv, "m:a:s:l:c:w:vh")) > 0)

        switch (opt)
        {

        case 'm':
        {
            if (!strcmp("read", optarg))
                mode |= m_READ;
            else if (!strcmp("write", optarg))
                mode |= m_WRITE;
            else
                ERR("Invalid mode specified: %s", optarg);
            break;
        }

        case 'a':
        {
            if (mode & m_SYMBOL)
                ERR("You can specify only -a or -s, not both.");

            uint64_t _addr = 0;
            if ((_addr = strtoull(optarg, NULL, 16)) == 0 && errno == EINVAL)
                ERR("Invalid address specified: %s", optarg);
            kaddr += _addr;
            mode |= m_ADDRESS;
            break;
        }

        case 'l':
        {
            uint64_t _addr = 0;
            if ((_addr = strtoull(optarg, NULL, 16)) == 0 && errno == EINVAL)
                ERR("Invalid kslide specified: %s", optarg);
            kaddr += _addr;
            break;
        }

        case 's':
        {
            if (mode & m_ADDRESS)
                ERR("You can specify only -a or -s, not both.");

            km = map_file(KERNEL_PATH);
            uint64_t _addr = find_symbol_address(km, optarg);
            kaddr += _addr;
            mode |= m_SYMBOL;
            break;
        }

        case 'c':
        {
            uint32_t _c = 0;
            _c = atoi(optarg);
            if (_c > 0)
                count = _c;
            else
                ERR("Invalid count number.");
            break;
        }

        case 'w':
        {
            if (optarg[0] == ':')
            {
                delimiter = 1;
                optarg = (char *) ((uint8_t *) optarg + 1);
            }
            format = (uint8_t *) optarg;
            size_t length = strlen((const char *) format);
            for (int i = 0; i < length; i++)
            {
                uint8_t w = format[i] - 0x30;
                if (w != 1 && w != 2 && w != 4 && w != 8)
                {
                    ERR("Invalid number specified. Only 1, 2, 4 or 8 allowed.");
                }
                format_size += w;
            }
            mode |= m_CUSTOM;
            break;
        }

        case 'v':
        {
            verbose = 1;
            break;
        }

        case 'h':
        default:
            usage(argv[0]);
        }

    if (getuid() != 0)
        ERR("%s must be run as root.", argv[0]);

    if ((mode & m_READ && mode & m_WRITE) || (!(mode & m_READ) && (!(mode & m_WRITE))))
        ERR("You must select exactly one mode from <read|write>.");

    if (!(mode & m_ADDRESS) && (!(mode & m_SYMBOL)))
        ERR("You have not specified -a or -s.");


    mach_port_t tfp0 = MACH_PORT_NULL;
    obtain_tfp0(&tfp0);

    /* Kernel read primitive */
    if (mode & m_READ)
    {

        uint32_t size_to_read = count;

        if (mode & m_CUSTOM)
            size_to_read *= format_size;

        if (verbose)
            LOG("[i] reading %d byte(s) from: 0x%llx.\n", (uint32_t) size_to_read, kaddr);

        uint64_t *uaddr = malloc(size_to_read);
        kread(tfp0, kaddr, (vm_address_t) uaddr, size_to_read);

        if (mode & m_CUSTOM)
        {
            uint64_t *_addr = uaddr;
            size_t length = strlen((const char *) format);

            for (int j = 0; j < count; j++)
            {
                if (delimiter) LOG("----------------------------------\n");
                for (int i = 0; i < length; i++)
                {
                    uint8_t w = format[i] - 0x30;
                    switch (w)
                    {
                    case 1:
                    {
                        uint8_t x = *(uint8_t *)_addr;
                        LOG("[0x%016llx]: 0x%02x\n", kaddr, x)
                        break;
                    }
                    case 2:
                    {
                        uint16_t x = *(uint16_t *)_addr;
                        LOG("[0x%016llx]: 0x%04x\n", kaddr, x)
                        break;
                    }
                    case 4:
                    {
                        uint32_t x = *(uint32_t *)_addr;
                        LOG("[0x%016llx]: 0x%08x\n", kaddr, x)
                        break;
                    }
                    case 8:
                    {
                        uint64_t x = *(uint64_t *)_addr;
                        LOG("[0x%016llx]: 0x%016llx\n", kaddr, x)
                        break;
                    }
                    }
                    _addr = (uint64_t *) ((uint8_t *) _addr + w);
                    kaddr += w;
                }
            }
        }
        else
            hexdump_with_offset(uaddr, size_to_read, kaddr);
        free(uaddr);
    }

    if (mode & m_WRITE)   /* Kernel write primitive */
    {
        uint8_t x;
        uint64_t _addr = kaddr;
        /* Using 1 byte write is far from optimal, but for a small data sufficient */
        while (read(0, &x, 1))
        {
            kwrite_1B(tfp0, _addr, x);
            _addr += 1;
        }
    }

    return 0;
}
