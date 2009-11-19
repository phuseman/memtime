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

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <kvm.h>
#include <unistd.h>

#include "machdep.h"

static long pagesize;
static kvm_t *kd;
static pid_t pid;

pid_t sampling_fork()
{
     pid = fork();

     switch (pid) {
     case -1:
     case 0:
          return 0;
     default:
	  pagesize = sysconf(_SC_PAGESIZE);
	  kd = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, NULL);
	  return (kd != NULL) ? pid : -1;
     }
}

int get_sample(memtime_info_t *info)
{
     int count;
     struct kinfo_proc2 *kinfo;

     kinfo = kvm_getproc2(kd, KERN_PROC_PID, pid, sizeof (struct kinfo_proc2),
			  &count);
     if (kinfo == NULL || count != 1)
	  return 0;

     info->utime_ms = (kinfo->p_uutime_sec * 1000)
	  + (kinfo->p_uutime_usec / 1000);
     info->stime_ms = (kinfo->p_ustime_sec * 1000)
	  + (kinfo->p_ustime_usec / 1000);

#if defined(__NetBSD__) && (__NetBSD_Version__ >= 500000000)
     info->vsize_kb = (kinfo->p_vm_msize * pagesize) / 1024;
#else
     info->vsize_kb = ((kinfo->p_vm_tsize + kinfo->p_vm_dsize
			+ kinfo->p_vm_ssize) * pagesize) / 1024;
#endif
     info->rss_kb = (kinfo->p_vm_rssize * pagesize) / 1024;

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

#if defined(__NetBSD__) && (__NetBSD_Version__ >= 500000000)
int set_mem_limit(unsigned long maxbytes)
{
     struct  rlimit rl;

     rl.rlim_cur=maxbytes;
     rl.rlim_max=maxbytes;
     return setrlimit(RLIMIT_AS,&rl);
}
#endif

int set_cpu_limit(unsigned long maxseconds)
{
     struct  rlimit rl;

     rl.rlim_cur=maxseconds;
     rl.rlim_max=maxseconds;
     return setrlimit(RLIMIT_CPU,&rl);
}
