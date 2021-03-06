#ifndef _HAVE_HELPER_H
#define _HAVE_HELPER_H

#include <stdio.h>          // printf
#include <stdlib.h>         // malloc

#include <fcntl.h>          // O_RDONLY
#include <sys/stat.h>       // struct stat
#include <sys/mman.h>       // PROT_READ

#include <string.h>         // strcmp, memmem

#include <mach-o/loader.h>  // struct load_command
#include <mach-o/nlist.h>   // struct nlist_64

#include <mach/mach.h>      // mach_port_allocate, mach_port_insert_right

#include <CoreFoundation/CoreFoundation.h>


#define LOG(x...)        printf(x);
#define ERR(x...) do { \
  LOG("Error:    " x); \
  LOG("\n"); \
  LOG("Location: %s:%u -> %s()", \
      __FILE__, __LINE__, __FUNCTION__); \
  LOG("\n"); \
  exit(1); \
} while (0)

typedef struct memory_map
{
    void *map;
    size_t sz;
} memory_map_t;

typedef memory_map_t kernel_map_t;
memory_map_t *map_file(const char *path);

struct load_command *find_load_command(memory_map_t *mapping, uint32_t cmd);

uint64_t find_symbol_address(memory_map_t *mapping, const char *symbol_name);

void hexdump_with_offset(void *address, uint64_t length, uint64_t offset_address);
void hexdump(void *address, uint64_t length);

void obtain_tfp0(mach_port_t *addr);

void kread(mach_port_t tfp0, vm_address_t kaddr, vm_address_t uaddr, uint32_t size);
void kwrite_1B(mach_port_t tfp0, vm_address_t kaddr, uint8_t value);
uint8_t *kread_c_string(mach_port_t tfp0, vm_address_t kaddr);
uint64_t find_kernel_base(kernel_map_t *mapping);

#endif