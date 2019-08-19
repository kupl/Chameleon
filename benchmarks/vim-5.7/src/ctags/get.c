/*****************************************************************************
*   $Id: get.c,v 8.3 1999/10/31 14:33:13 darren Exp $
*
*   Copyright (c) 1996-1999, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains the high level source read functions (preprocessor
*   directives are handled within this level).
*****************************************************************************/

/*============================================================================
=   Include files
============================================================================*/
#include "general.h"

#include <string.h>

#include "debug.h"
#include "entry.h"
#include "get.h"
#include "main.h"
#include "options.h"
#include "read.h"
#include "vstring.h"

/*============================================================================
=   Macros
============================================================================*/
#define stringMatch(s1,s2)	(strcmp(s1,s2) == 0)
#define isspacetab(c)		((c) == ' ' || (c) == '\t')

/*============================================================================
=   Data declarations
============================================================================*/
typedef enum { COMMENT_NONE, COMMENT_C, COMMENT_CPLUS } Comment;

enum eCppLimits {
    MaxCppNestingLevel = 20,
    MaxDirectiveName = 10
};

/*============================================================================
=   Data declarations
============================================================================*/

/*  Defines the one nesting level of a preprocessor conditional.
 */
typedef struct sConditionalInfo {
    boolean ignoreAllBranches;	/* ignoring parent conditional branch */
    boolean singleBranch;	/* choose only one branch */
    boolean branchChosen;	/* branch already selected */
    boolean ignoring;		/* current ignore state */
} conditionalInfo;

/*  Defines the current state of the pre-processor.
 */
typedef struct sCppState {
    int	    ungetch, ungetch2;	/* ungotten characters, if any */
    boolean resolveRequired;	/* must resolve if/else/elif/endif branch */
    struct sDirective {
	enum eState {
	    DRCTV_NONE,		/* no known directive - ignore to end of line */
	    DRCTV_DEFINE,	/* "#define" encountered */
	    DRCTV_HASH,		/* initial '#' read; determine directive */
	    DRCTV_IF,		/* "#if" or "#ifdef" encountered */
	    DRCTV_LINE,		/* "#line" encountered */
	    DRCTV_UNDEF		/* "#undef" encountered */
	} state;
	boolean	accept;		/* is a directive syntatically permitted? */
	vString * name;		/* macro name */
	unsigned int nestLevel;	/* level 0 is not used */
	conditionalInfo ifdef[MaxCppNestingLevel];
    } directive;
} cppState;

/*============================================================================
=   Data definitions
============================================================================*/

/*  Use brace formatting to detect end of block.
 */
static boolean BraceFormat = FALSE;

static cppState Cpp = {
    '\0', '\0',			/* ungetch characters */
    FALSE,			/* resolveRequired */
    {
	DRCTV_NONE,		/* state */
	FALSE,			/* accept */
	NULL,			/* tag name */
	0,			/* nestLevel */
	{ {FALSE,FALSE,FALSE,FALSE} }	/* ifdef array */
    }				/* directive */
};

/*============================================================================
=   Function prototypes
============================================================================*/
static boolean readDirective __ARGS((int c, char *const name, unsigned int maxLength));
static boolean readDefineTag __ARGS((int c, vString *const name, boolean *const parameterized));
static unsigned long readLineNumber __ARGS((int c));
static conditionalInfo *currentConditional __ARGS((void));
static boolean isIgnore __ARGS((void));
static boolean setIgnore __ARGS((const boolean ignore));
static boolean isIgnoreBranch __ARGS((void));
static void chooseBranch __ARGS((void));
static boolean pushConditional __ARGS((const boolean firstBranchChosen));
static boolean popConditional __ARGS((void));
static boolean directiveHash __ARGS((const int c));
static boolean directiveIf __ARGS((const int c));
static void makeDefineTag __ARGS((const char *const name));
static void directiveDefine __ARGS((const int c));
static vString *readFileName __ARGS((int c));
static void directiveLine __ARGS((int c));
static boolean handleDirective __ARGS((const int c));
static Comment isComment __ARGS((void));
static int skipOverCComment __ARGS((void));
static int skipOverCplusComment __ARGS((void));
static int skipToEndOfString __ARGS((void));
static int skipToEndOfChar __ARGS((void));

/*============================================================================
=   Function definitions
============================================================================*/

extern boolean isBraceFormat()
{
    return BraceFormat;
}

extern unsigned int getDirectiveNestLevel ()
{
    return Cpp.directive.nestLevel;
}

extern void cppInit( state )
    const boolean state;
{
    BraceFormat = state;

    Cpp.ungetch         = '\0';
    Cpp.ungetch2        = '\0';
    Cpp.resolveRequired = FALSE;

    Cpp.directive.state	    = DRCTV_NONE;
    Cpp.directive.accept    = TRUE;
    Cpp.directive.nestLevel = 0;

    if (Cpp.directive.name == NULL)
	Cpp.directive.name = vStringNew();
    else
	vStringClear(Cpp.directive.name);
}

extern void cppTerminate()
{
    if (Cpp.directive.name != NULL)
    {
	vStringDelete(Cpp.directive.name);
	Cpp.directive.name = NULL;
    }
}

extern void cppBeginStatement ()
{
    Cpp.resolveRequired = TRUE;
}

extern void cppEndStatement ()
{
    Cpp.resolveRequired = FALSE;
}

/*----------------------------------------------------------------------------
*   Scanning functions
*
*   This section handles preprocessor directives.  It strips out all
*   directives and may emit a tag for #define directives.
*--------------------------------------------------------------------------*/

/*  This puts a character back into the input queue for the source File.
 *  Up to two characters may be ungotten.
 */
extern void cppUngetc( c )
    const int c;
{
    Assert(Cpp.ungetch2 == '\0');
    Cpp.ungetch2 = Cpp.ungetch;
    Cpp.ungetch = c;
}

/*  Reads a directive, whose first character is given by "c", into "name".
 */
static boolean readDirective( c, name, maxLength )
    int c;
    char *const name;
    unsigned int maxLength;
{
    unsigned int i;

    for (i = 0  ;  i < maxLength - 1  ;  ++i)
    {
	if (i > 0)
	{
	    c = fileGetc();
	    if (c == EOF  ||  ! isalpha(c))
	    {
		fileUngetc(c);
		break;
	    }
	}
	name[i] = c;
    }
    name[i] = '\0';					/* null terminate */

    return (boolean)isspacetab(c);
}

/*  Reads an identifier, whose first character is given by "c", into "tag",
 *  together with the file location and corresponding line number.
 */
static boolean readDefineTag( c, name, parameterized )
    int c;
    vString *const name;
    boolean *const parameterized;
{
    vStringClear(name);
    do
    {
	vStringPut(name, c);
    } while (c = fileGetc(), (c != EOF  &&  isident(c)));
    fileUngetc(c);
    vStringPut(name, '\0');

    *parameterized = (boolean)(c == '(');
    return (boolean)(isspace(c)  ||  c == '(');
}

/*  Reads a line number.
 */
static unsigned long readLineNumber( c )
    int c;
{
    unsigned long lNum = 0;

    while (c != EOF  &&  isdigit(c))
    {
	lNum = (lNum * 10) + (c - '0');
	c = fileGetc();
    }
    fileUngetc(c);

    return lNum;
}

static conditionalInfo *currentConditional()
{
    return &Cpp.directive.ifdef[Cpp.directive.nestLevel];
}

static boolean isIgnore()
{
    return Cpp.directive.ifdef[Cpp.directive.nestLevel].ignoring;
}

static boolean setIgnore( ignore )
    const boolean ignore;
{
    return Cpp.directive.ifdef[Cpp.directive.nestLevel].ignoring = ignore;
}

static boolean isIgnoreBranch()
{
    conditionalInfo *const ifdef = currentConditional();

    /*  Force a single branch if an incomplete statement is discovered
     *  en route. This may have allowed earlier branches containing complete
     *  statements to be followed, but we must follow no further branches.
     */
    if (Cpp.resolveRequired  &&  ! BraceFormat)
	ifdef->singleBranch = TRUE;

    /*  We will ignore this branch in the following cases:
     *
     *  1.  We are ignoring all branches (conditional was within an ignored
     *        branch of the parent conditional)
     *  2.  A branch has already been chosen and either of:
     *      a.  A statement was incomplete upon entering the conditional
     *      b.  A statement is incomplete upon encountering a branch
     */
    return (boolean)(ifdef->ignoreAllBranches ||
		     (ifdef->branchChosen  &&  ifdef->singleBranch));
}

static void chooseBranch()
{
    if (! BraceFormat)
    {
	conditionalInfo *const ifdef = currentConditional();

	ifdef->branchChosen = (boolean)(ifdef->singleBranch ||
					Cpp.resolveRequired);
    }
}

/*  Pushes one nesting level for an #if directive, indicating whether or not
 *  the branch should be ignored and whether a branch has already been chosen.
 */
static boolean pushConditional( firstBranchChosen )
    const boolean firstBranchChosen;
{
    const boolean ignoreAllBranches = isIgnore();	/* current ignore */
    boolean ignoreBranch = FALSE;

    if (Cpp.directive.nestLevel < (unsigned int)MaxCppNestingLevel - 1)
    {
	conditionalInfo *ifdef;

	++Cpp.directive.nestLevel;
	ifdef = currentConditional();

	/*  We take a snapshot of whether there is an incomplete statement in
	 *  progress upon encountering the preprocessor conditional. If so,
	 *  then we will flag that only a single branch of the conditional
	 *  should be followed.
	 */
	ifdef->ignoreAllBranches= ignoreAllBranches;
	ifdef->singleBranch	= Cpp.resolveRequired;
	ifdef->branchChosen	= firstBranchChosen;
	ifdef->ignoring		= (boolean)(ignoreAllBranches || (
				    ! firstBranchChosen  &&  ! BraceFormat  &&
				    (ifdef->singleBranch || !Option.if0)));
	ignoreBranch = ifdef->ignoring;
    }
    return ignoreBranch;
}

/*  Pops one nesting level for an #endif directive.
 */
static boolean popConditional()
{
    if (Cpp.directive.nestLevel > 0)
	--Cpp.directive.nestLevel;

    return isIgnore();
}

static vString *readFileName( c )
    int c;
{
    vString *const fileName = vStringNew();
    boolean quoteDelimited = FALSE;

    if (c == '"')
    {
	c = fileGetc();			/* skip double-quote */
	quoteDelimited = TRUE;
    }
    while (c != EOF  &&  c != '\n'  &&
	    (quoteDelimited ? (c != '"') : (c != ' '  &&  c != '\t')))
    {
	vStringPut(fileName, c);
	c = fileGetc();
    }
    vStringPut(fileName, '\0');

    return fileName;
}

static void directiveLine( c )
    int c;
{
    if (Option.lineDirectives)
    {
	unsigned long lNum = readLineNumber(c);
	vString *fileName;

	/*  Skip over space and tabs.
	*/
	do { c = fileGetc(); } while (c != EOF  &&  (c == ' '  ||  c == '\t'));

	fileName = readFileName(c);
	if (vStringLength(fileName) == 0)
	    vStringDelete(fileName);
	else
	    setSourceFileName(fileName);
	setSourceFileLine(lNum - 1);		/* applies to NEXT line */
    }
    Cpp.directive.state = DRCTV_NONE;
}

static void makeDefineTag( name )
    const char *const name;
{
    const boolean isFileScope = (boolean)(! isHeaderFile());

    if (Option.include.cTypes.defines  &&
	(! isFileScope  ||  Option.include.fileScope))
    {
	tagEntryInfo e;

	initTagEntry(&e, name);

	e.lineNumberEntry = (boolean)(Option.locate != EX_PATTERN);
	e.isFileScope	= isFileScope;
	e.truncateLine	= TRUE;
	e.kindName	= "macro";
	e.kind		= 'd';

	makeTagEntry(&e);
    }
}

static void directiveDefine( c )
    const int c;
{
    boolean parameterized;

    if (isident1(c))
    {
	readDefineTag(c, Cpp.directive.name, &parameterized);
	makeDefineTag(vStringValue(Cpp.directive.name));
    }
    Cpp.directive.state = DRCTV_NONE;
}

static boolean directiveIf( c )
    const int c;
{
    DebugStatement( const boolean ignore0 = isIgnore(); )
    const boolean ignore = pushConditional((boolean)(c != '0'));

    Cpp.directive.state = DRCTV_NONE;
    DebugStatement( debugCppNest(TRUE, Cpp.directive.nestLevel);
		    if (ignore != ignore0) debugCppIgnore(ignore); )

    return ignore;
}

static boolean directiveHash( c )
    const int c;
{
    boolean ignore = FALSE;

    if (isdigit(c))
	directiveLine(c);
    else
    {
	char directive[MaxDirectiveName];
	DebugStatement( const boolean ignore0 = isIgnore(); )

	readDirective(c, directive, MaxDirectiveName);
	if (stringMatch(directive, "define"))
	    Cpp.directive.state = DRCTV_DEFINE;
	else if (stringMatch(directive, "undef"))
	    Cpp.directive.state = DRCTV_UNDEF;
	else if (strncmp(directive, "if", (size_t)2) == 0)
	    Cpp.directive.state = DRCTV_IF;
	else if (stringMatch(directive, "elif")  ||
		stringMatch(directive, "else"))
	{
	    ignore = setIgnore(isIgnoreBranch());
	    if (! ignore  &&  stringMatch(directive, "else"))
		chooseBranch();
	    Cpp.directive.state = DRCTV_NONE;
	    DebugStatement( if (ignore != ignore0) debugCppIgnore(ignore); )
	}
	else if (stringMatch(directive, "endif"))
	{
	    DebugStatement( debugCppNest(FALSE, Cpp.directive.nestLevel); )
	    ignore = popConditional();
	    Cpp.directive.state = DRCTV_NONE;
	    DebugStatement( if (ignore != ignore0) debugCppIgnore(ignore); )
	}
	else if (stringMatch(directive, "line"))
	    Cpp.directive.state = DRCTV_LINE;
	else	/* "pragma", etc. */
	    Cpp.directive.state = DRCTV_NONE;
    }
    return ignore;
}

/*  Handles a pre-processor directive whose first character is given by "c".
 */
static boolean handleDirective( c )
    const int c;
{
    boolean ignore = FALSE;

    switch (Cpp.directive.state)
    {
	case DRCTV_NONE:	ignore = isIgnore();		break;
	case DRCTV_DEFINE:	directiveDefine(c);		break;
	case DRCTV_HASH:	ignore = directiveHash(c);	break;
	case DRCTV_IF:		ignore = directiveIf(c);	break;
	case DRCTV_LINE:	directiveLine(c);		break;
	case DRCTV_UNDEF:	directiveDefine(c);		break;
    }
    return ignore;
}

/*  Called upon reading of a slash ('/') characters, determines whether a
 *  comment is encountered, and its type.
 */
static Comment isComment()
{
    Comment comment;
    const int next = fileGetc();

    if (next == '*')
	comment = COMMENT_C;
    else if (next == '/')
	comment = COMMENT_CPLUS;
    else
    {
	fileUngetc(next);
	comment = COMMENT_NONE;
    }
    return comment;
}

/*  Skips over a C style comment. According to ANSI specification a comment
 *  is treated as white space, so we perform this subsitution.
 */
static int skipOverCComment()
{
    int c = fileGetc();

    while (c != EOF)
    {
	if (c != '*')
	    c = fileGetc();
	else
	{
	    const int next = fileGetc();

	    if (next != '/')
		c = next;
	    else
	    {
		c = ' ';			/* replace comment with space */
		break;
	    }
	}
    }
    return c;
}

/*  Skips over a C++ style comment.
 */
static int skipOverCplusComment()
{
    int c;

    while ((c = fileGetc()) != EOF)
    {
	if (c == BACKSLASH)
	    fileGetc();			/* throw away next character, too */
	else if (c == NEWLINE)
	    break;
    }
    return c;
}

/*  Skips to the end of a string, returning a special character to
 *  symbolically represent a generic string.
 */
static int skipToEndOfString()
{
    int c;

    while ((c = fileGetc()) != EOF)
    {
	if (c == BACKSLASH)
	    fileGetc();			/* throw away next character, too */
	else if (c == DOUBLE_QUOTE)
	    break;
    }
    return STRING_SYMBOL;		/* symbolic representation of string */
}

/*  Skips to the end of the three (possibly four) 'c' sequence, returning a
 *  special character to symbolically represent a generic character.
 */
static int skipToEndOfChar()
{
    int c;

    while ((c = fileGetc()) != EOF)
    {
	if (c == BACKSLASH)
	    fileGetc();			/* throw away next character, too */
	else if (c == SINGLE_QUOTE)
	    break;
	else if (c == NEWLINE)
	{
	    fileUngetc(c);
	    break;
	}
    }
    return CHAR_SYMBOL;		    /* symbolic representation of character */
}

/*  This function returns the next character, stripping out comments,
 *  C pre-processor directives, and the contents of single and double
 *  quoted strings. In short, strip anything which places a burden upon
 *  the tokenizer.
 */
extern int cppGetc()
{
    boolean directive = FALSE;
    boolean ignore = FALSE;
    int c;

    if (Cpp.ungetch != '\0')
    {
	c = Cpp.ungetch;
	Cpp.ungetch = Cpp.ungetch2;
	Cpp.ungetch2 = '\0';
	return c;	    /* return here to avoid re-calling debugPutc() */
    }
    else do
    {
	c = fileGetc();
	switch (c)
	{
	    case EOF:
		ignore    = FALSE;
		directive = FALSE;
		break;

	    case TAB:
	    case SPACE:
		break;				/* ignore most white space */

	    case NEWLINE:
		if (directive  &&  ! ignore)
		    directive = FALSE;
		Cpp.directive.accept = TRUE;
		break;

	    case DOUBLE_QUOTE:
		Cpp.directive.accept = FALSE;
		c = skipToEndOfString();
		break;

	    case '#':
		if (Cpp.directive.accept)
		{
		    directive = TRUE;
		    Cpp.directive.state  = DRCTV_HASH;
		    Cpp.directive.accept = FALSE;
		}
		break;

	    case SINGLE_QUOTE:
		Cpp.directive.accept = FALSE;
		c = skipToEndOfChar();
		break;

	    case '/':
	    {
		const Comment comment = isComment();

		if (comment == COMMENT_C)
		    c = skipOverCComment();
		else if (comment == COMMENT_CPLUS)
		{
		    c = skipOverCplusComment();
		    if (c == NEWLINE)
			fileUngetc(c);
		}
		else
		    Cpp.directive.accept = FALSE;
		break;
	    }

	    case '\\':
	    {
		int next = fileGetc();

		if (next == NEWLINE)
		    continue;
		else
		    fileUngetc(next);
		break;
	    }

	    default:
		Cpp.directive.accept = FALSE;
		if (directive)
		    ignore = handleDirective(c);
		break;
	}
    } while (directive || ignore);

    DebugStatement( debugPutc(c, DEBUG_CPP); )
    DebugStatement( if (c == NEWLINE)
			debugPrintf(DEBUG_CPP, "%6d: ", getInputFileLine()); )

    return c;
}

/* vi:set tabstop=8 shiftwidth=4: */
