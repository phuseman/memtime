/* -*- mode: C; c-file-style: k&r; -*-
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

#define _USE_BSD

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include "machdep.h"

int main (int argc, char *argv[] )
{
     struct rusage kid_usage;
     pid_t  kid;
     int    kid_status;
     int    rc;
     int    i, cnt=0, t1, t2;
     int    opt, echo_args = 0;
     long int sample_time=0, delay = 24000, time1=0, time2=1, acc=250;

     unsigned int max_vsize = 0, max_rss = 0;
     unsigned int start, end, elapse;

     struct memtime_info info;

     if (argc < 2) {
	  char *tmp = strrchr(argv[0], '/');       
	  tmp = (tmp ? tmp + 1 : argv[0]);

	  fprintf(stderr, 
		  "%s: usage %s [-t <interval>] [-e] <cmd> [<params>]\n",
		  tmp,tmp);
	  exit(EXIT_FAILURE);
     }

     while ((opt = getopt(argc, argv, "+et:")) != -1) {

	  switch (opt) {
	  case 'e' : 
	       echo_args = 1;
	       break;

	  case 't' :
	       errno = 0;
	       sample_time = strtol(optarg, NULL, 0);
	       if (errno) {
		    perror("Illegal argument to t option");
		    exit(EXIT_FAILURE);
	       }
	       break;
	  }
     }

     if (echo_args) {
	  fprintf(stderr,"Command line: ");
	  for (i = optind; i < argc; i++)
	       fprintf(stderr,"%s ", argv[i]);
	  fprintf(stderr,"\n");
     }

     start = get_time();
    
     switch (kid = fork()) {
	
     case -1 :
	  perror("fork failed");
	  exit(EXIT_FAILURE);
	
     case 0 :	
	  execvp(argv[optind], &(argv[optind]));
	  perror("exec failed");
	  exit(EXIT_FAILURE);
	
     default :
	  break;
     }

     if (!init_machdep(kid)) {
	  fprintf(stderr, "%s: Failed to initialise sampling.\n", argv[0]);
	  exit(EXIT_FAILURE);
     }

     do {

	  if (!get_sample(&info)) {
	       fprintf(stderr, "%s: Sampling failed.\n", argv[0]);
	       exit(EXIT_FAILURE);
	  }

	  max_vsize = (info.vsize_kb > max_vsize ? info.vsize_kb : max_vsize);
	  max_rss = (info.rss_kb > max_rss ? info.rss_kb : max_rss);
 
	  if (sample_time) {
	       time1 += acc;
	       if (time1 > 6000) {
		    time1 = 0;
		    time2++;
		    if (time2 > sample_time) {

			 end = get_time();

			 fprintf(stderr,"%.2f user, %.2f system, "
				 "%.2f elapsed -- VSize = %dKB, RSS = %dKB\n",
				 (double)info.utime_ms/1000.0,
				 (double)info.stime_ms/1000.0,
				 (double)(end - start)/1000.0,
				 info.vsize_kb, info.rss_kb);
			 fflush(stdout);
 
			 time2 = 1;
		    }
	       }
	  }
       
	  if (cnt == 500) {
	       delay = 240000;
	       acc = 2000;
	  } 

	  usleep(delay);

     } while (wait4(kid, &kid_status, WNOHANG, &kid_usage) != kid);

     end = get_time();

     fprintf(stderr,"%d.%02d user, %d.%02d system, %.2f elapsed -- "
	     "Max VSize = %dKB, Max RSS = %dKB\n", 
	     kid_usage.ru_utime.tv_sec, kid_usage.ru_utime.tv_usec / 10000,
	     kid_usage.ru_stime.tv_sec, kid_usage.ru_stime.tv_usec / 10000,
	     (double)(end - start) / 1000.0, max_vsize, max_rss);

     exit (EXIT_SUCCESS);
}

