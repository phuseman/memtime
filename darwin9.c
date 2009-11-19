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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <mach/mach.h>
#include <mach/task.h>
#include <mach/mach_error.h>
#if defined(HAVE_MACH_SHARED_REGION_H) && HAVE_DECL_SHARED_REGION_SIZE
#include <mach/shared_region.h>
#elif defined(HAVE_MACH_SHARED_MEMORY_SERVER_H)
#include <mach/shared_memory_server.h>
#endif
#include <mach/vm_map.h>
#include <mach/vm_region.h>

#include "machdep.h"

static task_t       child_task = MACH_PORT_NULL;

#define CHECK_MACH_ERROR(err, msg)                                      \
    if (err != KERN_SUCCESS) {                                          \
        mach_error (msg, err);                                          \
        return -1;                                                      \
    }                                                                   \

#ifdef _LP64
#define vm_region vm_region_64
#endif

static int
setup_recv_port (mach_port_t *recv_port)
{
    kern_return_t       err;
    mach_port_t         port = MACH_PORT_NULL;
    err = mach_port_allocate (mach_task_self (),
                              MACH_PORT_RIGHT_RECEIVE, &port);
    CHECK_MACH_ERROR (err, "mach_port_allocate failed:");

    err = mach_port_insert_right (mach_task_self (),
                                  port,
                                  port,
                                  MACH_MSG_TYPE_MAKE_SEND);
    CHECK_MACH_ERROR (err, "mach_port_insert_right failed:");

    *recv_port = port;
    return 0;
}

static int
recv_port (mach_port_t recv_port, mach_port_t *port)
{
    kern_return_t       err;
    struct {
        mach_msg_header_t          header;
        mach_msg_body_t            body;
        mach_msg_port_descriptor_t task_port;
        mach_msg_trailer_t         trailer;
    } msg;

    err = mach_msg (&msg.header, MACH_RCV_MSG,
                    0, sizeof msg, recv_port,
                    MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    CHECK_MACH_ERROR (err, "mach_msg failed:");

    *port = msg.task_port.name;
    return 0;
}

static int
send_port (mach_port_t remote_port, mach_port_t port)
{
    kern_return_t       err;

    struct {
        mach_msg_header_t          header;
        mach_msg_body_t            body;
        mach_msg_port_descriptor_t task_port;
    } msg;

    msg.header.msgh_remote_port = remote_port;
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
    kern_return_t       err;
    mach_port_t         parent_recv_port = MACH_PORT_NULL;
    mach_port_t         child_recv_port = MACH_PORT_NULL;

    if (setup_recv_port (&parent_recv_port) != 0)
        return -1;
    err = task_set_bootstrap_port (mach_task_self (), parent_recv_port);
    CHECK_MACH_ERROR (err, "task_set_bootstrap_port failed:");

    /* This is a very roundabout way to get the child's task port to
     * the parent via a new bootstrap port, AND setting the child's
     * bootstrap port back to the parent's original bootstrap port.
     *
     * I wish the Mach bits would have better documentation.
     */
    pid_t               pid;
    switch (pid = fork ()) {
    case -1:
        err = mach_port_deallocate (mach_task_self(), parent_recv_port);
        CHECK_MACH_ERROR (err, "mach_port_deallocate failed:");
        return pid;
    case 0: /* child */
        err = task_get_bootstrap_port (mach_task_self (), &parent_recv_port);
        CHECK_MACH_ERROR (err, "task_get_bootstrap_port failed:");
        if (setup_recv_port (&child_recv_port) != 0)
            return -1;
        if (send_port (parent_recv_port, mach_task_self ()) != 0)
            return -1;
        if (send_port (parent_recv_port, child_recv_port) != 0)
            return -1;
        if (recv_port (child_recv_port, &bootstrap_port) != 0)
            return -1;
        err = task_set_bootstrap_port (mach_task_self (), bootstrap_port);
        CHECK_MACH_ERROR (err, "task_set_bootstrap_port failed:");
        break;
    default: /* parent */
        err = task_set_bootstrap_port (mach_task_self (), bootstrap_port);
        CHECK_MACH_ERROR (err, "task_set_bootstrap_port failed:");
        if (recv_port (parent_recv_port, &child_task) != 0)
            return -1;
        if (recv_port (parent_recv_port, &child_recv_port) != 0)
            return -1;
        if (send_port (child_recv_port, bootstrap_port) != 0)
            return -1;
        err = mach_port_deallocate (mach_task_self(), parent_recv_port);
        CHECK_MACH_ERROR (err, "mach_port_deallocate failed:");
        break;
    }

    return pid;
}

static int
get_timeinfo (memtime_info_t *info, task_t task, struct task_basic_info *ti)
{
    /* calculate CPU times, adapted from top/libtop.c */

    kern_return_t err;
    thread_array_t thread_table;
    unsigned int table_size, i;
    thread_basic_info_t thi;
    thread_basic_info_data_t thi_data;

    unsigned long utime_ms = ti->user_time.seconds * 1000
	  + ti->user_time.microseconds / 1000;
    unsigned long stime_ms = ti->system_time.seconds * 1000
	  + ti->system_time.microseconds / 1000;

    err = task_threads(task, &thread_table, &table_size);
    CHECK_MACH_ERROR(err, "task_threads failed:");
    thi = &thi_data;

    for (i = 0; i != table_size; ++i) {
	  mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;

	  err = thread_info(thread_table[i], THREAD_BASIC_INFO,
			      (thread_info_t)thi, &count);
	  CHECK_MACH_ERROR(err, "thread_info failed:");
	  if ((thi->flags & TH_FLAGS_IDLE) == 0) {
	       utime_ms += thi->user_time.seconds * 1000
		    + thi->user_time.microseconds / 1000;
	       stime_ms += thi->system_time.seconds * 1000
		    + thi->system_time.microseconds / 1000;
	  }
	  if (task != mach_task_self()) {
	       err = mach_port_deallocate(mach_task_self(), thread_table[i]);
	       CHECK_MACH_ERROR(err, "mach_port_deallocate failed:");
	  }
    }
    err = vm_deallocate(mach_task_self(), (vm_offset_t)thread_table,
                          table_size * sizeof(thread_array_t));
    CHECK_MACH_ERROR(err, "vm_deallocate failed:");

    info->utime_ms = utime_ms;
    info->stime_ms = stime_ms;
    return 0;
}

static int
in_shared_region (vm_address_t address)
{
#if HAVE_DECL_SHARED_REGION_SIZE
    return (address >= SHARED_REGION_BASE
            && address < (SHARED_REGION_BASE + SHARED_REGION_SIZE));
#elif HAVE_DECL_GLOBAL_SHARED_TEXT_SEGMENT
    return (address >= GLOBAL_SHARED_TEXT_SEGMENT
            && address < (GLOBAL_SHARED_DATA_SEGMENT
                          + SHARED_DATA_REGION_SIZE));
#endif
}

static int
get_meminfo (memtime_info_t *info, task_t task, struct task_basic_info *ti)
{
    info->rss_kb = ti->resident_size / 1024UL;
    unsigned long int      vmsize_bytes = ti->virtual_size;
    vm_address_t           address;
    vm_size_t              size = 0, empty = 0;
    mach_msg_type_number_t count;
    mach_port_t            object_name;
    vm_region_top_info_data_t r_info;
    int has_shared_regions = 0;

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

        if (in_shared_region (address)) {
            /* This region is private shared. */

            /*
             * Check if this process has the globally shared
             * text and data regions mapped in.  If so, adjust
             * virtual memory size and exit loop.
             */
            if (!has_shared_regions && r_info.share_mode == SM_EMPTY) {
                vm_region_basic_info_data_64_t b_info;

                count = VM_REGION_BASIC_INFO_COUNT_64;
                if (vm_region_64 (task, &address,
                                  &size, VM_REGION_BASIC_INFO,
                                  (vm_region_info_t)&b_info, &count,
                                  &object_name) != KERN_SUCCESS) {
                    break;
                }

                if (b_info.reserved)
                    has_shared_regions = 1;
            }
            if (r_info.share_mode != SM_PRIVATE)
                continue;
        }

        if (r_info.share_mode == SM_EMPTY)
            empty += size;
    }

    if (has_shared_regions) {
#if HAVE_DECL_SHARED_REGION_SIZE
        vmsize_bytes -= SHARED_REGION_SIZE;
#elif HAVE_DECL_SHARED_TEXT_REGION_SIZE
        vmsize_bytes -= (SHARED_TEXT_REGION_SIZE + SHARED_DATA_REGION_SIZE);
#endif
    }
    vmsize_bytes -= empty;

    info->vsize_kb = vmsize_bytes / 1024UL;
    return 0;
}


int
get_sample (memtime_info_t *info)
{
    task_basic_info_data_t ti;
    mach_msg_type_number_t ti_count = TASK_BASIC_INFO_COUNT;
    if (task_info (child_task, TASK_BASIC_INFO, (task_info_t)&ti, &ti_count)
        != KERN_SUCCESS) {
        return 1;
    }

    int rc = 0;
    if (get_meminfo (info, child_task, &ti) != 0) rc = -1;
    if (get_timeinfo (info, child_task, &ti) != 0) rc = -1;
    return rc;
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
set_cpu_limit (unsigned long maxseconds)
{
    struct rlimit       rl;
    rl.rlim_cur = maxseconds;
    rl.rlim_max = maxseconds;
    return setrlimit (RLIMIT_CPU, &rl);
}
