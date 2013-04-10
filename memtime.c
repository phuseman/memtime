/* -*- mode: C; c-file-style: "k&r"; -*-
 *---------------------------------------------------------------------------*
 *
 * Copyright (c) 2000, Johan Bengtsson
 * Copyright (c) 2009, Michael Weber
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

#include <signal.h>

#include <errno.h>
#include <assert.h>

#include "machdep.h"

void
usage (FILE *ffile, const char *progname)
{
     fprintf (ffile,
              "Usage:\n%s [-t <interval>] [-e] [-m <maxkilobytes>] "
              "[-c <maxcpuseconds>] [-o <filedescriptor>] <cmd> [<params>]\n", progname);
}

static int print_stats;
static pid_t kid;
int kid_signalled = 0;
static int outfd = 2;
static FILE* outf; /* initialized to stderr in main */

static int
xkill (pid_t kid, int signal)
{
     int rc = -1;
     if (kid != 0) {
          rc = kill (kid, signal);
          kid_signalled = 1;
     }
     return rc;
}

static const int MAX_SIGNAL = 64;       /* works for most OSes */
static void (*signal_prev[64]) (int);
static int caught_signals[] = {
     SIGINT, SIGTERM, SIGHUP, SIGUSR1, SIGUSR2,
#ifdef SIGXCPU
     SIGXCPU,
#endif
#ifdef SIGXFSZ
     SIGXFSZ,
#endif
     0
};

static void
fwd_signal (int signal)
{
     xkill (kid, signal);
     assert (signal < MAX_SIGNAL);
     if (signal_prev[signal] != NULL && signal_prev[signal] != fwd_signal)
          signal_prev[signal] (signal);
}

static void (*sigprof_prev) (int);
static void
handle_sigprof (int signal)
{
     print_stats = 1;
     if (sigprof_prev != NULL && sigprof_prev != handle_sigprof)
          sigprof_prev (signal);
}


int
main (int argc, char *argv[])
{
     struct rusage kid_usage;
     int    kid_status;
     int    i, opt, echo_args = 0, exit_flag;
     char   *endptr;

     unsigned long sample_time = 0, time = 0;
     unsigned long maxkbytes = 0; // kilobytes
     unsigned long maxseconds = 0; // seconds
     unsigned long maxmillis = 0;

     unsigned long max_vsize = 0, max_rss = 0;
     unsigned long start, end;

     memtime_info_t info;
     memset (&info, 0, sizeof info);

     outf = stderr;

     if (argc < 2) {
          usage(stderr, argv[0]);
          exit(EXIT_FAILURE);
     }

     while ((opt = getopt(argc, argv, "+eht:m:c:o:")) != -1) {

          switch (opt) {
          case 'e' :
               echo_args = 1;
               break;

          case 't' :
               errno = 0;
               sample_time = strtoul(optarg, &endptr, 10);
               if (errno) {
                    perror("Illegal argument to t option");
                    exit(EXIT_FAILURE);
               } else if (endptr == optarg || *endptr != '\0') {
                    fprintf(stderr, "Illegal argument to t option\n");
                    exit(EXIT_FAILURE);
               }
               break;
          case 'm' :
               errno = 0;
               maxkbytes =  strtoul(optarg, &endptr, 10);
               if (errno) {
                    perror("Illegal argument to m option");
                    exit(EXIT_FAILURE);
               } else if (endptr == optarg || *endptr != '\0') {
                    fprintf(stderr, "Illegal argument to m option\n");
                    exit(EXIT_FAILURE);
               }
               break;

          case 'c' :
               errno = 0;
               maxseconds = strtoul(optarg, &endptr, 10);
               if (errno) {
                    perror("Illegal argument to c option");
                    exit(EXIT_FAILURE);
               } else if (endptr == optarg || *endptr != '\0') {
                    fprintf(stderr, "Illegal argument to c option\n");
                    exit(EXIT_FAILURE);
               }
               maxmillis = 1000*maxseconds;
               break;

          case 'o' :
               outfd = strtoul(optarg, &endptr, 10);
               if (errno) {
                    perror("Illegal argument to o option");
                    exit(EXIT_FAILURE);
               } else if (endptr == optarg || *endptr != '\0') {
                    fprintf(stderr, "Illegal argument to o option\n");
                    exit(EXIT_FAILURE);
               }
               outf = fdopen(outfd, "w");
               if (errno) {
                    perror("Cannot open file descriptor given as argument to o option");
                    exit(EXIT_FAILURE);
               }
               break;

          case 'h':
               usage (stdout, argv[0]);
               exit(EXIT_SUCCESS);

          default:
               exit(EXIT_FAILURE);
          }
     }

     if (optind == argc) {
	  fprintf(stderr, "Missing command\n");
	  exit(EXIT_FAILURE);
     }

     if (echo_args) {
          fprintf(stderr,"Command line: ");
          for (i = optind; i < argc; i++)
               fprintf(outf,"%s ", argv[i]);
          fprintf(outf,"\n");
     }

     start = get_time();

     switch (kid = sampling_fork()) {
     case -1 :
          perror("sampling_fork failed");
          exit(EXIT_FAILURE);

     case 0 :
#if defined(CAN_USE_RLIMIT_VSIZE)
          if (maxkbytes>0) {
               set_mem_limit(maxkbytes*1024);
          }
#endif
#if defined(CAN_USE_RLIMIT_CPU)
          if (maxseconds>0) {
               set_cpu_limit(maxseconds);
          }
#endif
          execvp(argv[optind], &(argv[optind]));
          perror("exec failed");
          exit(EXIT_FAILURE);

     default :
          break;
     }

     for (i = 0; caught_signals[i] != 0; ++i) {
          assert (caught_signals[i] < MAX_SIGNAL);
          signal_prev[caught_signals[i]] =
               signal (caught_signals[i], fwd_signal);
     }
     sigprof_prev = signal (SIGPROF, handle_sigprof);

     do {
          get_sample(&info);

          max_vsize = (info.vsize_kb > max_vsize ? info.vsize_kb : max_vsize);
          max_rss = (info.rss_kb > max_rss ? info.rss_kb : max_rss);

          if (sample_time) {
               time++;
               if (time >= 10 * sample_time) {
                    time = 0;
                    print_stats = 1;
               }
          }

          if (print_stats) {
               end = get_time();

               fprintf(outf,"%.2f user, %.2f system, %.2f elapsed"
                       " -- VSize = %luKB, RSS = %luKB\n",
                       (double)info.utime_ms/1000.0,
                       (double)info.stime_ms/1000.0,
                       (double)(end - start)/1000.0,
                       info.vsize_kb, info.rss_kb);
               fflush(outf);
               print_stats = 0;               
          }

          usleep(100000);

          int wait_for = wait4(kid, &kid_status, WNOHANG, &kid_usage);
          if (wait_for == -1 && errno != EINTR) {
               perror ("wait4");
               abort ();
          }
          exit_flag = (wait_for == kid
                       && (WIFEXITED(kid_status) || WIFSIGNALED(kid_status)));

#if !defined(CAN_USE_RLIMIT_VSIZE)
          if ((maxkbytes>0) && (max_vsize>maxkbytes)) {
               xkill(kid, SIGKILL);
          }
#endif
#if !defined(CAN_USE_RLIMIT_CPU)
          if ((maxmillis>0) && (info.utime_ms>maxmillis)) {
               xkill(kid, SIGKILL);
          }
#endif
     } while (!exit_flag);

     end = get_time();

     for (i = 0; caught_signals[i] != 0; ++i)
          signal (caught_signals[i], signal_prev[caught_signals[i]]);
     signal (SIGPROF, sigprof_prev);

     if (WIFEXITED(kid_status)) {
          fprintf(outf, "Exit [%d]\n", WEXITSTATUS(kid_status));
     } else {
          fprintf(outf, "Killed [%d]\n", WTERMSIG(kid_status));
     }

     {
          max_vsize = (info.vsize_kb > max_vsize ? info.vsize_kb : max_vsize);
          max_rss = (info.rss_kb > max_rss ? info.rss_kb : max_rss);
          double kid_utime = ((double)kid_usage.ru_utime.tv_sec
                              + (double)kid_usage.ru_utime.tv_usec / 1E6);
          double kid_stime = ((double)kid_usage.ru_stime.tv_sec
                              + (double)kid_usage.ru_stime.tv_usec / 1E6);

          fprintf(outf, "%.2f user, %.2f system, %.2f elapsed -- "
                  "Max VSize = %luKB, Max RSS = %luKB\n",
                  kid_utime, kid_stime, (double)(end - start) / 1000.0,
                  max_vsize, max_rss);
     }

     if(WIFEXITED(kid_status))
          exit(WEXITSTATUS(kid_status));
     else {
          int csig = WTERMSIG(kid_status);
          switch (csig) {
          case SIGHUP: case SIGINT: case SIGUSR1: case SIGUSR2:
          case SIGKILL: case SIGALRM: case SIGTERM: case SIGPIPE:
#ifdef SIGXCPU
          case SIGXCPU:
#endif
#ifdef SIGXFSZ
          case SIGXFSZ:
#endif
               raise (csig);
          }
          fprintf (outf, "%s: child died with signal %d, aborting.\n",
                   argv[0], csig);
          abort ();
     }
}
