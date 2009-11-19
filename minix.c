/* -*- mode: C; c-file-style: "k&r"; -*-
 *---------------------------------------------------------------------------*
 *
 * Copyright (c) 2009, Jeroen Ketema
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 *---------------------------------------------------------------------------*/

#include <minix/config.h>
#include <minix/const.h>
#include <minix/ipc.h>
#include <minix/sysinfo.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <alloca.h>
#include <stdlib.h>
#include <stdio.h>
#include <timers.h>
#include <unistd.h>

#include "/usr/src/kernel/arch/i386/include/archtypes.h"
#include "/usr/src/kernel/const.h"
#include "/usr/src/kernel/type.h"
#include "/usr/src/kernel/proc.h"
#include "/usr/src/servers/pm/mproc.h"

#include "machdep.h"

static pid_t pid;
static u32_t sys_hz; // must be initialized by sampling_fork before wait4

/*
 * Fake wait4 on Minix (as far as we need it). Unfortunately the times
 * call cannot differentiate between different children of our memtime
 * process.
 */
pid_t wait4(pid_t pid, int *status, int options, struct rusage *rusage)
{
     pid_t pid_ret = waitpid(pid, status, options);

     if (rusage != NULL && pid_ret == pid) {
	  struct tms tms;
	  int sec, usec;

	  times(&tms);

	  sec = tms.tms_cutime / sys_hz;
	  usec = (tms.tms_cutime - (sec * (long)sys_hz)) * (1000000 / sys_hz);
	  rusage->ru_utime.tv_sec = sec;
	  rusage->ru_utime.tv_usec = usec;

	  sec = tms.tms_cstime / sys_hz;
	  usec = (tms.tms_cstime - (sec * (long)sys_hz)) * (1000000 / sys_hz);
	  rusage->ru_stime.tv_sec = sec;
	  rusage->ru_stime.tv_usec = usec;
     }

     return pid_ret;
}


pid_t sampling_fork()
{
     pid = fork();
     getsysinfo_up(PM_PROC_NR, SIU_SYSTEMHZ, sizeof(sys_hz), &sys_hz);

     if (pid == 0) {
	  uid_t uid = getuid();
	  gid_t gid = getgid();
	  if (seteuid(uid) != 0) {
	       perror("seteuid failed");
	       exit(EXIT_FAILURE);
	  }
	  if (setegid(gid) != 0) {
	       perror("setegid failed");
	       exit(EXIT_FAILURE);
	  }
     }

     return pid;
}


int get_sample(memtime_info_t *info)
{
     int i;
     struct proc proc[NR_TASKS + NR_PROCS];
     struct mproc mproc[NR_PROCS];

     if (getsysinfo(PM_PROC_NR, SI_KPROC_TAB, proc) < 0)
	  return 0;

     if (getsysinfo(PM_PROC_NR, SI_PROC_TAB, mproc) < 0)
	  return 0;

     for (i = 0; i < NR_PROCS; i++) {
	  if (!isemptyp(&proc[i + NR_TASKS]) && mproc[i].mp_pid == pid)
	       break;
     }

     if (i == NR_PROCS)
	  return 0;

     i+=NR_TASKS;

     info->utime_ms = proc[i].p_user_time * (1000 / sys_hz);
     info->stime_ms = proc[i].p_sys_time * (1000 / sys_hz);

     info->vsize_kb = ((proc[i].p_memmap[T].mem_len
			+ proc[i].p_memmap[D].mem_len) << CLICK_SHIFT) / 1024;
     info->rss_kb = info->vsize_kb;

     return 1;
}

unsigned long get_time()
{
     struct timeval now;
     struct timezone dummy;
     int rc;

     rc = gettimeofday(&now, &dummy);

     if (rc == -1) {
	  return 0;
     }

     return (now.tv_sec * 1000) + (now.tv_usec / 1000);
}
