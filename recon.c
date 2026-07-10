#include "recon.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "elf.h"
#include "log.h"
#include "procfs.h"

int find_pid(const char* canary)
{
    struct ProcfsProcessesIter* ps = procfs_processes_iter_new();
    if (!ps)
    {
        log("Failed to create processes iterator\n");
        return 0;
    }

    int pid;
    while ((pid = procfs_processes_iter_next(ps)))
    {
        struct ProcfsMapIter* iter = procfs_map_iter_new(pid, canary);
        if (!iter)
        {
            continue;
        }

        bool canary_found = !!procfs_map_iter_next(iter);

        procfs_map_iter_free(iter);

        if (canary_found)
            break;
    }

    procfs_processes_iter_free(ps);

    log("Found pid: %d\n", pid);
    return pid;
}

struct Section
{
    uint64_t start;
    uint64_t end;
    uint64_t offset;
    void* data;
};

struct Sections
{
    struct Section* sections;
    size_t count;
    size_t capacity;
};

static void sections_init(struct Sections* ss)
{
    ss->sections = malloc(sizeof(struct Section) * 16);
    ss->count = 0;
    ss->capacity = 16;
}

static void sections_free(struct Sections* ss)
{
    free(ss->sections);
}

static void sections_add(struct Sections* ss, uint64_t start, uint64_t end, uint64_t offset)
{
    if (ss->count >= ss->capacity)
    {
        ss->capacity *= 2;
        ss->sections = realloc(ss->sections, sizeof(struct Section) * ss->capacity);
    }

    ss->sections[ss->count].start = start;
    ss->sections[ss->count].end = end;
    ss->sections[ss->count].offset = offset;
    ss->sections[ss->count].data = NULL;
    ss->count++;
}

static struct Section* sections_find(struct Sections* ss, uint64_t offset, uint64_t size)
{
    uint64_t end = offset + size;
    for (size_t i = 0; i < ss->count; i++)
    {
        struct Section* s = &ss->sections[i];

        uint64_t section_start = s->offset;
        uint64_t section_end   = s->offset + (s->end - s->start);
        if (section_start <= offset && end <= section_end)
        {
            return s;
        }
    }

    return NULL;
}

static const char* safe_str(char* buf, size_t size, size_t offset, size_t limit)
{
    if (offset >= size)
        return NULL;

    if (limit > size)
    {
        for (size_t i = offset; i < size; i++)
        {
            if (buf[i] == '\0')
                return buf + offset;
        }

        return NULL;
    }

    return buf + offset;
}

struct LibcFunctions find_libc(int pid)
{
    struct LibcFunctions ret = {0};

    char libcPath[256];
    uint64_t elfloc = 0;

    struct Section xloc;
    struct Sections sections;

    // Discover the location of libc.so.6 in the target process's memory space
    {
        struct ProcfsMapIter* iter = procfs_map_iter_new(pid, "libc.so.6");
        if (!iter)
        {
            return ret;
        }

        sections_init(&sections);

        struct ProcfsMapEntry* entry;
        while ((entry = procfs_map_iter_next(iter)))
        {
            if (!entry->pathname)
            {
                log("Found libc: %llx %llx-%llx %.4s (no pathname?)\n", entry->offset, entry->start, entry->end, entry->perms);
                continue;
            }

            log("Found libc: %llx %llx-%llx %.4s\n", entry->offset, entry->start, entry->end, entry->perms);
            if (0 == entry->offset)
            {
                elfloc = entry->start;
                strncpy(libcPath, entry->pathname, sizeof(libcPath) - 1);
                libcPath[255] = '\0';
            }

            if (entry->perms[2] == 'x')
            {
                xloc.start  = entry->start;
                xloc.end    = entry->end;
                xloc.offset = entry->offset;
            }
            if (entry->perms[0] == 'r'
             && entry->perms[1] != 'w'
             && entry->perms[2] != 'x')
            {
                sections_add(&sections, entry->start, entry->end, entry->offset);
            }
        }

        procfs_map_iter_free(iter);
    }

    if (!elfloc || !xloc.start || !xloc.end)
    {
        log("Failed to find libc.so.6 in process %d\n", pid);
        sections_free(&sections);
        return ret;
    }

    FILE* ram = NULL;
    Elf64_Shdr* shdrs = NULL;
    Elf64_Sym* sym_table = NULL;
    char* str_table = NULL;

    // Analyze and prove that we have libc decently mapped by reading the ELF header and section headers
    {
        char path[64] = {0};
        snprintf(path, sizeof(path), "/proc/%d/mem", pid);
        ram = fopen(path, "rb");
        if (!ram)
        {
            log("Failed to open /proc/%d/mem\n", pid);
            goto fini;
        }
    }

    // Start reading ELF
    Elf64_Ehdr ehdr;
    {
        fseeko64(ram, elfloc, SEEK_SET);
        if (1 != fread(&ehdr, sizeof(ehdr), 1, ram))
        {
            log("Failed to read ELF header from libc.so.6 in process %d\n", pid);
            goto fini;
        }

        if (ehdr.e_ident[0] != 0x7f || ehdr.e_ident[1] != 'E' || ehdr.e_ident[2] != 'L' || ehdr.e_ident[3] != 'F')
        {
            log("Failed to read ELF header from libc.so.6 in process %d\n", pid);
            goto fini;
        }
    }

    // shdrs are not actually mapped in memory so we have to open the file and read them from disk
    shdrs = malloc(sizeof(Elf64_Shdr) * ehdr.e_shnum);
    {
        FILE* libc = fopen(libcPath, "rb");
        if (!libc)
        {
            log("Failed to open libc.so.6 at '%s'\n", libcPath);
            goto fini;
        }

        fseeko64(libc, ehdr.e_shoff, SEEK_SET);
        size_t amt = fread(shdrs, sizeof(Elf64_Shdr), ehdr.e_shnum, libc);
        fclose(libc);

        if (amt != ehdr.e_shnum)
        {
            log("Failed to read section headers from libc.so.6\n");
            goto fini;
        }
    }

    // as raw pointers inside 'shdrs'
    Elf64_Shdr* dynsym = NULL;
    Elf64_Shdr* strtab = NULL;
    for (int i = 0; i < ehdr.e_shnum; i++)
    {
        Elf64_Shdr* shdr = &shdrs[i]; 
        if (!(shdr->sh_flags & SHF_ALLOC))
        {
            continue;
        }

        // TODO: uncba to check the names...
        if (shdr->sh_type == SHT_DYNSYM)
            dynsym = shdr;
        else if (shdr->sh_type == SHT_STRTAB)
            strtab = shdr;
    }

    if (!dynsym || !strtab)
    {
        log("Failed to find dynsym or strtab in libc.so.6\n");
        goto fini;
    }

    // Load in from RAM the dynsym and strtab sections so we can iterate over them and find the symbols we want
    struct Section* dynsymSec = sections_find(&sections, dynsym->sh_offset, dynsym->sh_size);
    struct Section* strtabSec = sections_find(&sections, strtab->sh_offset, strtab->sh_size);
    if (!dynsymSec || !strtabSec)
    {
        log("Failed to find dynsym or strtab in mapped sections\n");
        goto fini;
    }

    sym_table = malloc(dynsym->sh_size);
    fseeko64(ram, dynsymSec->start + (dynsym->sh_offset - dynsymSec->offset), SEEK_SET);
    if (1 != fread(sym_table, dynsym->sh_size, 1, ram))
    {
        log("Failed to read dynsym from process %d\n", pid);
        goto fini;
    }

    str_table = malloc(strtab->sh_size);
    fseeko64(ram, strtabSec->start + (strtab->sh_offset - strtabSec->offset), SEEK_SET);
    if (1 != fread((void*)str_table, strtab->sh_size, 1, ram))
    {
        log("Failed to read strtab from process %d\n", pid);
        goto fini;
    }

    int total_symbols = dynsym->sh_size / sizeof(Elf64_Sym);
    for (int i = 0; i < total_symbols; i++) {
        if (sym_table[i].st_name == 0)
            continue;

        const char *sym_name = safe_str(str_table, strtab->sh_size, sym_table[i].st_name, 16);
        if (!sym_name)
            continue;

        if (strcmp(sym_name, "__errno_location") == 0)
        {
            ret.errno_location = xloc.start + sym_table[i].st_value - xloc.offset;
            if (ret.errno_location > xloc.end)
            {
                log("Found __errno_location outside of libc.so.6 mapping\n");
                ret.errno_location = 0;
            }
        }
        else if (strcmp(sym_name, "syscall") == 0)
        {
            ret.syscall = xloc.start + sym_table[i].st_value - xloc.offset;
            if (ret.syscall > xloc.end)
            {
                log("Found syscall outside of libc.so.6 mapping\n");
                ret.syscall = 0;
            }
        }
    }

fini:
    if (str_table) free(str_table);
    if (sym_table) free(sym_table);
    if (shdrs) free(shdrs);
    if (ram) fclose(ram);
    sections_free(&sections);
    return ret;
}
