/*
 * This file has been modified from the original CUT distribution.
 *
 * CUT 2.3-kr1
 * Copyright (c) 2001-2002 Samuel A. Falvo II, William D. Tanksley
 * See CUT-LICENSE.TXT for details.
 *
 * $Log: cutgen.c,v $
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "util.h"

#define DO_NOT_PROCESS        "..."

#define SEARCH_TOKEN_TEST     "__CUT__"
#define SEARCH_TOKEN_BRINGUP  "__CUT_BRINGUP__"
#define SEARCH_TOKEN_TAKEDOWN "__CUT_TAKEDOWN__"

#define MAX_SYMBOL_LENGTH 256  /* arbitrary */
#define MAX_LINE_LENGTH   1024 /* arbitrary */

#define SEARCH_TOKEN_TEST_LENGTH       sizeof( SEARCH_TOKEN_TEST )-1
#define SEARCH_TOKEN_BRINGUP_LENGTH    sizeof( SEARCH_TOKEN_BRINGUP )-1

#define terr(fmt, args...) do { fprintf(stderr, "\n"); twarn(fmt, ##args); exit(1); } while (0)
#define terrx(fmt, args...) do { fprintf(stderr, "\n"); twarnx(fmt, ##args); exit(1); } while (0)

typedef enum TestType {
   TYPE_TEST = 0,
   TYPE_BRINGUP = 1,
} TestType;

typedef struct TestItem {
   char name[MAX_SYMBOL_LENGTH];
   struct TestItem *next;
} TestItem;

typedef struct TestGroup {
   char name[MAX_SYMBOL_LENGTH];
   struct TestItem *tests;
   struct TestGroup *next;
} TestGroup;

/* globals */

TestGroup *test_groups = 0;
FILE *outfile;

static int g_count, g_ready, g_index;  /* Used by filename globbing support for windows */
static char **g_wildcards, g_fileName[MAX_LINE_LENGTH];

int NameAndTypeInTestList( char *name, TestType type )
{
   TestItem *item;
   TestGroup *group;

   for (group = test_groups; group; group = group->next) {
      if (!strcmp(group->name, name) && type == TYPE_BRINGUP) return 1;
      for (item = group->tests; item; item = item->next) {
         if (!strcmp(item->name, name)) return 1;
      }
   }

   return 0;
}

void AppendToTestList( char *name, TestType type )
{
   TestGroup *new, *cur;
   TestItem *newt;

   if (type == TYPE_BRINGUP) {
      struct TestGroup *new;

      new = malloc(sizeof(struct TestGroup));
      if (!new) terr("malloc");

      new->next = test_groups;
      strcpy(new->name, name);
      test_groups = new;
   } else {
      cur = test_groups;
      if (!cur) terrx("no current test group");

      newt = malloc(sizeof(struct TestItem));
      if (!newt) terr("malloc");

      newt->next = cur->tests;
      strcpy(newt->name, name);
      cur->tests = newt;
   }
}

void InsertNameAndTypeIntoTestList( char *name, TestType type )
{
    if ( !NameAndTypeInTestList( name, type ) )
        AppendToTestList( name, type );
}

int CharacterIsDigit(char ch)
{
    return ( ( ch >= '0') && ( ch <= '9' ) );
}

int CharacterIsUppercase(char ch)
{
    return ( ( ch >= 'A' ) && ( ch <= 'Z' ) );
}

int CharacterIsLowercase(char ch)
{
    return ( ( ch >= 'a' ) && ( ch <= 'z' ) );
}

int CharacterIsAlphabetic(char ch)
{
    return CharacterIsUppercase(ch) || CharacterIsLowercase(ch) || ( ch == '_' );
}

int CharacterIsAlphanumeric( char ch )
{
    return CharacterIsDigit(ch) || CharacterIsAlphabetic(ch);
}

void ProcessGenericFunction( char *line, int position, 
                            TestType type, int tokenDisplacement )
{
    char name[MAX_SYMBOL_LENGTH] = "";
    int maxLength = strlen( line ) - 1, offset=0;
    position = position + tokenDisplacement;
    
    while ( CharacterIsAlphanumeric(line[position]) 
       && (position<maxLength) && (offset<MAX_SYMBOL_LENGTH) )
    {
        name[offset++] = line[position++];
        name[offset] = 0;
    }

    InsertNameAndTypeIntoTestList( name, type );
}

void ProcessBringupFunction( char *line, int position )
{
    ProcessGenericFunction( line, position, TYPE_BRINGUP, SEARCH_TOKEN_BRINGUP_LENGTH );
}

void ProcessTestFunction( char *line, int position )
{
    ProcessGenericFunction( line, position, TYPE_TEST, SEARCH_TOKEN_TEST_LENGTH );
}


int OffsetOfSubstring( char *line, char *token )
{
    char *inset = strstr(line,token);
    
    if ( !inset ) return -1;
    else return inset - line;
}

void CallIfSubstringFound( char *line, char *token, void (*function)(char*,int) )
{
    int index = OffsetOfSubstring( line, token );
    if ( index != -1 )
        function( line, index );
}

void ProcessSourceFile( char *filename )
{
   FILE *source;
   char line[MAX_LINE_LENGTH];
  
   if( strcmp( filename, DO_NOT_PROCESS ) != 0 )
   {

      source = fopen(filename,"r");
      
      while ( fgets(line,MAX_LINE_LENGTH,source) )
      {
         CallIfSubstringFound( line, SEARCH_TOKEN_BRINGUP, ProcessBringupFunction );
         CallIfSubstringFound( line, SEARCH_TOKEN_TEST, ProcessTestFunction );
      }
      
      fclose(source);
   }
}

void EmitExternDeclarationFor( char *name, char *prefix )
{
    fprintf( outfile, "extern void %s%s( void );\n", prefix, name );
}

void Emit(char *text)
{
   fprintf(outfile, "%s\n", text);
}

void BlankLine()
{
    Emit( "" );
}

void ListExternalFunctions()
{
   TestGroup *cur;
   TestItem *item;

   for (cur = test_groups; cur; cur = cur->next) {
       EmitExternDeclarationFor(cur->name, SEARCH_TOKEN_BRINGUP);
       EmitExternDeclarationFor(cur->name, SEARCH_TOKEN_TAKEDOWN);
       for (item = cur->tests; item; item = item->next) {
          EmitExternDeclarationFor(item->name, SEARCH_TOKEN_TEST);
       }
   }
   BlankLine();
}

void ListHeaderFiles(void)
{
    Emit(
        "#include <string.h>\n"
        "#include <stdlib.h>\n"
        "#include <stdio.h>\n"
        "#include <stdarg.h>\n"
        "#include \"cut.h\"\n"
    );
    BlankLine();
    BlankLine();
}

void EmitIndented(int indent,char *format, ...)
{
   va_list v;
   /* Print two spaces per level of indentation. */
   fprintf( outfile, "%*s", indent*2, "" );
   
   va_start(v,format);
   vfprintf( outfile, format, v );
   va_end(v);
   
   fprintf( outfile, "\n" );
}

void EmitUnitTesterBody()
{
    TestItem *test;
    TestGroup *group;

    Emit( "int main( int argc, char *argv[] )\n{" );
    Emit( "  if ( argc == 1 )" );
    Emit( "    cut_init( -1 );" );
    Emit( "  else cut_init( atoi( argv[1] ) );" );
    BlankLine();

    for (group = test_groups; group; group = group->next) {
       for (test = group->tests; test; test = test->next) {
          EmitIndented(1, "cut_run(%s, %s);", group->name, test->name);
       }
    }

    BlankLine();
    Emit( "  cut_break_formatting();" );
    Emit( "  printf(\"Done.\\n\");" );
    Emit( "  return 0;\n}\n" );
}

void EmitCutCheck()
{
    Emit( "/* Automatically generated: DO NOT MODIFY. */" );
    ListHeaderFiles();
    BlankLine();
    ListExternalFunctions();
    BlankLine();
    EmitUnitTesterBody();
}

void FileName( char *base )
{
    strncpy( g_fileName, base, MAX_LINE_LENGTH );
    g_fileName[ MAX_LINE_LENGTH - 1 ] = 0;
}

int LoadArgument( void )
{
   if ( g_index >= g_count )
      return 0;

   FileName( g_wildcards[g_index] );
   g_index++;  /* MUST come after FileName() call; bad code smell */
   return 1;
}

void StartArguments( int starting, int count, char *array[] )
{
   g_index = starting;
   g_count = count;
   g_wildcards = array;

   g_ready = LoadArgument();
}

int NextArgument( void )
{
   if( g_ready )
      g_ready = LoadArgument();

   return g_ready;
}

char *GetArgument( void )
{
   if( g_ready )
      return g_fileName;

   return NULL;
}

void EstablishOutputFile( int argc, char *argv[] )
{
   int i;

   i = 0;
   while( i < argc )
   {
      if( ( argv[i+1] != NULL ) && ( strcmp( argv[i], "-o" ) == 0 ) )
      {
         outfile = fopen( argv[i+1], "wb+" );
         if( outfile == NULL )
            fprintf( stderr, "ERROR: Can't open %s for writing.\n", argv[i+1] );
         
         argv[i] = argv[i+1] = DO_NOT_PROCESS;

         return;
      }

      i++;
   }

   outfile = stdout;
}

int main( int argc,char *argv[] )
{
   char *filename;

   if ( argc < 2 )
   {
      fprintf(
              stderr,
              "USAGE:\n"
              "   %s [options] <input file> [<input file> [...]]\n"
              "\n"
              "OPTIONS:\n"
              "   -o filename   Specifies output file.\n"
              "\n"
              "NOTES:\n"
              "   If -o is left unspecified, output defaults to stdout.\n",
              argv[0]
             );
      return 3;
   }

   EstablishOutputFile( argc, argv );

   /* Skip the executable's name and the output filename. */
   StartArguments(0,argc,argv);

   /* Consume the rest of the arguments, one at a time. */
   while ( NextArgument() )
   {
      filename = GetArgument();

      if( strcmp( filename, DO_NOT_PROCESS ) )
      {
         fprintf( stderr, "  - parsing '%s'... ", filename);
         ProcessSourceFile( filename );
         fprintf( stderr, "done.\n");
      }
   }
   
   EmitCutCheck();
   fflush(outfile);
   fclose(outfile);
   return 0;
}

/*
 * vim: tabstop=3 shiftwidth=3 expandtab 
 */

