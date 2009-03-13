/* This file is adapted from code originally supplied by Apple Computer,
 * Inc.  The Berkeley Open Infrastructure for Network Computing project
 * has modified the original code and made additions as of September 22,
 * 2006.  The original Apple Public Source License statement appears
 * below: */

/*
 * Copyright (c) 2002-2004 Apple Computer, Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

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

#define CHECK_MACH_ERROR(err, msg)                                      \
    if (err != KERN_SUCCESS) {                                          \
        mach_error (msg, err);                                          \
        return -1;                                                      \
    }                                                                   \


static int
setup_bootstrap_port (mach_port_t *bs_port)
{
    kern_return_t       err;
    mach_port_t         notify_port = MACH_PORT_NULL;
    err = mach_port_allocate (mach_task_self (),
                              MACH_PORT_RIGHT_RECEIVE, &notify_port);
    CHECK_MACH_ERROR (err, "mach_port_allocate failed:");

    err = mach_port_insert_right (mach_task_self (),
                                  notify_port,
                                  notify_port,
                                  MACH_MSG_TYPE_MAKE_SEND);
    CHECK_MACH_ERROR (err, "mach_port_insert_right failed:");

    err = task_set_bootstrap_port (mach_task_self (), notify_port);
    CHECK_MACH_ERROR (err, "task_set_bootstrap_port failed:");

    *bs_port = notify_port;
    return 0;
}

static int
recv_port (mach_port_t notify_port, mach_port_t *port)
{
    kern_return_t       err;
    struct {
        mach_msg_header_t          header;
        mach_msg_body_t            body;
        mach_msg_port_descriptor_t task_port;
        mach_msg_trailer_t         trailer;
    } msg;

    err = mach_msg (&msg.header, MACH_RCV_MSG,
                    0, sizeof msg, notify_port,
                    MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    CHECK_MACH_ERROR (err, "mach_msg failed:");

    *port = msg.task_port.name;
    return 0;
}

static int
send_port (mach_port_t port)
{
    kern_return_t       err;
    mach_port_t         bs_port = MACH_PORT_NULL;
    struct {
        mach_msg_header_t          header;
        mach_msg_body_t            body;
        mach_msg_port_descriptor_t task_port;
    } msg;

    err = task_get_bootstrap_port (mach_task_self (), &bs_port);
    CHECK_MACH_ERROR (err, "task_get_bootstrap_port failed:");

    msg.header.msgh_remote_port = bs_port;
    msg.header.msgh_local_port = MACH_PORT_NULL;
    msg.header.msgh_bits = MACH_MSGH_BITS (MACH_MSG_TYPE_COPY_SEND, 0) |
        MACH_MSGH_BITS_COMPLEX;
    msg.header.msgh_size = sizeof msg;

    msg.body.msgh_descriptor_count = 1;
    msg.task_port.name = port;
    msg.task_port.disposition = MACH_MSG_TYPE_COPY_SEND;
    msg.task_port.type = MACH_MSG_PORT_DESCRIPTOR;

    err = mach_msg_send (&msg.header);
    CHECK_MACH_ERROR (err, "mach_msg_send failed:");

    return 0;
}

pid_t
sampling_fork ()
{
    mach_port_t         notify_port = MACH_PORT_NULL;

    if (setup_bootstrap_port (&notify_port) != 0) {
        return -1;
    }

    pid_t               pid;
    switch (pid = fork ()) {
    case -1:
        return pid;

    case 0:
        if (send_port (mach_task_self ()) != 0)
            return -1;
        break;

    default:
        if (recv_port (notify_port, &task) != 0)
            return -1;
        break;
    }

    return pid;
}

int
get_sample (memtime_info_t *info)
{
    struct task_basic_info ti;
    mach_msg_type_number_t ti_count = TASK_BASIC_INFO_COUNT;
    task_info (task, TASK_BASIC_INFO, (task_info_t)&ti, &ti_count);

    info->rss_kb = ti.resident_size / 1024UL;
    unsigned long int      vmsize_bytes = ti.virtual_size;
#if 1
    vm_address_t           address;
    vm_size_t              size;
    mach_msg_type_number_t count;
    mach_port_t            object_name;
    vm_region_top_info_data_t r_info;

    /*
     * Iterate through the VM regions of the process and determine
     * the amount of memory of various types it has mapped.
     */
    for (address = 0; ; address += size) {
        /* Get memory region. */
        count = VM_REGION_TOP_INFO_COUNT;
        if (vm_region (task, &address, &size,
                       VM_REGION_TOP_INFO, (vm_region_info_t)&r_info,
                       &count, &object_name) != KERN_SUCCESS) {
            /* No more memory regions. */
            break;
        }

        if (address >= GLOBAL_SHARED_TEXT_SEGMENT
            && address < (GLOBAL_SHARED_DATA_SEGMENT
                          + SHARED_DATA_REGION_SIZE)) {
            /* This region is private shared. */

            /*
             * Check if this process has the globally shared
             * text and data regions mapped in.  If so, adjust
             * virtual memory size and exit loop.
             */
            if (r_info.share_mode == SM_EMPTY) {
                vm_region_basic_info_data_64_t b_info;

                count = VM_REGION_BASIC_INFO_COUNT_64;
                if (vm_region_64 (task, &address,
                                  &size, VM_REGION_BASIC_INFO,
                                  (vm_region_info_t)&b_info, &count,
                                  &object_name) != KERN_SUCCESS) {
                    break;
                }

                if (b_info.reserved) {
                    vmsize_bytes -=
                        (SHARED_TEXT_REGION_SIZE +
                         SHARED_DATA_REGION_SIZE);
                    break;
                }
            }
        }
    }
#else
    if (vmsize_bytes > SHARED_TEXT_REGION_SIZE +
    SHARED_DATA_REGION_SIZE)
    vmsize_bytes -= SHARED_TEXT_REGION_SIZE + SHARED_DATA_REGION_SIZE;
#endif
    info->vsize_kb = vmsize_bytes / 1024UL;

    info->utime_ms = ti.user_time.seconds * 1000 +
        ti.user_time.microseconds / 1000;
    info->stime_ms = ti.system_time.seconds * 1000 +
        ti.system_time.microseconds / 1000;

    return 1;
}

unsigned long
get_time ()
{
    struct timeval      now;
    struct timezone     dummy;

    if (gettimeofday (&now, &dummy) == -1) {
        perror ("gettimeofday");
        return 0;
    }

    return (now.tv_sec * 1000) + (now.tv_usec / 1000);
}

int
set_mem_limit (unsigned long maxbytes)
{
    struct rlimit       rl;
    long int            softlimit = (long int)maxbytes * 95 / 100;
    rl.rlim_cur = softlimit;
    rl.rlim_max = maxbytes;
    return setrlimit (RLIMIT_RSS, &rl);
}

int
set_cpu_limit (long int maxseconds)
{
    struct rlimit       rl;
    rl.rlim_cur = maxseconds;
    rl.rlim_max = maxseconds;
    return setrlimit (RLIMIT_CPU, &rl);
}
