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

static int            breakpoint = 0;
static int            count = 0;
static int any_problems = 0;

/* I/O Functions */

static void print_string( char *string )
{
  printf( "%s", string );
  fflush( stdout );
}

static void print_string_as_error( char *filename, int lineNumber, char *string )
{
  printf( "%s(%d): %s", filename, lineNumber, string );
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

void cut_exit( void )
{
  exit(any_problems);
}

/* Test Progress Accounting functions */

static void
cut_mark_point(char out, char *filename, int lineNumber )
{
  if( ( count % 10 ) == 0 )
  {
    if( ( count % 50 ) == 0 )
      new_line();

    print_integer_in_field( count, 5 );
  }
  else
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
  
  new_line();
  print_string_as_error( filename, lineNumber, message );
  new_line();
  print_string_as_error( filename, lineNumber, "Failed expression: " );
  print_string( expression );
  new_line();

  exit(-1);
}


/* Test Delineation and Teardown Support Functions */

static void
die(const char *msg)
{
    perror(msg);
    exit(3);
}

void
__cut_run(char *group_name, cut_fn bringup, cut_fn takedown, char *test_name,
        cut_fn test, char *filename, int lineno)
{
    pid_t pid;
    int status;

    if (pid = fork()) {
        if (pid < 0) die("\nfork()");
        wait(&status);
  
        if (!status) {
            /* success */
            cut_mark_point('.', filename, lineno );
        } else if (status == 65280) {
            /* failure */
            cut_mark_point('F', filename, lineno );
            any_problems = 1;
        } else {
            /* error */
            cut_mark_point('E', filename, lineno );
            any_problems = 1;
        }
    } else {
        bringup();
        test();
        takedown();
        exit(0);
    }
}
