#include "helper.h"

/* Dump the raw data with offset specified as the last parameter */
void
hexdump_with_offset(void *address, uint64_t length, uint64_t offset_address)
{
    uint64_t i;

    uint8_t buff[17];
    uint8_t *ptr = (uint8_t *)address;

    for (i = 0; i < length; i++)
    {
        if ((i % 16) == 0)
        {
            /* Skip the first line*/
            if (i != 0)
                printf ("  %s\n", buff);
            /* Offset */
            printf ("[0x%016llx] ", i + offset_address);
        }
        printf (" %02x", ptr[i]);

        /* Print in the third column the ASCII representation */
        if ((ptr[i] < 0x20) || (ptr[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = ptr[i];
        buff[(i % 16) + 1] = '\0';
    }
    /* Align to 16 chars */
    while ((i % 16) != 0)
    {
        printf ("   ");
        i++;
    }
    /* ASCII representation of the last line */
    printf ("  %s\n", buff);
}


/* Dump the raw data with the zero offset */
void
hexdump(void *address, uint64_t length)
{
    hexdump_with_offset(address, length, 0);
}


/* Try to obtain tfp0 via HSP4 */
void
obtain_tfp0(mach_port_t *addr)
{
    host_get_special_port(mach_host_self(), HOST_LOCAL_NODE, 4, addr);
    if (*addr == 0)
    {
        ERR("Failed to obtain tfp0.");
    }
}


/* Read size bytes from the kernel memory to uaddr buffer */
void
kread(mach_port_t tfp0, vm_address_t kaddr, vm_address_t uaddr, uint32_t size)
{
    vm_size_t outsize;
    vm_read_overwrite(tfp0, kaddr, size, (vm_address_t) uaddr, &outsize);
    if (size != outsize)
        ERR("Intended to read: %d, actually read: %d. "
            "Maybe reading too much?", size, (uint32_t) outsize);
}


uint8_t *
kread_c_string(mach_port_t tfp0, vm_address_t kaddr)
{
    uint8_t *output = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    void *start = output;

    /* Read the kernel memory until you find a null terminated string */
    while (1)
    {
        uint32_t size = 1;
        kread(tfp0, kaddr, (vm_address_t) output, size);
        if (*output == 0)
            return start;
        kaddr++;
        output++;
    }
}


/* Store 1 byte value to the kernel memory */
void
kwrite_1B(mach_port_t tfp0, vm_address_t kaddr, uint8_t value)
{
    vm_write(tfp0, kaddr, (vm_offset_t) &value, 1);
}


memory_map_t *
map_file(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return 0;

    struct stat sb;
    fstat(fd, &sb);

    void *map = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);

    kernel_map_t *km = (kernel_map_t *) malloc(sizeof(kernel_map_t));
    km->map  = map;
    km->sz = sb.st_size;
    return km;
}


uint64_t
find_symbol_address(memory_map_t *mapping, const char *symbol_name)
{
    void *symbol_table = NULL, *string_table = NULL;
    uint32_t nsymbols = 0;

    struct mach_header_64 *mh = mapping->map;
    struct symtab_command *symtab_cmd = (struct symtab_command *)find_load_command(mapping, LC_SYMTAB);

    if (!symtab_cmd)
        return 0;

    symbol_table = ((void *)mh + symtab_cmd->symoff);
    string_table = ((void *)mh + symtab_cmd->stroff);
    nsymbols = symtab_cmd->nsyms;

    struct nlist_64 *entry = (struct nlist_64 *)symbol_table;

    for (uint32_t i = 0; i < nsymbols; ++i)
    {
        if (strcmp(string_table + (entry->n_un.n_strx), symbol_name) == 0)
        {
            return entry->n_value;
        }
        entry = ((void *)entry + sizeof(struct nlist_64));
    }

    ERR("Symbol '%s' not found, quiting.", symbol_name);
    exit(1);
}


struct load_command *
find_load_command(memory_map_t *mapping, uint32_t cmd)
{
    struct mach_header_64 *mh = mapping->map;
    struct load_command *lc, *flc;

    lc = (struct load_command *)((uint64_t)mh + sizeof(struct mach_header_64));

    for (int i = 0; i < mh->ncmds; ++i)
    {
        if (lc->cmd == cmd)
        {
            return lc;
        }
        lc = (struct load_command *)((uint64_t)lc + (uint64_t)lc->cmdsize);
    }
    ERR("[-] load_command '%x' not found, quiting.", cmd);
}

struct segment_command_64 *find_segment_64(struct mach_header_64 *mh, const char *segname)
{
    struct load_command *lc;
    struct segment_command_64 *s;
    lc = (struct load_command *)((uint64_t)mh + sizeof(struct mach_header_64));

    for (int i = 0; i < mh->ncmds; ++i)
    {
        if (lc->cmd == LC_SEGMENT_64)
        {
            s = (struct segment_command_64 *)lc;
            if (!strcmp(s->segname, segname))
            {
                return s;
            }
        }
        lc = (struct load_command *)((uint64_t)lc + (uint64_t)lc->cmdsize);
    }
    ERR("[-] segment '%s' not found, quiting.", segname);
}

uint64_t find_kernel_base(kernel_map_t *mapping)
{
    struct mach_header_64 *mh = mapping->map;
    struct segment_command_64 *text = find_segment_64(mh, SEG_TEXT);
    return (uint64_t)text->vmaddr;
}