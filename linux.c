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
#include <asm/param.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdio.h>
#include <string.h>

#include <sys/resource.h>

#include "machdep.h"

static int proc_fd = -1;


pid_t
sampling_fork()
{
     char filename[255];
     pid_t pid;
     switch (pid = fork()) {
     case -1:
     case 0:
          return pid;
     default:
          snprintf(filename, sizeof filename, "/proc/%d/stat", (int)pid);
          proc_fd = open(filename, O_RDONLY);
          return (proc_fd != -1) ? pid : -1;
     }
}

int get_sample(memtime_info_t *info)
{
     static char buffer[2048];
     char *tmp;
     long i, utime, stime;
     unsigned long vsize, rss;
     int rc;

     lseek(proc_fd, 0, SEEK_SET);
     rc = read(proc_fd, buffer, 2048);

     if (rc == -1)
	  return 0;

     *(buffer + rc) = '\0';

     for (i=0, tmp=buffer; i < 13; i++)
	  tmp = strchr(tmp + 1, ' ');

     sscanf(tmp + 1, "%lu %lu", &utime, &stime);
    
     for (/* empty */; i < 22; i++)
	  tmp = strchr(tmp + 1, ' ');

     sscanf(tmp + 1, "%lu %lu", &vsize, &rss);

     info->utime_ms = utime * (1000 / CLOCKS_PER_SEC);
     info->stime_ms = stime * (1000 / CLOCKS_PER_SEC);

     info->rss_kb = (rss * getpagesize()) / 1024;
     info->vsize_kb = vsize / 1024;

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
	long int softlimit=(long int)maxbytes*0.95;
	rl.rlim_cur=softlimit; 
	rl.rlim_max=maxbytes;
	return setrlimit(RLIMIT_AS,&rl);
}

int set_cpu_limit(long int maxseconds)
{
	struct  rlimit rl;
	rl.rlim_cur=maxseconds; 
	rl.rlim_max=maxseconds;
	return setrlimit(RLIMIT_CPU,&rl);
}
