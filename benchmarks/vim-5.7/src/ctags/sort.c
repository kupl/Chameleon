/*****************************************************************************
*   $Id: sort.c,v 8.9 2000/01/11 03:53:57 darren Exp $
*
*   Copyright (c) 1996-1999, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions to sort the tag entries.
*****************************************************************************/

/*============================================================================
=   Include files
============================================================================*/
#include "general.h"

#if defined(HAVE_STDLIB_H)
# include <stdlib.h>	/* to declare malloc() */
#endif
#include <string.h>
#include <stdio.h>

#include "debug.h"
#include "entry.h"
#include "main.h"
#include "options.h"
#include "read.h"
#include "sort.h"

#ifdef TRAP_MEMORY_CALLS
# include "safe_malloc.h"
#endif

/*============================================================================
=   Function prototypes
============================================================================*/
#ifndef EXTERNAL_SORT
static void failedSort __ARGS((FILE *const fp, const char* msg));
static int compareTags __ARGS((const void *const one, const void *const two));
static void writeSortedTags __ARGS((char **const table, const size_t numTags, const boolean toStdout));
#endif

/*============================================================================
=   Function definitions
============================================================================*/

extern void catFile( name )
    const char *const name;
{
    FILE *const fp = fopen(name, "r");

    if (fp != NULL)
    {
	int c;

	while ((c = getc(fp)) != EOF)
	    putchar(c);
	fflush(stdout);
	fclose(fp);
    }
}

#ifdef EXTERNAL_SORT

#ifdef NON_CONST_PUTENV_PROTOTYPE
# define PE_CONST
#else
# define PE_CONST const
#endif

extern void externalSortTags( toStdout )
    const boolean toStdout;
{
    const char *const sortCommand = "sort -u -o";
    PE_CONST char *const sortOrder1 = "LC_COLLATE=C";
    PE_CONST char *const sortOrder2 = "LC_ALL=C";
    const size_t length	= 4 + strlen(sortOrder1) + strlen(sortOrder2) +
	    strlen(sortCommand) + (2 * strlen(tagFileName()));
    char *const cmd = (char *)malloc(length + 1);
    int ret = -1;

    if (cmd != NULL)
    {
	/*  Ensure ASCII value sort order.
	 */
#ifdef HAVE_SETENV
	setenv("LC_COLLATE", "C", 1);
	setenv("LC_ALL", "C", 1);
	sprintf(cmd, "%s %s %s", sortCommand, tagFileName(), tagFileName());
#else
# ifdef HAVE_PUTENV
	putenv(sortOrder1);
	putenv(sortOrder2);
	sprintf(cmd, "%s %s %s", sortCommand, tagFileName(), tagFileName());
# else
	sprintf(cmd, "%s %s %s %s %s", sortOrder1, sortOrder2, sortCommand,
		tagFileName(), tagFileName());
# endif
#endif
	if (Option.verbose)
	    printf("system(\"%s\")\n", cmd);
	ret = system(cmd);
	free(cmd);

    }
    if (ret != 0)
	error(FATAL | PERROR, "cannot sort tag file");
    else if (toStdout)
	catFile(tagFileName());
}

#else

/*----------------------------------------------------------------------------
 *  These functions provide a basic internal sort. No great memory
 *  optimization is performed (e.g. recursive subdivided sorts),
 *  so have lots of memory if you have large tag files.
 *--------------------------------------------------------------------------*/

static void failedSort( fp, msg )
    FILE *const fp;
    const char* msg;
{
    const char* const cannotSort = "cannot sort tag file";
    if (fp != NULL)
	fclose(fp);
    if (msg == NULL)
	error(FATAL | PERROR, cannotSort);
    else
	error(FATAL, "%s: %s", msg, cannotSort);
}

static int compareTags( one, two )
    const void *const one;
    const void *const two;
{
    const char *const line1 = *(const char *const *const)one;
    const char *const line2 = *(const char *const *const)two;

    return strcmp(line1, line2);
}

static void writeSortedTags( table, numTags, toStdout )
    char **const table;
    const size_t numTags;
    const boolean toStdout;
{
    FILE *fp;
    size_t i;

    /*	Write the sorted lines back into the tag file.
     */
    if (toStdout)
	fp = stdout;
    else
    {
	fp = fopen(tagFileName(), "w");
	if (fp == NULL)
	    failedSort(fp, NULL);
    }
    for (i = 0 ; i < numTags ; ++i)
    {
	/*  Here we filter out identical tag *lines* (including search
	 *  pattern) if this is not an xref file.
	 */
	if (i == 0  ||  Option.xref  ||  strcmp(table[i], table[i-1]) != 0)
	    if (fputs(table[i], fp) == EOF)
		failedSort(fp, NULL);
    }
    if (toStdout)
	fflush(fp);
    else
	fclose(fp);
}

extern void internalSortTags( toStdout )
    const boolean toStdout;
{
    vString *vLine = vStringNew();
    FILE *fp = NULL;
    const char *line;
    size_t i;

    /*	Allocate a table of line pointers to be sorted.
     */
    size_t numTags = TagFile.numTags.added + TagFile.numTags.prev;
    const size_t tableSize = numTags * sizeof(char *);
    char **const table = (char **)malloc(tableSize);	/* line pointers */
    DebugStatement( size_t mallocSize = tableSize; )	/* cumulative total */

    if (table == NULL)
	failedSort(fp, "out of memory");

    /*	Open the tag file and place its lines into allocated buffers.
     */
    fp = fopen(tagFileName(), "r");
    if (fp == NULL)
	failedSort(fp, NULL);
    for (i = 0  ;  i < numTags  &&  ! feof(fp)  ;  )
    {
	line = readLine(vLine, fp);
	if (line == NULL)
	{
	    if (! feof(fp))
		failedSort(fp, NULL);
	    break;
	}
	else if (*line == '\0'  ||  strcmp(line, "\n") == 0)
	    ;		/* ignore blank lines */
	else
	{
	    const size_t stringSize = strlen(line) + 1;

	    table[i] = (char *)malloc(stringSize);
	    if (table[i] == NULL)
		failedSort(fp, "out of memory");
	    DebugStatement( mallocSize += stringSize; )
	    strcpy(table[i], line);
	    ++i;
	}
    }
    numTags = i;
    fclose(fp);
    vStringDelete(vLine);

    /*	Sort the lines.
     */
    qsort(table, numTags, sizeof(*table),
	  (int (*)__ARGS((const void *, const void *)))compareTags);

    writeSortedTags(table, numTags, toStdout);

    PrintStatus(("sort memory: %ld bytes\n", (long)mallocSize));
    for (i = 0 ; i < numTags ; ++i)
	free(table[i]);
    free(table);
}

#endif

/* vi:set tabstop=8 shiftwidth=4: */
