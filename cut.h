/*
 * cut.h
 * CUT 2.3
 *
 * Copyright (c) 2001 Samuel A. Falvo II, William D. Tanksley
 * See CUT-LICENSE.TXT for details.
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

typedef void(*cut_fn)(void);

void cut_init(const char *, int);
void cut_exit(void);

#define cut_run(G, T) __cut_run("group-" #G, \
                          __CUT_BRINGUP__ ## G, \
                          __CUT_TAKEDOWN__ ## G, \
                          #T, \
                          __CUT__ ## T, \
                          __FILE__, \
                          __LINE__);

#define ADDR(S) (__cut_debug_addr(#S, __FILE__, __LINE__))

#define ASSERT(X,msg)        __cut_assert(__FILE__,__LINE__,msg,#X,X)

#define STATIC_ASSERT(X)  extern bool __static_ASSERT_at_line_##__LINE__##__[ (0!=(X))*2-1 ];

/*
 * These functions are not officially "public".  They exist here because they
 * need to be for proper operation of CUT.  Please use the aforementioned
 * macros instead.
 */

void __cut_run(char *, cut_fn, cut_fn, char *, cut_fn, char *, int);
void __cut_assert( char *, int, char *, char *, int );
void *__cut_debug_value(const char *, const char *, int);

#endif

