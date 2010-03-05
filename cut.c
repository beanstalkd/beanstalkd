/*
 * libcut.inc
 * CUT 2.1
 *
 * Copyright (c) 2001-2002 Samuel A. Falvo II, William D. Tanksley
 * See CUT-LICENSE.TXT for details.
 *
 * Based on WDT's 'TestAssert' package.
 *
 * $log$
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include "cut.h"


#define BUF_SIZE 1024

#ifndef BOOL		/* Just in case -- helps in portability */
#define BOOL int
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef TRUE
#define TRUE 1
#endif

typedef struct test_output *test_output;

struct test_output {
    test_output next;
    int status;
    const char *desc;
    const char *group_name;
    const char *test_name;
    FILE *file;
};

static int            breakpoint = 0;
static int count = 0, count_failures = 0, count_errors = 0;
static cut_fn cur_takedown = 0;
static test_output problem_reports = 0;
static const char *program;

static void
die(int code, const char *fmt, ...)
{
    va_list v;

    putc('\n', stderr);

    va_start(v, fmt);
    vfprintf(stderr, fmt, v);
    va_end(v);

    if (fmt && *fmt) fputs(": ", stderr);
    fprintf(stderr, "%s\n", strerror(errno));
    exit(code);
}

/* I/O Functions */

static void print_string( char *string )
{
  printf( "%s", string );
  fflush( stdout );
}

static void print_string_as_error( char *filename, int lineNumber, char *string )
{
  printf( "  %s:%d: %s", filename, lineNumber, string );
  fflush( stdout );
}

static void print_integer( int i )
{
  printf( "%d", i );
  fflush( stdout );
}

static void print_integer_in_field( int i, int width )
{
  printf( "%*d", width, i );
  fflush( stdout );
}

static void new_line( void )
{
  printf( "\n" );
  fflush( stdout );
}

static void print_character( char ch )
{
  printf( "%c", ch );
  fflush( stdout );
}

/* CUT Initialization and Takedown  Functions */

void cut_init(const char *prog_name, int brkpoint )
{
  breakpoint = brkpoint;
  count = 0;
  program = prog_name;

  if( brkpoint >= 0 )
  {
    print_string( "Breakpoint at test " );
    print_integer( brkpoint );
    new_line();
  }
}

void cut_exit( void )
{
    int r, s;
    char buf[BUF_SIZE];
    test_output to;

    printf("\n");
    for (to = problem_reports; to; to = to->next) {
        printf("\n%s in %s/%s", to->desc, to->group_name, to->test_name);
        if (!WIFEXITED(to->status) || (WEXITSTATUS(to->status) != 255)) {
            if (WIFEXITED(to->status)) {
                printf(" (Exit Status %d)", WEXITSTATUS(to->status));
            }
            if (WIFSIGNALED(to->status)) {
                printf(" (Signal %d)", WTERMSIG(to->status));
            }
        }
        printf("\n");
        rewind(to->file);
        while ((r = fread(buf, 1, BUF_SIZE, to->file))) {
            s = fwrite(buf, 1, r, stdout);
            if (r != s) die(3, "fwrite");
        }
    }

    printf("\n%d tests; %d failures; %d errors.\n", count, count_failures,
            count_errors);
    exit(!!(count_failures + count_errors));
}

/* Test Progress Accounting functions */

static void
cut_mark_point(char out, char *filename, int lineNumber )
{
  if ((count % 10) == 0) {
    if ((count % 50) == 0) new_line();
    print_integer_in_field( count, 5 );
  }

  print_character(out);
  count++;

  if( count == breakpoint )
  {
    print_string_as_error( filename, lineNumber, "Breakpoint hit" );
    new_line();
    cut_exit();
  }
}


void __cut_assert(
                  char *filename,
                  int   lineNumber,
                  char *message,
                  char *expression,
                  BOOL  success
                 )
{
  if (success) return;

  print_string_as_error( filename, lineNumber, "(" );
  print_string( expression );
  print_string(") ");
  print_string( message );
  new_line();

  if (cur_takedown) cur_takedown();
  fflush(stdout);
  fflush(stderr);
  exit(-1);
}

typedef void(*collect_fn)(void *);

static FILE *
collect(pid_t *pid, collect_fn fn, void *data)
{
    int r;
    FILE *out;

    out = tmpfile();
    if (!out) return 0;

    fflush(stdout);
    fflush(stderr);

    if ((*pid = fork())) {
        if (*pid < 0) return 0;
        return out;
    } else {
        r = dup2(fileno(out), fileno(stdout));
        if (r < 0) die(3, "dup2");
        r = fclose(out);
        if (r) die(3, "fclose");
        out = 0;

        fn(data);
        exit(0);
    }
}

static void
run_in_child(void *data)
{
    int r;
    cut_fn *fns = data, bringup = fns[0], test = fns[1], takedown = fns[2];

    r = dup2(fileno(stdout), fileno(stderr));
    if (r < 0) die(3, "dup2");
    bringup();
    cur_takedown = takedown;
    test();
    takedown();
    fflush(stdout);
    fflush(stderr);
}

void
__cut_run(char *group_name, cut_fn bringup, cut_fn takedown, char *test_name,
        cut_fn test, char *filename, int lineno)
{
    pid_t pid = -1;
    int status, r;
    FILE *out;
    test_output to;
    char *problem_desc = 0;
    cut_fn fns[3] = { bringup, test, takedown };

    out = collect(&pid, run_in_child, fns);
    if (!out) die(1, "  %s:%d: collect", filename, lineno);
    if (pid < 0) die(3, "fork");

    r = waitpid(pid, &status, 0);
    if (r != pid) die(3, "wait");

    if (!status) {
        cut_mark_point('.', filename, lineno );
    } else if (WIFEXITED(status) && (WEXITSTATUS(status) == 255)) {
        cut_mark_point('F', filename, lineno );
        count_failures++;
        problem_desc = "Failure";
    } else {
        cut_mark_point('E', filename, lineno );
        count_errors++;
        problem_desc = "Error";
    }

    if (!problem_desc) {
        fclose(out);
        return;
    }

    /* collect the output */
    to = malloc(sizeof(struct test_output));
    if (!to) die(3, "malloc");

    to->desc = problem_desc;
    to->status = status;
    to->group_name = group_name;
    to->test_name = test_name;
    to->file = out;
    to->next = problem_reports;
    problem_reports = to;
}
