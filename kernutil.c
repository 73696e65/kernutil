#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/uio.h>

#include <mach/mach.h>      // kern_return_t

#include "modes.h"
#include "helper.h"

uint8_t  verbose   = 0;
size_t delimiter   = 0;

/*
TODO: Implement a routine for finding kslide
*/


/* Standard Usage Info */
static void
usage(char *argv0)
{
    LOG("Usage: "
        "%s -m <read|write|kslide> <-a address|-s symbol> [-l kslide] [-w format] [-c count]\n"
        "    -m <mode>      read, write - r/w from the kernel memory\n"
        "                   kslide - find the kslide\n"
        "    -s <symbol>    symbol used in r/w mode\n"
        "    -a <address>   address used in r/w mode, in the hex format\n"
        "    -l <offset>    add kslide (or arbitrary positive offset in hex format) to this address\n"
        "    -c <count>     count of bytes or numbers (for -w) to read. default: 8\n"
        "    -w             use custom format for printing unsigned numbers interpretation\n"
        "                   width could be: s (string) 1 (byte), 2 (word) 4 (dword) or 8 (qword)\n"
        "                   example: -w \"144s\"\n"
        "                   to print the delimiter (for arrays), use ':' as the first character\n"
        "    -v             verbose mode\n"
        "    -b             raw kernelbase address (without kaslr)\n"
        "    -f             kernel image or decrypted kernelcache. default: /System/Library/Kernels/kernel\n\n"
        "Examples for reading:\n"
        "    sudo %s -m read -l 0x8000000 -s _allproc -w 8 -c 20\n"
        "    sudo %s -m read -l 0x8000000 -a 0xffffff80001961a3 -c 10\n"
        "    sudo %s -m read -l 0x8000000 -a 0xFFFFFF8000C48090 -w :88422 -c 100\n"
        "    sudo %s -v -m read -l 0x8000000 -s _mac_policy_list -w 4444448 -c 1\n"
        "    sudo %s -m read -a 0xffffff7f892664b8 -c 1 -w :ss8"
        "\n\n"
        "Examples for writing:\n"
        "    echo -ne \"\\xde\\xad\\xbe\\xef\\x13\\x37\" | sudo %s -m write -l 0x8000000 -a 0xffffff80001961a3\n"
        "    echo -ne \"\\xc0\\xde\" > test.bin; sudo %s -m write -l 0x8000000 -a 0xffffff80001961a3 < test.bin\n"
        "    python -c 'from sys import stdout; stdout.write(\"\\x41\" * 6)' | sudo %s -m write -l 0x8000000 -a 0xffffff80001961a3\n"
        "\n"
        , argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0);
    exit(1);
}

int
main(int argc, char *argv[])
{

    int opt;

    uint8_t  mode           = 0;
    uint8_t  mode_selected  = 0;
    uint64_t kaddr          = 0;
    uint8_t  *format        = NULL;

    size_t ptr_size         = 0;
    size_t format_size      = 0;
    size_t count            = 8;

    uint64_t kernelbase     = 0;
    char    *kernelpath     = NULL;
    char    *symbol         = NULL;

    kernel_map_t *km        = NULL;

    while ((opt = getopt(argc, argv, "m:a:s:l:c:w:vhkb:f:")) > 0)

        switch (opt)
        {

        case 'b':
        {
            uint64_t _addr = 0;
            if ((_addr = strtoull(optarg, NULL, 16)) == 0 && errno == EINVAL)
                ERR("Invalid kernelbase specified: %s", optarg);
            kernelbase = _addr;
            break;
        }

        case 'f':
        {
            kernelpath = optarg;
            if (access(kernelpath, F_OK) == -1 )
                ERR("File %s cannot be read", optarg);
            break;
        }

        case 'm':
        {
            if (mode_selected == 1)
                ERR("Only one mode can be selected.");
            if (!strcmp("read", optarg))
                mode |= m_READ;
            else if (!strcmp("write", optarg))
                mode |= m_WRITE;
            else if (!strcmp("kslide", optarg))
                mode |= m_FIND_KSLIDE;
            else
                ERR("Invalid mode specified: %s", optarg);
            mode_selected = 1;
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
            mode |= m_SET_KSLIDE;
            break;
        }

        case 's':
        {
            if (mode & m_ADDRESS)
                ERR("You can specify only -a or -s, not both.");
            symbol = optarg;
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
                /* Interpret 's' as a 8-byte pointer to a string */
                if (format[i] == 's')
                {
                    format_size += 8;
                    continue;
                }
                /* Numerical outputs */
                uint8_t w = format[i] - 0x30;
                if (w != 1 && w != 2 && w != 4 && w != 8)
                {
                    ERR("Invalid number specified. Only s, 1, 2, 4 or 8 allowed.");
                }
                format_size += w;
            }
            mode |= m_FORMAT;
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

    /* If we are using r/w mode, we need to set the kaddr */
    if ( (mode & m_READ || mode & m_WRITE) && !(mode & m_ADDRESS) && !(mode & m_SYMBOL) )
        ERR("You have not specified -a or -s.");

    /* We cannot read the symbols in memory without kslide */
    if (mode & m_SYMBOL && !(mode & m_SET_KSLIDE))
        ERR("For symbol resolution (-s) you must specify the kslide.");

    /* We need a valid file for a symbol resolution or
       kslide brute force when kernelbase is not specified */
    if (mode & m_SYMBOL || (mode & m_FIND_KSLIDE && kernelbase == 0))
    {
        if (kernelpath == NULL)
            km = map_file(DEFAULT_KERNEL_PATH);
        else
            km = map_file(kernelpath);

        if (!km)
            ERR("Resolving symbols works only with a valid kernel path, use -f parameter.");
        kernelbase = find_kernel_base(km);

        if (verbose)
            LOG("[i] kernelbase from file: 0x%llx.\n", kernelbase);
    }

    /* Symbol resolution */
    if (mode & m_SYMBOL && !(mode & m_FIND_KSLIDE))
    {
        uint64_t _addr = find_symbol_address(km, symbol);
        if (verbose)
            LOG("[i] Symbol '%s' resolved at 0x%llx\n", symbol, _addr);
        kaddr += _addr;
    }

    mach_port_t tfp0 = MACH_PORT_NULL;
    obtain_tfp0(&tfp0);

    if (verbose)
        LOG("[i] tfp0 obtained\n");

    /* Kernel read primitive */
    if (mode & m_READ)
        kernutil_read(tfp0, kaddr, mode, count, format, format_size);

    /* Kernel write primitive */
    if (mode & m_WRITE)
        kernutil_write(tfp0, kaddr);

    /* Find the kernel slide */
    if (mode & m_FIND_KSLIDE)
        kernutil_find_kslide(tfp0, kernelbase);

    return 0;
}