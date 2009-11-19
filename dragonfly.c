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

#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/kinfo.h>
#include <kvm.h>
#include <fcntl.h>
#include <unistd.h>
#include <paths.h>
#include <limits.h>

#include "machdep.h"

static long pagesize;
static kvm_t *kd;
static pid_t pid;

pid_t sampling_fork()
{
     char errbuf[_POSIX2_LINE_MAX];

     pid = fork();

     switch (pid) {
     case -1:
     case 0:
          return pid;
     default:
	  pagesize = sysconf(_SC_PAGESIZE);
	  kd = kvm_openfiles(_PATH_DEVNULL, _PATH_DEVNULL, NULL, O_RDONLY,
			     errbuf);
	  return (kd != NULL) ? pid : -1;
     }
}

int get_sample(memtime_info_t *info)
{
     int count;
     struct kinfo_proc *kinfo;

     kinfo = kvm_getprocs(kd, KERN_PROC_PID, pid, &count);
     if (kinfo == NULL || count != 1)
	  return 0;

     info->utime_ms = kinfo->kp_lwp.kl_uticks / 1000;
     info->stime_ms = kinfo->kp_lwp.kl_sticks / 1000;

     info->vsize_kb = kinfo->kp_vm_map_size / 1024;
     info->rss_kb = (kinfo->kp_vm_rssize * pagesize) / 1024;

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

int set_mem_limit(unsigned long maxbytes)
{
     struct  rlimit rl;

     rl.rlim_cur=maxbytes;
     rl.rlim_max=maxbytes;
     return setrlimit(RLIMIT_AS,&rl);
}

int set_cpu_limit(unsigned long maxseconds)
{
     struct  rlimit rl;

     rl.rlim_cur=maxseconds;
     rl.rlim_max=maxseconds;
     return setrlimit(RLIMIT_CPU,&rl);
}
