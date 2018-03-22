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


extern uint8_t  verbose;
extern size_t delimiter;

void
kernutil_read(mach_port_t tfp0,
              uint64_t kaddr,
              uint8_t mode,
              size_t count,
              uint8_t *format,
              size_t format_size)
{

    uint32_t size_to_read = count;

    if (mode & m_FORMAT)
        size_to_read *= format_size;

    if (verbose)
        LOG("[i] reading %d byte(s) from: 0x%llx.\n", (uint32_t) size_to_read, kaddr);

    uint64_t *uaddr = malloc(size_to_read);
    kread(tfp0, kaddr, (vm_address_t) uaddr, size_to_read);

    if (mode & m_FORMAT)
    {
        uint64_t *_addr = uaddr;
        size_t length = strlen((const char *) format);

        for (int j = 0; j < count; j++)
        {
            if (delimiter)
                LOG("----------------------------------\n");
            for (int i = 0; i < length; i++)
            {
                uint8_t w = format[i];
                switch (w)
                {
                case '1':
                {
                    uint8_t x = *(uint8_t *)_addr;
                    LOG("[0x%016llx]: 0x%02x\n", kaddr, x)
                    break;
                }
                case '2':
                {
                    uint16_t x = *(uint16_t *)_addr;
                    LOG("[0x%016llx]: 0x%04x\n", kaddr, x)
                    break;
                }
                case '4':
                {
                    uint32_t x = *(uint32_t *)_addr;
                    LOG("[0x%016llx]: 0x%08x\n", kaddr, x)
                    break;
                }
                case '8':
                {
                    uint64_t x = *(uint64_t *)_addr;
                    LOG("[0x%016llx]: 0x%016llx\n", kaddr, x)
                    break;
                }
                case 's':
                {
                    uint64_t x = *(uint64_t *)_addr;
                    void *output = kread_c_string(tfp0, x);
                    LOG("[0x%016llx]: 0x%016llx => %s\n", kaddr, x, (char *) output);
                    munmap(output, PAGE_SIZE);
                    w = '8'; /* Set the same width as a 8B pointer */
                    break;
                }
                }
                w = w - 0x30; /* Convert from ascii to integer */
                _addr = (uint64_t *) ((uint8_t *) _addr + w);
                kaddr += w;
            }
        }
    }
    else
        hexdump_with_offset(uaddr, size_to_read, kaddr);
    free(uaddr);
}

void
kernutil_write(mach_port_t tfp0,
               uint64_t kaddr)
{
    uint64_t _addr = kaddr;
    uint8_t x;

    /* Using 1 byte write is far from optimal, but for a small data sufficient */
    while (read(0, &x, 1))
    {
        kwrite_1B(tfp0, _addr, x);
        _addr++;
    }
}

void
kernutil_find_kslide(mach_port_t tfp0, uint64_t kernelbase)
{
    ERR("TODO");
    // kread(mach_port_t tfp0, vm_address_t kaddr, vm_address_t uaddr, uint32_t size)
    // kread(tfp0, kaddr, (vm_address_t) uaddr, size_to_read);

}


