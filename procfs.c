#include "procfs.h"

#include "log.h"

#include <stdio.h>
#include <windows.h>

struct ProcfsMapIter
{
    FILE* file;
    const char* filter;
    struct ProcfsMapEntry entry;
    char* line;
    int line_buf_size;
};

struct ProcfsMapIter* procfs_map_iter_new(int pid, const char* filter)
{
    char line[100];
    sprintf(line, "/proc/%d/maps", pid);

    FILE* f = fopen(line, "r");
    if (!f)
    {
        return NULL;
    }

    struct ProcfsMapIter* iter = malloc(sizeof(struct ProcfsMapIter));
    iter->file = f;
    iter->filter = filter;
    iter->line_buf_size = 256;
    iter->line = malloc(iter->line_buf_size);
    return iter;
}

void procfs_map_iter_free(struct ProcfsMapIter* iter)
{
    fclose(iter->file);
    free(iter->line);
    free(iter);
}

struct ProcfsMapEntry* procfs_map_iter_next(struct ProcfsMapIter* iter)
{
    int line_loc = 0;

    while (1)
    {
        int sym = fgetc(iter->file);
        if (sym == EOF)
        {
            break;
        }

        if (line_loc >= iter->line_buf_size - 1)
        {
            iter->line_buf_size *= 2;
            iter->line = realloc(iter->line, iter->line_buf_size);
        }

        iter->line[line_loc++] = sym;
        if (sym == '\n')
        {
            iter->line[line_loc - 1] = '\0';
            line_loc = 0;

            if (!strstr(iter->line, iter->filter))
                continue;

            char* dash = strchr(iter->line, '-');
            if (!dash)
                continue;
            char* space = strchr(iter->line, ' ');
            if (!space)
                continue;

            char* perms = space + 1;
            char* offsetStart = perms + 5;
            char* offsetEnd = strchr(offsetStart, ' ');
            if (!offsetEnd)
                continue;

            char tmp[32] = {0};

            strncpy(tmp, iter->line, dash - iter->line);
            tmp[dash - iter->line] = '\0';
            iter->entry.start = strtoull(tmp, NULL, 16);

            strncpy(tmp, dash + 1, space - dash - 1);
            tmp[space - dash - 1] = '\0';
            iter->entry.end = strtoull(tmp, NULL, 16);

            strncpy(tmp, offsetStart, offsetEnd - offsetStart);
            tmp[offsetEnd - offsetStart] = '\0';
            uint32_t offset = strtoul(tmp, NULL, 16);
            iter->entry.offset = offset;

            memcpy(iter->entry.perms, perms, 4);

            iter->entry.pathname = strchr(offsetEnd, '/');

            return &iter->entry;
        }
    }

    return NULL;
}

struct ProcfsProcessesIter
{
    WIN32_FIND_DATA findData;
    HANDLE hFind;
};

struct ProcfsProcessesIter* procfs_processes_iter_new()
{
    struct ProcfsProcessesIter* iter = malloc(sizeof(struct ProcfsProcessesIter));
    iter->hFind = FindFirstFile("/proc/*", &iter->findData);
    if (iter->hFind == INVALID_HANDLE_VALUE)
    {
        free(iter);
        log("Failed to open /proc\n");
        return NULL;
    }

    return iter;
}

void procfs_processes_iter_free(struct ProcfsProcessesIter* iter)
{
    FindClose(iter->hFind);
    free(iter);
}

int procfs_processes_iter_next(struct ProcfsProcessesIter* iter)
{
    int result = 0;
    do
    {
        if (result)
            break;

        if (!(iter->findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            continue;

        result = atoi(iter->findData.cFileName);
    } while (FindNextFile(iter->hFind, &iter->findData) != 0);

    return result;
}
