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
#include <sys/wait.h>
#include "cut.h"


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
static cut_fn cur_takedown;
static test_output problem_reports = 0;

static void
die(const char *msg)
{
    perror(msg);
    exit(3);
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

static void dot( void )
{
  print_character( '.' );
}

static void space( void )
{
  print_character( ' ' );
}

/* CUT Initialization and Takedown  Functions */

void cut_init( int brkpoint )
{
  breakpoint = brkpoint;
  count = 0;

  if( brkpoint >= 0 )
  {
    print_string( "Breakpoint at test " );
    print_integer( brkpoint );
    new_line();
  }
}

#define BUF_SIZE 1024

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
        while (r = fread(buf, 1, BUF_SIZE, to->file)) {
            s = fwrite(buf, 1, r, stdout);
            if (r != s) die("fwrite");
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

  cur_takedown();
  fflush(stdout);
  fflush(stderr);
  exit(-1);
}


/* Test Delineation and Teardown Support Functions */

void
__cut_run(char *group_name, cut_fn bringup, cut_fn takedown, char *test_name,
        cut_fn test, char *filename, int lineno)
{
    pid_t pid;
    int status, r;
    FILE *out;
    test_output to;
    char *problem_desc = 0;

    out = tmpfile();
    fflush(stdout);
    fflush(stderr);

    if (pid = fork()) {
        if (pid < 0) die("\nfork()");
        wait(&status);
  
        if (!status) {
            /* success */
            cut_mark_point('.', filename, lineno );
        } else if (WIFEXITED(status) && (WEXITSTATUS(status) == 255)) {
            /* failure */
            cut_mark_point('F', filename, lineno );
            count_failures++;
            problem_desc = "Failure";
        } else {
            /* error */
            cut_mark_point('E', filename, lineno );
            count_errors++;
            problem_desc = "Error";
        }

        /* collect the output */
        if (problem_desc) {
            to = malloc(sizeof(struct test_output));
            if (!to) die("malloc");

            to->desc = problem_desc;
            to->status = status;
            to->group_name = group_name;
            to->test_name = test_name;
            to->file = out;
            to->next = problem_reports;
            problem_reports = to;
        } else {
            fclose(out);
            out = 0;
        }
    } else {
        r = dup2(fileno(out), fileno(stdout));
        if (r < 0) die("dup2");
        r = dup2(fileno(out), fileno(stderr));
        if (r < 0) die("dup2");
        r = fclose(out);
        if (r) die("fclose");
        out = 0;

        bringup();
        cur_takedown = takedown;
        test();
        takedown();
        fflush(stdout);
        fflush(stderr);
        exit(0);
    }
}
