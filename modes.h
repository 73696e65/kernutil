#ifndef _HAVE_MODES_H
#define _HAVE_MODES_H

#include "helper.h"

#define DEFAULT_KERNEL_PATH "/System/Library/Kernels/kernel"

#define m_READ         (1<<1)
#define m_WRITE        (1<<2)

#define m_SYMBOL       (1<<3)
#define m_ADDRESS      (1<<4)
#define m_FORMAT       (1<<5)
#define m_SET_KSLIDE   (1<<6)

#define m_FIND_KSLIDE  (1<<7)

void
kernutil_read(mach_port_t tfp0,
              uint64_t kaddr,
              uint8_t mode,
              size_t count,
              uint8_t *format,
              size_t format_size);

void
kernutil_write(mach_port_t tfp0,
               uint64_t kaddr);

void
kernutil_find_kslide(mach_port_t tfp0, uint64_t kernelbase);

#endif