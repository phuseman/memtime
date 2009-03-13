#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <mach/mach.h>
#include <mach/task.h>
#include <mach/mach_error.h>
#include <mach/shared_memory_server.h>

#include "machdep.h"

static task_t       task = MACH_PORT_NULL;

static task_t
recv_task_port (mach_port_t notify_port)
{
    kern_return_t err;
    struct {
        mach_msg_header_t header;
        mach_msg_body_t body;
        mach_msg_port_descriptor_t task_port;
        mach_msg_trailer_t trailer;
    } msg;

    err = mach_msg (&msg.header, MACH_RCV_MSG,
                    0, sizeof msg, notify_port,
                    MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    if (err != KERN_SUCCESS) {
        mach_error ("mach_msg failed:", err);
        return MACH_PORT_NULL;
    }

    return msg.task_port.name;
}

int
send_task_port()
{
    kern_return_t err;
    mach_port_t bs_port = MACH_PORT_NULL;
    struct {
        mach_msg_header_t header;
        mach_msg_body_t body;
        mach_msg_port_descriptor_t task_port;
    } msg;

    err = task_get_bootstrap_port (mach_task_self(), &bs_port);
    if (err != KERN_SUCCESS) {
        mach_error ("task_get_bootstrap_port failed:", err);
        return -1;
    }

    msg.header.msgh_remote_port = bs_port;
    msg.header.msgh_local_port = MACH_PORT_NULL;
    msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0) |
        MACH_MSGH_BITS_COMPLEX;
    msg.header.msgh_size = sizeof msg;

    msg.body.msgh_descriptor_count  = 1;
    msg.task_port.name              = mach_task_self();
    msg.task_port.disposition       = MACH_MSG_TYPE_COPY_SEND;
    msg.task_port.type              = MACH_MSG_PORT_DESCRIPTOR;

    err = mach_msg_send (&msg.header);
    if (err != KERN_SUCCESS) {
        mach_error ("mach_msg_send failed:", err);
        return -1;
    }
    return 0;
}

pid_t
sampling_fork()
{
    kern_return_t err;
    mach_port_t notify_port = MACH_PORT_NULL;
    err = mach_port_allocate (mach_task_self(),
                              MACH_PORT_RIGHT_RECEIVE,
                              &notify_port);
    if (err != KERN_SUCCESS) {
        mach_error ("mach_port_allocate failed:", err);
        return -1;
    }

    err = mach_port_insert_right (mach_task_self(),
                                  notify_port,
                                  notify_port,
                                  MACH_MSG_TYPE_MAKE_SEND);
    if (err != KERN_SUCCESS) {
        mach_error("mach_port_insert_right failed:", err);
        return -1;
    }

    err = task_set_bootstrap_port (mach_task_self(), notify_port);
    if (err != KERN_SUCCESS) {
        mach_error ("task_set_bootstrap_port failed:", err);
        return -1;
    }

    pid_t pid;
    switch (pid = fork()) {
    case -1:
        return pid;

    case 0:
        if (send_task_port() == -1)
            return -1;
        break;

    default:
        task = recv_task_port (notify_port);
        if (task == MACH_PORT_NULL)
            return -1;
        break;
    }

    return pid;
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
