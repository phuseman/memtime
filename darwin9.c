#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <mach/task.h>
#include <mach/mach_init.h>
#include <mach/shared_memory_server.h>

#include "machdep.h"

static task_t       task = MACH_PORT_NULL;

int
init_machdep (pid_t pid)
{
    if (task_for_pid (current_task (), pid, &task) != KERN_SUCCESS) {
        perror ("task_for_pid");
        return 0;
    }

    return 1;
}

int
get_sample (memtime_info_t * info)
{
    struct task_basic_info task_basic_info;
    mach_msg_type_number_t task_basic_info_count = TASK_BASIC_INFO_COUNT;
    task_info (task, TASK_BASIC_INFO, (task_info_t)&task_basic_info,
               &task_basic_info_count);

    info->rss_kb = task_basic_info.resident_size / 1024UL;
    unsigned long int   vmsize_bytes = task_basic_info.virtual_size;
    if (vmsize_bytes > SHARED_TEXT_REGION_SIZE + SHARED_DATA_REGION_SIZE)
        vmsize_bytes -= SHARED_TEXT_REGION_SIZE + SHARED_DATA_REGION_SIZE;
    info->vsize_kb = vmsize_bytes / 1024UL;

    struct rusage       rusage;
    if (getrusage (RUSAGE_SELF, &rusage) == -1) {
        perror ("getrusage");
        return 0;
    }

    info->utime_ms = rusage.ru_utime.tv_sec * 1000 +
        rusage.ru_utime.tv_usec / 1000;
    info->stime_ms = rusage.ru_stime.tv_sec * 1000 +
        rusage.ru_stime.tv_usec / 1000;

    return 1;
}

unsigned long
get_time ()
{
    struct timeval      now;
    struct timezone     dummy;

    if (gettimeofday (&now, &dummy) == -1) {
        perror ("getrusage");
        return 0;
    }
    
    return (now.tv_sec * 1000) + (now.tv_usec / 1000);
}

int
set_mem_limit (unsigned long maxbytes)
{
    struct rlimit       rl;
    long int            softlimit = (long int)maxbytes * 0.95;
    rl.rlim_cur = softlimit;
    rl.rlim_max = maxbytes;
    return setrlimit (RLIMIT_AS, &rl);
}

int
set_cpu_limit (long int maxseconds)
{
    struct rlimit       rl;
    rl.rlim_cur = maxseconds;
    rl.rlim_max = maxseconds;
    return setrlimit (RLIMIT_CPU, &rl);
}
