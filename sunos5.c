/* -*- mode: C; c-file-style: "k&r"; -*-
 *---------------------------------------------------------------------------*
 *
 * Copyright (c) 2000, Johan Bengtsson
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

#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>
#include <fcntl.h>
#include <procfs.h>

#include <stdio.h>
#include <string.h>

#include "machdep.h"

static int psinfo_fd = -1;
static int pstatus_fd = -1;

int init_machdep(pid_t process)
{
     char filename[64];
     sprintf(filename, "/proc/%d/psinfo", (int)process);
     psinfo_fd = open(filename, O_RDONLY | O_RSYNC);

     sprintf(filename, "/proc/%d/status", (int)process);
     pstatus_fd = open(filename, O_RDONLY | O_RSYNC);

     return (psinfo_fd != -1 && pstatus_fd != -1);
}

int get_sample(struct memtime_info *info)
{
     struct psinfo pinfo;
     struct pstatus sinfo;
     int rc;

     rc = pread(psinfo_fd, &pinfo, sizeof(struct psinfo), 0);

     if (rc == -1) {
	  return 0;
     }

     rc = pread(pstatus_fd, &sinfo, sizeof(struct pstatus), 0);

     if (rc == -1) {
	  return 0;
     }

     info->rss_kb = pinfo.pr_rssize;
     info->vsize_kb = pinfo.pr_size;

     info->utime_ms = ((1000 * sinfo.pr_utime.tv_sec) 
		       + (sinfo.pr_utime.tv_nsec / 1000000));
     info->stime_ms = ((1000 * sinfo.pr_stime.tv_sec) 
		       + (sinfo.pr_stime.tv_nsec / 1000000));

     return 1;
}

unsigned int get_time()
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

int set_mem_limit(long int maxbytes)
{
	struct  rlimit rl;
	rl.rlim_cur=maxbytes; 
	rl.rlim_max=maxbytes;
	return setrlimit(RLIMIT_VMEM,&rl);
}

int set_cpu_limit(long int maxseconds)
{
	struct  rlimit rl;
	rl.rlim_cur=maxseconds; 
	rl.rlim_max=maxseconds;
	return setrlimit(RLIMIT_CPU,&rl);

}
