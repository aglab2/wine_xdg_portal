#include <stdint.h>

struct ProcfsMapEntry
{
    uint64_t start;
    uint64_t end;
    uint64_t offset;
    char perms[4];
    const char* pathname;
};

struct ProcfsMapIter;

struct ProcfsMapIter* procfs_map_iter_new(int pid, const char* filter);
void procfs_map_iter_free(struct ProcfsMapIter* iter);
struct ProcfsMapEntry* procfs_map_iter_next(struct ProcfsMapIter* iter);

struct ProcfsProcessesIter;

struct ProcfsProcessesIter* procfs_processes_iter_new();
void procfs_processes_iter_free(struct ProcfsProcessesIter* iter);
int procfs_processes_iter_next(struct ProcfsProcessesIter* iter);
