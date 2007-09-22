/*
 * cut.h
 * CUT 2.3
 *
 * Copyright (c) 2001 Samuel A. Falvo II, William D. Tanksley
 * See LICENSE.TXT for details.
 *
 * Based on WDT's "TestAssert" package.
 *
 * $Log: cut.h,v $
 * Revision 1.4  2003/03/18 05:53:50  sfalvo
 * ADD: cutgen.c: cut_exit() -- common exit point; returns proper error code
 * at all times.
 *
 * FIX: cutgen.c: Factored all instances of exit() to invoke cut_exit()
 * instead.  This fixes the bug #703793.
 *
 * Revision 1.3  2003/03/13 04:27:54  sfalvo
 * ADD: LICENSE.TXT -- zlib license
 *
 * ADD: README cut.h cutgen.c -- Changelog token for CVS
 *
 * FIX: test/bringup-failure -- reflects new usage for bringups and
 * teardowns in CUT 2.2.
 *
 */

#ifndef CUT_CUT_H_INCLUDED
#define CUT_CUT_H_INCLUDED

typedef void CUTTakedownFunction( void );

void cut_start            ( char *, CUTTakedownFunction * );
void cut_init             ( int breakpoint );
void cut_break_formatting ( void );
void cut_resume_formatting( void );
void cut_interject( const char *, ... );

#define cut_end(t)           __cut_end( __FILE__, __LINE__, t )
#define cut_mark_point()     __cut_mark_point(__FILE__,__LINE__)
#define cut_check_errors()   __cut_check_errors( __FILE__, __LINE__ )
#define ASSERT(X,msg)        __cut_assert(__FILE__,__LINE__,msg,#X,X)

#define ASSERT_EQUALS(X,Y,msg)   __cut_assert_equals( __FILE__, __LINE__, msg, #X " == " #Y, ( (X) == (Y) ), X )

#define STATIC_ASSERT(X)  extern bool __static_ASSERT_at_line_##__LINE__##__[ (0!=(X))*2-1 ];

/*
 * These functions are not officially "public".  They exist here because they
 * need to be for proper operation of CUT.  Please use the aforementioned
 * macros instead.
 */

void __cut_end          ( char *, int, char * );
void __cut_mark_point   ( char *, int );
void __cut_assert       ( char *, int, char *, char *, int );
void __cut_assert_equals( char *, int, char *, char *, int, int );
int  __cut_check_errors ( char *, int );

#endif

