/*****************************************************************************
*   $Id: parse.c,v 8.24 2000/04/15 02:50:32 darren Exp $
*
*   Copyright (c) 1996-1999, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions for parsing and scanning C, C++ and Java
*   source files.
*****************************************************************************/

/*============================================================================
=   Include files
============================================================================*/
#include "general.h"

#include <string.h>
#include <setjmp.h>

#include "ctags.h"
#include "debug.h"
#include "entry.h"
#include "get.h"
#include "keyword.h"
#include "main.h"
#include "options.h"
#include "parse.h"
#include "read.h"

/*============================================================================
=   Macros
============================================================================*/
#define MaxFields   6

#define stringValue(a)		#a
#define activeToken(st)		((st)->token[(int)(st)->tokenIndex])
#define parentDecl(st)		((st)->parent == NULL ? \
				DECL_NONE : (st)->parent->declaration)
#define isType(token,t)		(boolean)((token)->type == (t))
#define insideEnumBody(st)	(boolean)((st)->parent == NULL ? FALSE : \
				    ((st)->parent->declaration == DECL_ENUM))
#define isExternCDecl(st,c)	(boolean)((c) == STRING_SYMBOL  && \
		    !(st)->haveQualifyingName  && (st)->scope == SCOPE_EXTERN)

#define isOneOf(c,s)		(boolean)(strchr((s), (c)) != NULL)

/*============================================================================
=   Data declarations
============================================================================*/

enum { NumTokens = 3 };

typedef enum eException {
    ExceptionNone, ExceptionEOF, ExceptionFormattingError,
    ExceptionBraceFormattingError
} exception_t;

/*  Used to specify type of keyword.
 */
typedef enum eKeywordId {
    KEYWORD_NONE,
    KEYWORD_ATTRIBUTE, KEYWORD_ABSTRACT,
    KEYWORD_BOOLEAN, KEYWORD_BYTE,
    KEYWORD_CATCH, KEYWORD_CHAR, KEYWORD_CLASS, KEYWORD_CONST,
    KEYWORD_DOUBLE,
    KEYWORD_ENUM, KEYWORD_EXPLICIT, KEYWORD_EXTERN, KEYWORD_EXTENDS,
    KEYWORD_FINAL, KEYWORD_FLOAT, KEYWORD_FRIEND,
    KEYWORD_IMPLEMENTS, KEYWORD_IMPORT, KEYWORD_INLINE, KEYWORD_INT,
    KEYWORD_INTERFACE,
    KEYWORD_LONG,
    KEYWORD_MUTABLE,
    KEYWORD_NAMESPACE, KEYWORD_NEW, KEYWORD_NATIVE,
    KEYWORD_OPERATOR, KEYWORD_OVERLOAD,
    KEYWORD_PACKAGE, KEYWORD_PRIVATE, KEYWORD_PROTECTED, KEYWORD_PUBLIC,
    KEYWORD_REGISTER,
    KEYWORD_SHORT, KEYWORD_SIGNED, KEYWORD_STATIC, KEYWORD_STRUCT,
    KEYWORD_SYNCHRONIZED,
    KEYWORD_TEMPLATE, KEYWORD_THROW, KEYWORD_THROWS, KEYWORD_TRANSIENT,
    KEYWORD_TRY, KEYWORD_TYPEDEF, KEYWORD_TYPENAME,
    KEYWORD_UNION, KEYWORD_UNSIGNED, KEYWORD_USING,
    KEYWORD_VIRTUAL, KEYWORD_VOID, KEYWORD_VOLATILE,
    KEYWORD_WCHAR_T
} keywordId;

/*  Used to determine whether keyword is valid for the current language and
 *  what its ID is.
 */
typedef struct sKeywordDesc {
    const char *name;
    keywordId id;
    short isValid[LANG_COUNT]; /* indicates languages for which kw is valid */
} keywordDesc;

/*  Used for reporting the type of object parsed by nextToken().
 */
typedef enum eTokenType {
    TOKEN_NONE,		/* none */
    TOKEN_ARGS,		/* a parenthetical pair and its contents */
    TOKEN_BRACE_CLOSE,
    TOKEN_BRACE_OPEN,
    TOKEN_COMMA,		/* the comma character */
    TOKEN_DOUBLE_COLON,	/* double colon indicates nested-name-specifier */
    TOKEN_KEYWORD,
    TOKEN_NAME,		/* an unknown name */
    TOKEN_PACKAGE,	/* a Java package name */
    TOKEN_PAREN_NAME,	/* a single name in parentheses */
    TOKEN_SEMICOLON,	/* the semicolon character */
    TOKEN_SPEC,		/* a storage class specifier, qualifier, type, etc. */
    TOKEN_COUNT
} tokenType;

/*  This describes the scoping of the current statement.
 */
typedef enum eTagScope {
    SCOPE_GLOBAL,		/* no storage class specified */
    SCOPE_STATIC,		/* static storage class */
    SCOPE_EXTERN,		/* external storage class */
    SCOPE_FRIEND,		/* declares access only */
    SCOPE_TYPEDEF,		/* scoping depends upon context */
    SCOPE_COUNT
} tagScope;

typedef enum eDeclaration {
    DECL_NONE,
    DECL_BASE,			/* base type (default) */
    DECL_CLASS,
    DECL_ENUM,
    DECL_FUNCTION,
    DECL_IGNORE,		/* non-taggable "declaration" */
    DECL_INTERFACE,
    DECL_NAMESPACE,
    DECL_NOMANGLE,		/* C++ name demangling block */
    DECL_PACKAGE,
    DECL_STRUCT,
    DECL_UNION,
    DECL_COUNT
} declType;

typedef enum eVisibilityType {
    ACCESS_UNDEFINED,
    ACCESS_PRIVATE,
    ACCESS_PROTECTED,
    ACCESS_PUBLIC,
    ACCESS_DEFAULT,		/* Java-specific */
    ACCESS_COUNT
} accessType;

/*  Information about the parent class of a member (if any).
 */
typedef struct sMemberInfo {
    accessType	access;		/* access of current statement */
    accessType	accessDefault;	/* access default for current statement */
} memberInfo;

typedef struct sTokenInfo {
    tokenType     type;
    keywordId     keyword;
    vString*      name;		/* the name of the token */
    unsigned long lineNumber;	/* line number of tag */
    fpos_t        filePosition;	/* file position of line containing name */
} tokenInfo;

typedef enum eImplementation {
    IMP_DEFAULT,
    IMP_ABSTRACT,
    IMP_VIRTUAL,
    IMP_PURE_VIRTUAL,
    IMP_COUNT
} impType;

/*  Describes the statement currently undergoing analysis.
 */
typedef struct sStatementInfo {
    tagScope	scope;
    declType	declaration;	/* specifier associated with TOKEN_SPEC */
    boolean	gotName;	/* was a name parsed yet? */
    boolean	haveQualifyingName;  /* do we have a name we are considering? */
    boolean	gotParenName;	/* was a name inside parentheses parsed yet? */
    boolean	gotArgs;	/* was a list of parameters parsed yet? */
    boolean	isPointer;	/* is 'name' a pointer? */
    impType	implementation;	/* abstract or concrete implementation? */
    unsigned int tokenIndex;	/* currently active token */
    tokenInfo*	token[(int)NumTokens];
    tokenInfo*	context;	/* accumulated scope of current statement */
    tokenInfo*	blockName;	/* name of current block */
    memberInfo	member;		/* information regarding parent class/struct */
    vString*	parentClasses;	/* parent classes */
    struct sStatementInfo *parent;  /* statement we are nested within */
} statementInfo;

/*  Describes the type of tag being generated.
 */
typedef enum eTagType {
    TAG_UNDEFINED,
    TAG_CLASS,			/* class name */
    TAG_ENUM,			/* enumeration name */
    TAG_ENUMERATOR,		/* enumerator (enumeration value) */
    TAG_FIELD,			/* field (Java) */
    TAG_FUNCTION,		/* function definition */
    TAG_INTERFACE,		/* interface declaration */
    TAG_MEMBER,			/* structure, class or interface member */
    TAG_METHOD,			/* method declaration */
    TAG_NAMESPACE,		/* namespace name */
    TAG_PACKAGE,		/* package name */
    TAG_PROTOTYPE,		/* function prototype or declaration */
    TAG_STRUCT,			/* structure name */
    TAG_TYPEDEF,		/* typedef name */
    TAG_UNION,			/* union name */
    TAG_VARIABLE,		/* variable definition */
    TAG_EXTERN_VAR,		/* external variable declaration */
    TAG_COUNT			/* must be last */
} tagType;

typedef struct sParenInfo {
    boolean isPointer;
    boolean isParamList;
    boolean isKnrParamList;
    boolean isNameCandidate;
    boolean invalidContents;
    boolean nestedArgs;
    unsigned int parameterCount;
} parenInfo;

/*============================================================================
=   Data definitions
============================================================================*/

static jmp_buf Exception;

static const keywordDesc KeywordTable[] = {
    /* 						    C++		*/
    /* 					     ANSI C  |  Java	*/
    /* keyword		keyword ID		 \   |   /  	*/
    { "__attribute__",	KEYWORD_ATTRIBUTE,	{ 1, 1, 0 } },
    { "abstract",	KEYWORD_ABSTRACT,	{ 0, 0, 1 } },
    { "boolean",	KEYWORD_BOOLEAN,	{ 0, 0, 1 } },
    { "byte",		KEYWORD_BYTE,		{ 0, 0, 1 } },
    { "catch",		KEYWORD_CATCH,		{ 0, 1, 0 } },
    { "char",		KEYWORD_CHAR,		{ 1, 1, 1 } },
    { "class",		KEYWORD_CLASS,		{ 0, 1, 1 } },
    { "const",		KEYWORD_CONST,		{ 1, 1, 1 } },
    { "double",		KEYWORD_DOUBLE,		{ 1, 1, 1 } },
    { "enum",		KEYWORD_ENUM,		{ 1, 1, 0 } },
    { "explicit",	KEYWORD_EXPLICIT,	{ 0, 1, 0 } },
    { "extends",	KEYWORD_EXTENDS,	{ 0, 0, 1 } },
    { "extern",		KEYWORD_EXTERN,		{ 1, 1, 0 } },
    { "final",		KEYWORD_FINAL,		{ 0, 0, 1 } },
    { "float",		KEYWORD_FLOAT,		{ 1, 1, 1 } },
    { "friend",		KEYWORD_FRIEND,		{ 0, 1, 0 } },
    { "implements",	KEYWORD_IMPLEMENTS,	{ 0, 0, 1 } },
    { "import",		KEYWORD_IMPORT,		{ 0, 0, 1 } },
    { "inline",		KEYWORD_INLINE,		{ 0, 1, 0 } },
    { "int",		KEYWORD_INT,		{ 1, 1, 1 } },
    { "interface",	KEYWORD_INTERFACE,	{ 0, 0, 1 } },
    { "long",		KEYWORD_LONG,		{ 1, 1, 1 } },
    { "mutable",	KEYWORD_MUTABLE,	{ 0, 1, 0 } },
    { "namespace",	KEYWORD_NAMESPACE,	{ 0, 1, 0 } },
    { "native",		KEYWORD_NATIVE,		{ 0, 0, 1 } },
    { "new",		KEYWORD_NEW,		{ 0, 1, 1 } },
    { "operator",	KEYWORD_OPERATOR,	{ 0, 1, 0 } },
    { "overload",	KEYWORD_OVERLOAD,	{ 0, 1, 0 } },
    { "package",	KEYWORD_PACKAGE,	{ 0, 0, 1 } },
    { "private",	KEYWORD_PRIVATE,	{ 0, 1, 1 } },
    { "protected",	KEYWORD_PROTECTED,	{ 0, 1, 1 } },
    { "public",		KEYWORD_PUBLIC,		{ 0, 1, 1 } },
    { "register",	KEYWORD_REGISTER,	{ 1, 1, 0 } },
    { "short",		KEYWORD_SHORT,		{ 1, 1, 1 } },
    { "signed",		KEYWORD_SIGNED,		{ 1, 1, 0 } },
    { "static",		KEYWORD_STATIC,		{ 1, 1, 1 } },
    { "struct",		KEYWORD_STRUCT,		{ 1, 1, 0 } },
    { "synchronized",	KEYWORD_SYNCHRONIZED,	{ 0, 0, 1 } },
    { "template",	KEYWORD_TEMPLATE,	{ 0, 1, 0 } },
    { "throw",		KEYWORD_THROW,		{ 0, 1, 1 } },
    { "throws",		KEYWORD_THROWS,		{ 0, 0, 1 } },
    { "transient",	KEYWORD_TRANSIENT,	{ 0, 0, 1 } },
    { "try",		KEYWORD_TRY,		{ 0, 1, 0 } },
    { "typedef",	KEYWORD_TYPEDEF,	{ 1, 1, 0 } },
    { "typename",	KEYWORD_TYPENAME,	{ 0, 1, 0 } },
    { "union",		KEYWORD_UNION,		{ 1, 1, 0 } },
    { "unsigned",	KEYWORD_UNSIGNED,	{ 1, 1, 0 } },
    { "using",		KEYWORD_USING,		{ 0, 1, 0 } },
    { "virtual",	KEYWORD_VIRTUAL,	{ 0, 1, 0 } },
    { "void",		KEYWORD_VOID,		{ 1, 1, 1 } },
    { "volatile",	KEYWORD_VOLATILE,	{ 1, 1, 1 } },
    { "wchar_t",	KEYWORD_WCHAR_T,	{ 1, 1, 0 } }
};

/*============================================================================
=   Function prototypes
============================================================================*/
static void initToken __ARGS((tokenInfo *const token));
static void advanceToken __ARGS((statementInfo *const st));
static void retardToken __ARGS((statementInfo *const st));
static tokenInfo *prevToken __ARGS((const statementInfo *const st, unsigned int n));
static void setToken __ARGS((statementInfo *const st, const tokenType type));
static tokenInfo *newToken __ARGS((void));
static void deleteToken __ARGS((tokenInfo *const token));
static const char *accessString __ARGS((const accessType access));
static const char *implementationString __ARGS((const impType imp));
static boolean isContextualKeyword __ARGS((const tokenInfo *const token));
static boolean isContextualStatement __ARGS((const statementInfo *const st));
static boolean isMember __ARGS((const statementInfo *const st));
static void initMemberInfo __ARGS((statementInfo *const st));
static void reinitStatement __ARGS((statementInfo *const st, const boolean comma));
static void initStatement __ARGS((statementInfo *const st, statementInfo *const parent));
static const char *tagName __ARGS((const tagType type));
static int tagLetter __ARGS((const tagType type));
static boolean includeTag __ARGS((const tagType type, const boolean isFileScope));
static tagType declToTagType __ARGS((const declType declaration));
static const char* accessField __ARGS((const statementInfo *const st));
static int addOtherFields __ARGS((const tagType type, const statementInfo *const st, vString *const scope, const char *(*otherFields)[MaxFields]));
static void addContextSeparator __ARGS((vString *const scope));
static void findScopeHierarchy __ARGS((vString *const string, const statementInfo *const st));
static void makeExtraTagEntry __ARGS((tagEntryInfo *const e, vString *const scope));
static void makeTag __ARGS((const tokenInfo *const token, const statementInfo *const st, boolean isFileScope, const tagType type));
static boolean isValidTypeSpecifier __ARGS((const declType declaration));
static void qualifyEnumeratorTag __ARGS((const statementInfo *const st, const tokenInfo *const nameToken));
static void qualifyFunctionTag __ARGS((const statementInfo *const st, const tokenInfo *const nameToken));
static void qualifyFunctionDeclTag __ARGS((const statementInfo *const st, const tokenInfo *const nameToken));
static void qualifyCompoundTag __ARGS((const statementInfo *const st, const tokenInfo *const nameToken));
static void qualifyBlockTag __ARGS((statementInfo *const st, const tokenInfo *const nameToken));
static void qualifyVariableTag __ARGS((const statementInfo *const st, const tokenInfo *const nameToken));
static int skipToNonWhite __ARGS((void));
static void skipToFormattedBraceMatch __ARGS((void));
static void skipToMatch __ARGS((const char *const pair));
static void skipParens __ARGS((void));
static void skipBraces __ARGS((void));
static keywordId analyzeKeyword __ARGS((const char *const name));
static void analyzeIdentifier __ARGS((tokenInfo *const token));
static void readIdentifier __ARGS((tokenInfo *const token, const int firstChar));
static void readPackageName __ARGS((tokenInfo *const token, const int firstChar));
static void readOperator __ARGS((statementInfo *const st));
static void copyToken __ARGS((tokenInfo *const dest, const tokenInfo *const src));
static void setAccess __ARGS((statementInfo *const st, const accessType access));
static void discardTypeList __ARGS((tokenInfo *const token));
static void addParentClass __ARGS((statementInfo *const st, tokenInfo *const token));
static void readParents __ARGS((statementInfo *const st, const int qualifier));
static void readPackage __ARGS((tokenInfo *const token));
static void processName __ARGS((statementInfo *const st));
static void processToken __ARGS((tokenInfo *const token, statementInfo *const st));
static void restartStatement __ARGS((statementInfo *const st));
static void skipMemIntializerList __ARGS((tokenInfo *const token));
static void skipMacro __ARGS((statementInfo *const st));
static boolean skipPostArgumentStuff __ARGS((statementInfo *const st, parenInfo *const info));
static void skipJavaThrows __ARGS((statementInfo *const st));
static void analyzePostParens __ARGS((statementInfo *const st, parenInfo *const info));
static int parseParens __ARGS((statementInfo *const st, parenInfo *const info));
static void initParenInfo __ARGS((parenInfo *const info));
static void analyzeParens __ARGS((statementInfo *const st));
static void addContext __ARGS((statementInfo *const st, const tokenInfo* const token));
static void processColon __ARGS((statementInfo *const st));
static int skipInitializer __ARGS((statementInfo *const st));
static void processInitializer __ARGS((statementInfo *const st));
static void parseIdentifier __ARGS((statementInfo *const st, const int c));
static void parseGeneralToken __ARGS((statementInfo *const st, const int c));
static void nextToken __ARGS((statementInfo *const st));
static statementInfo *newStatement __ARGS((statementInfo *const parent));
static void deleteStatement __ARGS((void));
static void deleteAllStatements __ARGS((void));
static boolean isStatementEnd __ARGS((const statementInfo *const st));
static void checkStatementEnd __ARGS((statementInfo *const st));
static void nest __ARGS((statementInfo *const st, const unsigned int nestLevel));
static void tagCheck __ARGS((statementInfo *const st));
static void createTags __ARGS((const unsigned int nestLevel, statementInfo *const parent));
static void buildCKeywordHash __ARGS((void));
static void init __ARGS((void));

/*============================================================================
=   Function definitions
============================================================================*/

/*----------------------------------------------------------------------------
*-	Token management
----------------------------------------------------------------------------*/

static void initToken( token )
    tokenInfo *const token;
{
    token->type		= TOKEN_NONE;
    token->keyword	= KEYWORD_NONE;
    token->lineNumber	= getFileLine();
    token->filePosition	= getFilePosition();
    vStringClear(token->name);
}

static void advanceToken( st )
    statementInfo *const st;
{
    if (st->tokenIndex >= (unsigned int)NumTokens - 1)
	st->tokenIndex = 0;
    else
	++st->tokenIndex;

    initToken(st->token[st->tokenIndex]);
}

static void retardToken( st )
    statementInfo *const st;
{
    if (st->tokenIndex == 0)
	st->tokenIndex = (unsigned int)NumTokens - 1;
    else
	--st->tokenIndex;
    setToken(st, TOKEN_NONE);
}

static tokenInfo *prevToken( st, n )
    const statementInfo *const st;
    unsigned int n;
{
    unsigned int tokenIndex;
    unsigned int num = (unsigned int)NumTokens;

    Assert(n < num);
    tokenIndex = (st->tokenIndex + num - n) % num;

    return st->token[tokenIndex];
}

static void setToken( st, type )
    statementInfo *const st;
    const tokenType type;
{
    tokenInfo *token;

    token = activeToken(st);
    initToken(token);
    token->type = type;
}

static tokenInfo *newToken()
{
    tokenInfo *const token = (tokenInfo *)eMalloc(sizeof(tokenInfo));

    token->name = vStringNew();
    initToken(token);

    return token;
}

static void deleteToken( token )
    tokenInfo *const token;
{
    if (token != NULL)
    {
	vStringDelete(token->name);
	eFree(token);
    }
}

static const char *accessString( access )
    const accessType access;
{
    static const char *const names[] ={
	    "?", "private", "protected", "public", "default"
    };
    Assert(sizeof(names)/sizeof(names[0]) == ACCESS_COUNT);
    Assert((int)access < ACCESS_COUNT);
    return names[(int)access];
}

static const char *implementationString( imp )
    const impType imp;
{
    static const char *const names[] ={
	    "?", "abstract", "virtual", "pure virtual"
    };
    Assert(sizeof(names)/sizeof(names[0]) == IMP_COUNT);
    Assert((int)imp < IMP_COUNT);
    return names[(int)imp];
}

/*----------------------------------------------------------------------------
*-	Debugging functions
----------------------------------------------------------------------------*/

#ifdef DEBUG

#define boolString(c)	    ((c) ? "TRUE" : "FALSE")

static const char *tokenString __ARGS((const tokenType type));
static const char *tokenString( type )
    const tokenType type;
{
    static const char *const names[] = {
	"none", "args", "}", "{", "comma", "double colon", "keyword", "name",
	"package", "paren-name", "semicolon", "specifier"
    };
    Assert(sizeof(names)/sizeof(names[0]) == TOKEN_COUNT);
    Assert((int)type < TOKEN_COUNT);
    return names[(int)type];
}

static const char *scopeString __ARGS((const tagScope scope));
static const char *scopeString( scope )
    const tagScope scope;
{
    static const char *const names[] = {
	"global", "static", "extern", "friend", "typedef"
    };
    Assert(sizeof(names)/sizeof(names[0]) == SCOPE_COUNT);
    Assert((int)scope < SCOPE_COUNT);
    return names[(int)scope];
}

static const char *declString __ARGS((const declType declaration));
static const char *declString( declaration )
    const declType declaration;
{
    static const char *const names[] = {
	"?", "base", "class", "enum", "function", "ignore", "interface",
	"namespace", "no mangle", "package", "struct", "union",
    };
    Assert(sizeof(names)/sizeof(names[0]) == DECL_COUNT);
    Assert((int)declaration < DECL_COUNT);
    return names[(int)declaration];
}

static const char *keywordString __ARGS((const keywordId keyword));
static const char *keywordString( keyword )
    const keywordId keyword;
{
    const size_t count = sizeof(KeywordTable) / sizeof(KeywordTable[0]);
    const char *name = "none";
    size_t i;

    for (i = 0  ;  i < count  ;  ++i)
    {
	const keywordDesc *p = &KeywordTable[i];

	if (p->id == keyword)
	{
	    name = p->name;
	    break;
	}
    }
    return name;
}

extern void pt __ARGS((tokenInfo *const token));
extern void pt( token )
    tokenInfo *const token;
{
    if (isType(token, TOKEN_NAME))
	printf("type: %-12s: %-13s   line: %lu\n",
	       tokenString(token->type), vStringValue(token->name),
	       token->lineNumber);
    else if (isType(token, TOKEN_KEYWORD))
	printf("type: %-12s: %-13s   line: %lu\n",
	       tokenString(token->type), keywordString(token->keyword),
	       token->lineNumber);
    else
	printf("type: %-12s                  line: %lu\n",
	       tokenString(token->type), token->lineNumber);
}

extern void ps __ARGS((statementInfo *const st));
extern void ps( st )
    statementInfo *const st;
{
    unsigned int i;

    printf("scope: %s   decl: %s   gotName: %s   gotParenName: %s\n",
	   scopeString(st->scope), declString(st->declaration),
	   boolString(st->gotName), boolString(st->gotParenName));
    printf("haveQualifyingName: %s\n", boolString(st->haveQualifyingName));
    printf("access: %s   default: %s\n", accessString(st->member.access),
	   accessString(st->member.accessDefault));
    printf("token  : ");
    pt(activeToken(st));
    for (i = 1  ;  i < (unsigned int)NumTokens  ;  ++i)
    {
	printf("prev %u : ", i);
	pt(prevToken(st, i));
    }
    printf("context: ");
    pt(st->context);
}

#endif

/*----------------------------------------------------------------------------
*-	Statement management
----------------------------------------------------------------------------*/

static boolean isContextualKeyword( token )
    const tokenInfo *const token;
{
    boolean result;
    switch (token->keyword)
    {
	case KEYWORD_CLASS:
	case KEYWORD_ENUM:
	case KEYWORD_INTERFACE:
	case KEYWORD_NAMESPACE:
	case KEYWORD_STRUCT:
	case KEYWORD_UNION:
	    result = TRUE;
	    break;

	default:
	    result = FALSE;
	    break;
    }
    return result;
}

static boolean isContextualStatement( st )
    const statementInfo *const st;
{
    boolean result = FALSE;
    if (st != NULL) switch (st->declaration)
    {
	case DECL_CLASS:
	case DECL_ENUM:
	case DECL_INTERFACE:
	case DECL_NAMESPACE:
	case DECL_STRUCT:
	case DECL_UNION:
	    result = TRUE;
	    break;

	default:
	    result = FALSE;
	    break;
    }
    return result;
}

static boolean isMember( st )
    const statementInfo *const st;
{
    boolean result;
    if (isType(st->context, TOKEN_NAME))
	result = TRUE;
    else
	result = isContextualStatement(st->parent);
    return result;
}

static void initMemberInfo( st )
    statementInfo *const st;
{
    accessType accessDefault = ACCESS_UNDEFINED;

    if (st->parent != NULL) switch (st->parent->declaration)
    {
	case DECL_ENUM:
	case DECL_NAMESPACE:
	case DECL_UNION:
	    accessDefault = ACCESS_UNDEFINED;
	    break;

	case DECL_CLASS:
	    if (isLanguage(LANG_JAVA))
		accessDefault = ACCESS_DEFAULT;
	    else
		accessDefault = ACCESS_PRIVATE;
	    break;

	case DECL_INTERFACE:
	case DECL_STRUCT:
	    accessDefault = ACCESS_PUBLIC;
	    break;

	default: break;
    }
    st->member.accessDefault = accessDefault;
    st->member.access	     = accessDefault;
}

static void reinitStatement( st, partial )
    statementInfo *const st;
    const boolean partial;
{
    unsigned int i;

    if (! partial)
    {
	st->scope = SCOPE_GLOBAL;
	if (isContextualStatement(st->parent))
	    st->declaration = DECL_BASE;
	else
	    st->declaration = DECL_NONE;
    }
    st->gotParenName	= FALSE;
    st->isPointer	= FALSE;
    st->implementation	= IMP_DEFAULT;
    st->gotArgs		= FALSE;
    st->gotName		= FALSE;
    st->haveQualifyingName = FALSE;
    st->tokenIndex	= 0;

    for (i = 0  ;  i < (unsigned int)NumTokens  ;  ++i)
	initToken(st->token[i]);

    initToken(st->context);
    initToken(st->blockName);
    vStringClear(st->parentClasses);

    /*  Init member info.
     */
    if (! partial)
	st->member.access = st->member.accessDefault;
}

static void initStatement( st, parent )
    statementInfo *const st;
    statementInfo *const parent;
{
    st->parent = parent;
    initMemberInfo(st);
    reinitStatement(st, FALSE);
}

/*----------------------------------------------------------------------------
*-	Tag generation functions
----------------------------------------------------------------------------*/

static const char *tagName( type )
    const tagType type;
{
    /*  Note that the strings in this array must correspond to the types in
     *  the tagType enumeration.
     */
    static const char *const names[] = {
	"undefined", "class", "enum", "enumerator", "field", "function",
	"interface", "member", "method", "namespace", "package", "prototype",
	"struct", "typedef", "union", "variable", "externvar"
    };

    Assert(sizeof(names)/sizeof(names[0]) == TAG_COUNT);

    return names[(int)type];
}

static int tagLetter( type )
    const tagType type;
{
    /*  Note that the characters in this list must correspond to the types in
     *  the tagType enumeration.
     */
    const char *const chars = "?cgeffimmnppstuvx";

    Assert(strlen(chars) == TAG_COUNT);

    return chars[(int)type];
}

static boolean includeTag( type, isFileScope )
    const tagType type;
    const boolean isFileScope;
{
    boolean include = FALSE;

    if (isFileScope  &&  ! Option.include.fileScope)
	include = FALSE;
    else if (isLanguage(LANG_JAVA))
    {
	const struct sJavaInclude *const inc = &Option.include.javaTypes;

	switch (type)
	{
	case TAG_CLASS:		include = inc->classNames;	break;
	case TAG_FIELD:		include = inc->fields;		break;
	case TAG_METHOD:	include = inc->methods;		break;
	case TAG_INTERFACE:	include = inc->interfaceNames;	break;
	case TAG_PACKAGE:	include = inc->packageNames;	break;

	default: Assert("Bad type in Java file" == NULL); break;
	}
    }
    else
    {
	const struct sCInclude *const inc = &Option.include.cTypes;

	switch (type)
	{
	case TAG_CLASS:		include = inc->classNames;	break;
	case TAG_ENUM:		include = inc->enumNames;	break;
	case TAG_ENUMERATOR:	include = inc->enumerators;	break;
	case TAG_FUNCTION:	include = inc->functions;	break;
	case TAG_MEMBER:	include = inc->members;		break;
	case TAG_NAMESPACE:	include = inc->namespaceNames;	break;
	case TAG_PROTOTYPE:	include = inc->prototypes;	break;
	case TAG_STRUCT:	include = inc->structNames;	break;
	case TAG_TYPEDEF:	include = inc->typedefs;	break;
	case TAG_UNION:		include = inc->unionNames;	break;
	case TAG_VARIABLE:	include = inc->variables;	break;
	case TAG_EXTERN_VAR:	include = inc->externVars;	break;

	default: Assert("Bad type in C/C++ file" == NULL); break;
	}
    }
    return include;
}

static tagType declToTagType( declaration )
    const declType declaration;
{
    tagType type = TAG_UNDEFINED;

    switch (declaration)
    {
	case DECL_CLASS:	type = TAG_CLASS;	break;
	case DECL_ENUM:		type = TAG_ENUM;	break;
	case DECL_FUNCTION:	type = TAG_FUNCTION;	break;
	case DECL_INTERFACE:	type = TAG_INTERFACE;	break;
	case DECL_NAMESPACE:	type = TAG_NAMESPACE;	break;
	case DECL_STRUCT:	type = TAG_STRUCT;	break;
	case DECL_UNION:	type = TAG_UNION;	break;

	default: Assert("unexpected declaration" == NULL); break;
    }
    return type;
}

static const char* accessField( st )
    const statementInfo *const st;
{
    const char* result = NULL;
    if (isLanguage(LANG_CPP)  &&  st->scope == SCOPE_FRIEND)
	result = "friend";
    else if (st->member.access != ACCESS_UNDEFINED)
	result = accessString(st->member.access);
    return result;
}

static int addOtherFields( type, st, scope, otherFields )
    const tagType type;
    const statementInfo *const st;
    vString *const scope;
    const char *otherFields[2][MaxFields];
{
    int numFields = 0;

    /*  For selected tag types, append an extension flag designating the
     *  parent object in which the tag is defined.
     */
    switch (type)
    {
	default: break;

	case TAG_CLASS:
	case TAG_ENUM:
	case TAG_ENUMERATOR:
	case TAG_FIELD:
	case TAG_FUNCTION:
	case TAG_MEMBER:
	case TAG_METHOD:
	case TAG_PROTOTYPE:
	case TAG_STRUCT:
	case TAG_TYPEDEF:
	case TAG_UNION:
	    if (isMember(st) &&
		! (type == TAG_ENUMERATOR  &&  vStringLength(scope) == 0))
	    {
		Assert(numFields < MaxFields);
		if (isType(st->context, TOKEN_NAME))
		    otherFields[0][numFields] = tagName(TAG_CLASS);
		else
		{
		    Assert(st->parent != NULL);
		    otherFields[0][numFields] =
				tagName(declToTagType(parentDecl(st)));
		}
		otherFields[1][numFields] = vStringValue(scope);
		++numFields;
	    }
	    if ((type == TAG_CLASS  ||  type == TAG_STRUCT) &&
		vStringLength(st->parentClasses) > 0)
	    {
		Assert(numFields < MaxFields);
		otherFields[0][numFields] = "inherits";
		otherFields[1][numFields] = vStringValue(st->parentClasses);
		++numFields;
	    }
	    if (st->implementation != IMP_DEFAULT &&
		(isLanguage(LANG_CPP) || isLanguage(LANG_JAVA)))
	    {
		Assert(numFields < MaxFields);
		otherFields[0][numFields] = "implementation";
		otherFields[1][numFields] = implementationString(st->implementation);
		++numFields;
	    }
	    if (isMember(st) &&
		((isLanguage(LANG_CPP) && Option.include.cTypes.access) ||
		 (isLanguage(LANG_JAVA) && Option.include.javaTypes.access)))
	    {
		if (accessField(st) != NULL)
		{
		    Assert(numFields < MaxFields);
		    otherFields[0][numFields] = "access";
		    otherFields[1][numFields] = accessField(st);
		    ++numFields;
		}
	    }
	    break;
    }
    return numFields;
}

static void addContextSeparator( scope )
    vString *const scope;
{
    if (isLanguage(LANG_C)  ||  isLanguage(LANG_CPP))
	vStringCatS(scope, "::");
    else if (isLanguage(LANG_JAVA))
	vStringCatS(scope, ".");
}

static void findScopeHierarchy( string, st )
    vString *const string;
    const statementInfo *const st;
{
    const char* const anon = "<anonymous>";
    boolean nonAnonPresent = FALSE;

    vStringClear(string);
    if (isType(st->context, TOKEN_NAME))
    {
	vStringCopy(string, st->context->name);
	nonAnonPresent = TRUE;
    }
    if (st->parent != NULL)
    {
	vString *temp = vStringNew();
	const statementInfo *s;
	unsigned depth = 0;

	for (s = st->parent  ;  s != NULL  ;  s = s->parent, ++depth)
	{
	    if (isContextualStatement(s))
	    {
		vStringCopy(temp, string);
		vStringClear(string);
		if (isType(s->blockName, TOKEN_NAME))
		{
		    if (isType(s->context, TOKEN_NAME) &&
			vStringLength(s->context->name) > 0)
		    {
			vStringCat(string, s->context->name);
			addContextSeparator(string);
		    }
		    vStringCat(string, s->blockName->name);
		    nonAnonPresent = TRUE;
		}
		else
		    vStringCopyS(string, anon);
		if (vStringLength(temp) > 0)
		    addContextSeparator(string);
		vStringCat(string, temp);
	    }
	}
	vStringDelete(temp);

	if (! nonAnonPresent)
	    vStringClear(string);
    }
}

static void makeExtraTagEntry( e, scope )
    tagEntryInfo *const e;
    vString *const scope;
{
    if (scope != NULL  &&  vStringLength(scope) > 0  &&
	((isLanguage(LANG_CPP)   &&  Option.include.cTypes.classPrefix)  ||
	 (isLanguage(LANG_JAVA)  &&  Option.include.javaTypes.classPrefix)))
    {
	vString *const scopedName = vStringNew();

	vStringCopy(scopedName, scope);
	addContextSeparator(scopedName);
	vStringCatS(scopedName, e->name);
	e->name = vStringValue(scopedName);
	makeTagEntry(e);
	vStringDelete(scopedName);
    }
}

static void makeTag( token, st, isFileScope, type )
    const tokenInfo *const token;
    const statementInfo *const st;
    boolean isFileScope;
    const tagType type;
{
    /*  Nothing is really of file scope when it appears in a header file.
     */
    isFileScope = (boolean)(isFileScope && ! isHeaderFile());

    if (isType(token, TOKEN_NAME)  &&  vStringLength(token->name) > 0  &&
	includeTag(type, isFileScope))
    {
	const char *otherFields[2][MaxFields];
	vString *scope = vStringNew();
	tagEntryInfo e;

	initTagEntry(&e, vStringValue(token->name));

	e.lineNumber	= token->lineNumber;
	e.filePosition	= token->filePosition;
	e.isFileScope	= isFileScope;
	e.kindName	= tagName(type);
	e.kind		= tagLetter(type);

	findScopeHierarchy(scope, st);
	e.otherFields.count = addOtherFields(type, st, scope, otherFields);
	e.otherFields.label = otherFields[0];
	e.otherFields.value = otherFields[1];

	makeTagEntry(&e);
	makeExtraTagEntry(&e, scope);
	vStringDelete(scope);
    }
}

static boolean isValidTypeSpecifier( declaration )
    const declType declaration;
{
    boolean result;
    switch (declaration)
    {
	case DECL_BASE:
	case DECL_CLASS:
	case DECL_ENUM:
	case DECL_STRUCT:
	case DECL_UNION:
	    result = TRUE;
	    break;

	default:
	    result = FALSE;
	    break;
    }
    return result;
}

static void qualifyEnumeratorTag( st, nameToken )
    const statementInfo *const st;
    const tokenInfo *const nameToken;
{
    if (isType(nameToken, TOKEN_NAME))
	makeTag(nameToken, st, TRUE, TAG_ENUMERATOR);
}

static void qualifyFunctionTag( st, nameToken )
    const statementInfo *const st;
    const tokenInfo *const nameToken;
{
    if (isType(nameToken, TOKEN_NAME))
    {
	const tagType type = isLanguage(LANG_JAVA) ? TAG_METHOD : TAG_FUNCTION;
	const boolean isFileScope =
			(boolean)(st->member.access == ACCESS_PRIVATE ||
			(!isMember(st)  &&  st->scope == SCOPE_STATIC));

	makeTag(nameToken, st, isFileScope, type);
    }
}

static void qualifyFunctionDeclTag( st, nameToken )
    const statementInfo *const st;
    const tokenInfo *const nameToken;
{
    if (! isType(nameToken, TOKEN_NAME))
	;
    else if (isLanguage(LANG_JAVA))
	qualifyFunctionTag(st, nameToken);
    else if (st->scope == SCOPE_TYPEDEF)
	makeTag(nameToken, st, TRUE, TAG_TYPEDEF);
    else if (isValidTypeSpecifier(st->declaration))
	makeTag(nameToken, st, TRUE, TAG_PROTOTYPE);
}

static void qualifyCompoundTag( st, nameToken )
    const statementInfo *const st;
    const tokenInfo *const nameToken;
{
    if (isType(nameToken, TOKEN_NAME))
    {
	const tagType type = declToTagType(st->declaration);

	if (type != TAG_UNDEFINED)
	    makeTag(nameToken, st, (boolean)(! isLanguage(LANG_JAVA)), type);
    }
}

static void qualifyBlockTag( st, nameToken )
    statementInfo *const st;
    const tokenInfo *const nameToken;
{
    switch (st->declaration)
    {
	case DECL_CLASS:
	case DECL_ENUM:
	case DECL_INTERFACE:
	case DECL_NAMESPACE:
	case DECL_STRUCT:
	case DECL_UNION:
	    qualifyCompoundTag(st, nameToken);
	    break;
	default: break;
    }
}

static void qualifyVariableTag( st, nameToken )
    const statementInfo *const st;
    const tokenInfo *const nameToken;
{
    /*	We have to watch that we do not interpret a declaration of the
     *	form "struct tag;" as a variable definition. In such a case, the
     *	token preceding the name will be a keyword.
     */
    if (! isType(nameToken, TOKEN_NAME))
	;
    else if (st->declaration == DECL_IGNORE)
	;
    else if (st->scope == SCOPE_TYPEDEF)
	makeTag(nameToken, st, TRUE, TAG_TYPEDEF);
    else if (st->declaration == DECL_PACKAGE)
	makeTag(nameToken, st, FALSE, TAG_PACKAGE);
    else if (isValidTypeSpecifier(st->declaration))
    {
	if (isMember(st))
	{
	    if (isLanguage(LANG_JAVA))
		makeTag(nameToken, st,
			(boolean)(st->member.access == ACCESS_PRIVATE),
			TAG_FIELD);
	    else if (st->scope == SCOPE_GLOBAL  ||  st->scope == SCOPE_STATIC)
		makeTag(nameToken, st, FALSE, TAG_MEMBER);
	}
	else
	{
	    if (st->scope == SCOPE_EXTERN  ||  ! st->haveQualifyingName)
		makeTag(nameToken, st, FALSE, TAG_EXTERN_VAR);
	    else
		makeTag(nameToken, st, (boolean)(st->scope == SCOPE_STATIC),
			TAG_VARIABLE);
	}
    }
}

/*----------------------------------------------------------------------------
*-	Parsing functions
----------------------------------------------------------------------------*/

/*  Skip to the next non-white character.
 */
static int skipToNonWhite()
{
    int c;

    do
	c = cppGetc();
    while (isspace(c));

    return c;
}

/*  Skips to the next brace in column 1. This is intended for cases where
 *  preprocessor constructs result in unbalanced braces.
 */
static void skipToFormattedBraceMatch()
{
    int c, next;

    c = cppGetc();
    next = cppGetc();
    while (c != EOF  &&  (c != '\n'  ||  next != '}'))
    {
	c = next;
	next = cppGetc();
    }
}

/*  Skip to the matching character indicated by the pair string. If skipping
 *  to a matching brace and any brace is found within a different level of a
 *  #if conditional statement while brace formatting is in effect, we skip to
 *  the brace matched by its formatting. It is assumed that we have already
 *  read the character which starts the group (i.e. the first character of
 *  "pair").
 */
static void skipToMatch( pair )
    const char *const pair;
{
    const boolean braceMatching = (boolean)(strcmp("{}", pair) == 0);
    const boolean braceFormatting = (boolean)(isBraceFormat() && braceMatching);
    const unsigned int initialLevel = getDirectiveNestLevel();
    const int begin = pair[0], end = pair[1];
    const unsigned long lineNumber = getFileLine();
    int matchLevel = 1;
    int c = '\0';

    while (matchLevel > 0  &&  (c = cppGetc()) != EOF)
    {
	if (c == begin)
	{
	    ++matchLevel;
	    if (braceFormatting  &&  getDirectiveNestLevel() != initialLevel)
	    {
		skipToFormattedBraceMatch();
		break;
	    }
	}
	else if (c == end)
	{
	    --matchLevel;
	    if (braceFormatting  &&  getDirectiveNestLevel() != initialLevel)
	    {
		skipToFormattedBraceMatch();
		break;
	    }
	}
    }
    if (c == EOF)
    {
	if (Option.verbose)
	    printf("%s: failed to find match for '%c' at line %lu\n",
		   getFileName(), begin, lineNumber);
	if (braceMatching)
	    longjmp(Exception, (int)ExceptionBraceFormattingError);
	else
	    longjmp(Exception, (int)ExceptionFormattingError);
    }
}

static void skipParens()
{
    const int c = skipToNonWhite();

    if (c == '(')
	skipToMatch("()");
    else
	cppUngetc(c);
}

static void skipBraces()
{
    const int c = skipToNonWhite();

    if (c == '{')
	skipToMatch("{}");
    else
	cppUngetc(c);
}

static keywordId analyzeKeyword( name )
    const char *const name;
{
    const keywordId id = (keywordId)lookupKeyword(name, fileLanguage());
    return id;
}

static void analyzeIdentifier( token )
    tokenInfo *const token;
{
    char *const name = vStringValue(token->name);
    const char *replacement = NULL;
    boolean parensToo = FALSE;

    if (isLanguage(LANG_JAVA)  ||
	! isIgnoreToken(name, &parensToo, &replacement))
    {
	if (replacement != NULL)
	    token->keyword = analyzeKeyword(replacement);
	else
	    token->keyword = analyzeKeyword(vStringValue(token->name));

	if (token->keyword == KEYWORD_NONE)
	    token->type = TOKEN_NAME;
	else
	    token->type = TOKEN_KEYWORD;
    }
    else
    {
	initToken(token);
	if (parensToo)
	{
	    int c = skipToNonWhite();

	    if (c == '(')
		skipToMatch("()");
	}
    }
}

static void readIdentifier( token, firstChar )
    tokenInfo *const token;
    const int firstChar;
{
    vString *const name = token->name;
    int c = firstChar;

    initToken(token);

    do
    {
	vStringPut(name, c);
	c = cppGetc();
    } while (isident(c));
    vStringTerminate(name);
    cppUngetc(c);		/* unget non-identifier character */

    analyzeIdentifier(token);
}

static void readPackageName( token, firstChar )
    tokenInfo *const token;
    const int firstChar;
{
    vString *const name = token->name;
    int c = firstChar;

    initToken(token);

    while (isident(c)  ||  c == '.')
    {
	vStringPut(name, c);
	c = cppGetc();
    }
    vStringTerminate(name);
    cppUngetc(c);		/* unget non-package character */
}

static void readOperator( st )
    statementInfo *const st;
{
    const char *const acceptable = "+-*/%^&|~!=<>,[]";
    const tokenInfo* const prev = prevToken(st,1);
    tokenInfo *const token = activeToken(st);
    vString *const name = token->name;
    int c = skipToNonWhite();

    /*  When we arrive here, we have the keyword "operator" in 'name'.
     */
    if (isType(prev, TOKEN_KEYWORD) && (prev->keyword == KEYWORD_ENUM ||
	 prev->keyword == KEYWORD_STRUCT || prev->keyword == KEYWORD_UNION))
	;	/* ignore "operator" keyword if preceded by these keywords */
    else if (c == '(')
    {
	/*  Verify whether this is a valid function call (i.e. "()") operator.
	 */
	if (cppGetc() == ')')
	{
	    vStringPut(name, ' ');  /* always separate operator from keyword */
	    c = skipToNonWhite();
	    if (c == '(')
		vStringCatS(name, "()");
	}
	else
	{
	    skipToMatch("()");
	    c = cppGetc();
	}
    }
    else if (isident1(c))
    {
	/*  Handle "new" and "delete" operators, and conversion functions
	 *  (per 13.3.1.1.2[2] of the C++ spec).
	 */
	boolean whiteSpace = TRUE;	/* default causes insertion of space */
	do
	{
	    if (isspace(c))
		whiteSpace = TRUE;
	    else
	    {
		if (whiteSpace)
		{
		    vStringPut(name, ' ');
		    whiteSpace = FALSE;
		}
		vStringPut(name, c);
	    }
	    c = cppGetc();
	} while (! isOneOf(c, "(;")  &&  c != EOF);
	vStringTerminate(name);
    }
    else if (isOneOf(c, acceptable))
    {
	vStringPut(name, ' ');	/* always separate operator from keyword */
	do
	{
	    vStringPut(name, c);
	    c = cppGetc();
	} while (isOneOf(c, acceptable));
	vStringTerminate(name);
    }

    cppUngetc(c);

    token->type	= TOKEN_NAME;
    token->keyword = KEYWORD_NONE;
    processName(st);
}

static void copyToken( dest, src )
    tokenInfo *const dest;
    const tokenInfo *const src;
{
    dest->type         = src->type;
    dest->keyword      = src->keyword;
    dest->filePosition = src->filePosition;
    dest->lineNumber   = src->lineNumber;
    vStringCopy(dest->name, src->name);
}

static void setAccess( st, access )
    statementInfo *const st;
    const accessType access;
{
    if (isMember(st))
    {
	if (isLanguage(LANG_CPP))
	{
	    int c = skipToNonWhite();

	    if (c == ':')
		reinitStatement(st, FALSE);
	    else
		cppUngetc(c);

	    st->member.accessDefault = access;
	}
	st->member.access = access;
    }
}

static void discardTypeList( token )
    tokenInfo *const token;
{
    int c = skipToNonWhite();
    while (isident1(c))
    {
	readIdentifier(token, c);
	c = skipToNonWhite();
	if (c == '.'  ||  c == ',')
	    c = skipToNonWhite();
    }
    cppUngetc(c);
}

static void addParentClass( st, token )
    statementInfo *const st;
    tokenInfo *const token;
{
    if (vStringLength(st->parentClasses) > 0)
	vStringPut(st->parentClasses, ',');
    vStringCat(st->parentClasses, token->name);
}

static void readParents ( st, qualifier )
    statementInfo *const st;
    const int qualifier;
{
    tokenInfo *const token = newToken();
    tokenInfo *const parent = newToken();
    int c;

    do
    {
	c = skipToNonWhite();
	if (isident1(c))
	{
	    readIdentifier(token, c);
	    if (isType(token, TOKEN_NAME))
		vStringCat(parent->name, token->name);
	}
	else if (c == qualifier)
	    vStringPut(parent->name, c);
	else if (isType(token, TOKEN_NAME))
	{
	    addParentClass(st, parent);
	    initToken(parent);
	}
    } while (c != '{');
    cppUngetc(c);
    deleteToken(parent);
    deleteToken(token);
}

static void readPackage( token )
    tokenInfo *const token;
{
    Assert(isType(token, TOKEN_KEYWORD));
    readPackageName(token, skipToNonWhite());
    token->type = TOKEN_PACKAGE;
}

static void processName( st )
    statementInfo *const st;
{
    Assert(isType(activeToken(st), TOKEN_NAME));
    if (st->gotName  &&  st->declaration == DECL_NONE)
	st->declaration = DECL_BASE;
    st->gotName = TRUE;
    st->haveQualifyingName = TRUE;
}

static void processToken( token, st )
    tokenInfo *const token;
    statementInfo *const st;
{
    switch (token->keyword)		/* is it a reserved word? */
    {
	default: break;

	case KEYWORD_NONE:	processName(st);			break;
	case KEYWORD_ABSTRACT:	st->implementation = IMP_ABSTRACT;	break;
	case KEYWORD_ATTRIBUTE:	skipParens(); initToken(token);		break;
	case KEYWORD_CATCH:	skipParens(); skipBraces();		break;
	case KEYWORD_CHAR:	st->declaration = DECL_BASE;		break;
	case KEYWORD_CLASS:	st->declaration = DECL_CLASS;		break;
	case KEYWORD_CONST:	st->declaration = DECL_BASE;		break;
	case KEYWORD_DOUBLE:	st->declaration = DECL_BASE;		break;
	case KEYWORD_ENUM:	st->declaration = DECL_ENUM;		break;
	case KEYWORD_EXTENDS:	readParents(st, '.');
				setToken(st, TOKEN_NONE);		break;
	case KEYWORD_FLOAT:	st->declaration = DECL_BASE;		break;
	case KEYWORD_FRIEND:	st->scope	= SCOPE_FRIEND;		break;
	case KEYWORD_IMPLEMENTS:readParents(st, '.');
				setToken(st, TOKEN_NONE);		break;
	case KEYWORD_IMPORT:	st->declaration = DECL_IGNORE;		break;
	case KEYWORD_INT:	st->declaration = DECL_BASE;		break;
	case KEYWORD_INTERFACE: st->declaration = DECL_INTERFACE;	break;
	case KEYWORD_LONG:	st->declaration = DECL_BASE;		break;
	case KEYWORD_NAMESPACE: st->declaration = DECL_NAMESPACE;	break;
	case KEYWORD_OPERATOR:	readOperator(st);			break;
	case KEYWORD_PACKAGE:	readPackage(token);			break;
	case KEYWORD_PRIVATE:	setAccess(st, ACCESS_PRIVATE);		break;
	case KEYWORD_PROTECTED:	setAccess(st, ACCESS_PROTECTED);	break;
	case KEYWORD_PUBLIC:	setAccess(st, ACCESS_PUBLIC);		break;
	case KEYWORD_SHORT:	st->declaration = DECL_BASE;		break;
	case KEYWORD_SIGNED:	st->declaration = DECL_BASE;		break;
	case KEYWORD_STRUCT:	st->declaration = DECL_STRUCT;		break;
	case KEYWORD_THROWS:	discardTypeList(token);			break;
	case KEYWORD_TYPEDEF:	st->scope	= SCOPE_TYPEDEF;	break;
	case KEYWORD_UNION:	st->declaration = DECL_UNION;		break;
	case KEYWORD_UNSIGNED:	st->declaration = DECL_BASE;		break;
	case KEYWORD_USING:	st->declaration = DECL_IGNORE;		break;
	case KEYWORD_VOID:	st->declaration = DECL_BASE;		break;
	case KEYWORD_VOLATILE:	st->declaration = DECL_BASE;		break;
	case KEYWORD_VIRTUAL:	st->implementation = IMP_VIRTUAL;	break;

	case KEYWORD_EXTERN:
	    st->scope = SCOPE_EXTERN;
	    st->declaration = DECL_BASE;
	    break;

	case KEYWORD_STATIC:
	    if (! isLanguage(LANG_JAVA))
		st->scope = SCOPE_STATIC;
	    st->declaration = DECL_BASE;
	    break;
    }
}

/*----------------------------------------------------------------------------
*-	Parenthesis handling functions
----------------------------------------------------------------------------*/

static void restartStatement( st )
    statementInfo *const st;
{
    tokenInfo *const save = newToken();
    tokenInfo *token = activeToken(st);

    copyToken(save, token);
    DebugStatement( if (debug(DEBUG_PARSE)) printf("<ES>");)
    reinitStatement(st, FALSE);
    token = activeToken(st);
    copyToken(token, save);
    deleteToken(save);
    processToken(token, st);
}

/*  Skips over a the mem-initializer-list of a ctor-initializer, defined as:
 *
 *  mem-initializer-list:
 *    mem-initializer, mem-initializer-list
 *
 *  mem-initializer:
 *    [::] [nested-name-spec] class-name (...)
 *    identifier
 */
static void skipMemIntializerList( token )
    tokenInfo *const token;
{
    int c;

    do
    {
	c = skipToNonWhite();
	while (isident1(c)  ||  c == ':')
	{
	    if (c != ':')
		readIdentifier(token, c);
	    c = skipToNonWhite();
	}
	if (c == '<')
	{
	    skipToMatch("<>");
	    c = skipToNonWhite();
	}
	if (c == '(')
	{
	    skipToMatch("()");
	    c = skipToNonWhite();
	}
    } while (c == ',');
    cppUngetc(c);
}

static void skipMacro( st )
    statementInfo *const st;
{
    tokenInfo *const prev2 = prevToken(st, 2);

    if (isType(prev2, TOKEN_NAME))
	retardToken(st);
    skipToMatch("()");
}

/*  Skips over characters following the parameter list. This will be either
 *  non-ANSI style function declarations or C++ stuff. Our choices:
 *
 *  C (K&R):
 *    int func ();
 *    int func (one, two) int one; float two; {...}
 *  C (ANSI):
 *    int func (int one, float two);
 *    int func (int one, float two) {...}
 *  C++:
 *    int foo (...) [const|volatile] [throw (...)];
 *    int foo (...) [const|volatile] [throw (...)] [ctor-initializer] {...}
 *    int foo (...) [const|volatile] [throw (...)] try [ctor-initializer] {...}
 *        catch (...) {...}
 */
static boolean skipPostArgumentStuff( st, info )
    statementInfo *const st;
    parenInfo *const info;
{
    tokenInfo *const token = activeToken(st);
    unsigned int parameters = info->parameterCount;
    unsigned int elementCount = 0;
    boolean restart = FALSE;
    boolean end = FALSE;
    int c = skipToNonWhite();

    do
    {
	switch (c)
	{
	case ')':				break;
	case ':': skipMemIntializerList(token);	break;	/* ctor-initializer */
	case '[': skipToMatch("[]");		break;
	case '=': cppUngetc(c); end = TRUE;	break;
	case '{': cppUngetc(c); end = TRUE;	break;
	case '}': cppUngetc(c); end = TRUE;	break;

	case '(':
	    if (elementCount > 0)
		++elementCount;
	    skipToMatch("()");
	    break;

	case ';':
	    if (parameters == 0  ||  elementCount < 2)
	    {
		cppUngetc(c);
		end = TRUE;
	    }
	    else if (--parameters == 0)
		end = TRUE;
	    break;

	default:
	    if (isident1(c))
	    {
		readIdentifier(token, c);
		switch (token->keyword)
		{
		case KEYWORD_ATTRIBUTE:	skipParens();	break;
		case KEYWORD_THROW:	skipParens();	break;
		case KEYWORD_CONST:			break;
		case KEYWORD_TRY:			break;
		case KEYWORD_VOLATILE:			break;

		case KEYWORD_CATCH:	case KEYWORD_CLASS:
		case KEYWORD_EXPLICIT:	case KEYWORD_EXTERN:
		case KEYWORD_FRIEND:	case KEYWORD_INLINE:
		case KEYWORD_MUTABLE:	case KEYWORD_NAMESPACE:
		case KEYWORD_NEW:	case KEYWORD_OPERATOR:
		case KEYWORD_OVERLOAD:	case KEYWORD_PRIVATE:
		case KEYWORD_PROTECTED:	case KEYWORD_PUBLIC:
		case KEYWORD_STATIC:	case KEYWORD_TEMPLATE:
		case KEYWORD_TYPEDEF:	case KEYWORD_TYPENAME:
		case KEYWORD_USING:	case KEYWORD_VIRTUAL:
		    /*  Never allowed within parameter declarations.
		     */
		    restart = TRUE;
		    end = TRUE;
		    break;

		default:
		    if (isType(token, TOKEN_NONE))
			;
		    else if (info->isKnrParamList  &&  info->parameterCount > 0)
			++elementCount;
		    else
		    {
			/*  If we encounter any other identifier immediately
			 *  following an empty parameter list, this is almost
			 *  certainly one of those Microsoft macro "thingies"
			 *  that the automatic source code generation sticks
			 *  in. Terminate the current statement.
			 */
			restart = TRUE;
			end = TRUE;
		    }
		    break;
		}
	    }
	}
	if (! end)
	{
	    c = skipToNonWhite();
	    if (c == EOF)
		end = TRUE;
	}
    } while (! end);

    if (restart)
	restartStatement(st);
    else
	setToken(st, TOKEN_NONE);

    return (boolean)(c != EOF);
}

static void skipJavaThrows(st)
    statementInfo *const st;
{
    tokenInfo *const token = activeToken(st);
    int c = skipToNonWhite();

    if (isident1(c))
    {
	readIdentifier(token, c);
	if (token->keyword == KEYWORD_THROWS)
	{
	    do
	    {
		c = skipToNonWhite();
		if (isident1(c))
		{
		    readIdentifier(token, c);
		    c = skipToNonWhite();
		}
	    } while (c == '.'  ||  c == ',');
	}
    }
    cppUngetc(c);
    setToken(st, TOKEN_NONE);
}

static void analyzePostParens( st, info )
    statementInfo *const st;
    parenInfo *const info;
{
    const unsigned long lineNumber = getFileLine();
    int c = skipToNonWhite();

    cppUngetc(c);
    if (isOneOf(c, "{;,="))
	;
    else if (isLanguage(LANG_JAVA))
	skipJavaThrows(st);
    else
    {
	if (! skipPostArgumentStuff(st, info))
	{
	    if (Option.verbose)
	   printf("%s: confusing argument declarations beginning at line %lu\n",
		    getFileName(), lineNumber);
	    longjmp(Exception, (int)ExceptionFormattingError);
	}
    }
}

static int parseParens( st, info )
    statementInfo *const st;
    parenInfo *const info;
{
    tokenInfo *const token = activeToken(st);
    unsigned int identifierCount = 0;
    unsigned int depth = 1;
    boolean firstChar = TRUE;
    int nextChar = '\0';

    info->parameterCount = 1;
    do
    {
	int c = skipToNonWhite();

	switch (c)
	{
	    case '&':
	    case '*':
		info->isPointer = TRUE;
		info->isKnrParamList = FALSE;
		initToken(token);
		break;

	    case ':':
		info->isKnrParamList = FALSE;
		break;

	    case '.':
		info->isNameCandidate = FALSE;
		info->isKnrParamList = FALSE;
		break;

	    case ',':
		info->isNameCandidate = FALSE;
		if (info->isKnrParamList)
		{
		    ++info->parameterCount;
		    identifierCount = 0;
		}
		break;

	    case '=':
		info->isKnrParamList = FALSE;
		info->isNameCandidate = FALSE;
		if (firstChar)
		{
		    info->isParamList = FALSE;
		    skipMacro(st);
		    depth = 0;
		}
		break;

	    case '[':
		info->isKnrParamList = FALSE;
		skipToMatch("[]");
		break;

	    case '<':
		info->isKnrParamList = FALSE;
		skipToMatch("<>");
		break;

	    case ')':
		if (firstChar)
		    info->parameterCount = 0;
		--depth;
		break;

	    case '(':
		info->isKnrParamList = FALSE;
		if (firstChar)
		{
		    info->isNameCandidate = FALSE;
		    cppUngetc(c);
		    skipMacro(st);
		    depth = 0;
		}
		else if (isType(token, TOKEN_PAREN_NAME))
		{
		    c = skipToNonWhite();
		    if (c == '*')	/* check for function pointer */
		    {
			skipToMatch("()");
			c = skipToNonWhite();
			if (c == '(')
			    skipToMatch("()");
		    }
		    else
		    {
			cppUngetc(c);
			cppUngetc('(');
			info->nestedArgs = TRUE;
		    }
		}
		else
		    ++depth;
		break;

	    default:
		if (isident1(c))
		{
		    if (++identifierCount > 1)
			info->isKnrParamList = FALSE;
		    readIdentifier(token, c);
		    if (isType(token, TOKEN_NAME)  &&  info->isNameCandidate)
			token->type = TOKEN_PAREN_NAME;
		    else if (isType(token, TOKEN_KEYWORD))
		    {
			info->isKnrParamList = FALSE;
			info->isNameCandidate = FALSE;
		    }
		}
		else
		{
		    info->isParamList     = FALSE;
		    info->isKnrParamList  = FALSE;
		    info->isNameCandidate = FALSE;
		    info->invalidContents = TRUE;
		}
		break;
	}
	firstChar = FALSE;
    } while (! info->nestedArgs  &&  depth > 0  &&
	     (info->isKnrParamList  ||  info->isNameCandidate));

    if (! info->nestedArgs) while (depth > 0)
    {
	skipToMatch("()");
	--depth;
    }

    if (! info->isNameCandidate)
	initToken(token);

    return nextChar;
}

static void initParenInfo( info )
    parenInfo *const info;
{
    info->isPointer		= FALSE;
    info->isParamList		= TRUE;
    info->isKnrParamList	= TRUE;
    info->isNameCandidate	= TRUE;
    info->invalidContents	= FALSE;
    info->nestedArgs		= FALSE;
    info->parameterCount	= 0;
}

static void analyzeParens( st )
    statementInfo *const st;
{
    tokenInfo *const prev = prevToken(st, 1);

    if (! isType(prev, TOKEN_NONE))    /* in case of ignored enclosing macros */
    {
	tokenInfo *const token = activeToken(st);
	parenInfo info;
	int c;

	initParenInfo(&info);
	parseParens(st, &info);
	c = skipToNonWhite();
	cppUngetc(c);
	if (info.invalidContents)
	    reinitStatement(st, FALSE);
	else if (info.isNameCandidate  &&  isType(token, TOKEN_PAREN_NAME)  &&
		 ! st->gotParenName  &&
		 (! st->haveQualifyingName  ||  isOneOf(c, "(=")  ||
		  (st->declaration == DECL_NONE  &&  isOneOf(c, ",;"))))
	{
	    token->type = TOKEN_NAME;
	    processName(st);
	    st->gotParenName = TRUE;
	    if (! (c == '('  &&  info.nestedArgs))
		st->isPointer = info.isPointer;
	}
	else if (! st->gotArgs  &&  info.isParamList)
	{
	    st->gotArgs = TRUE;
	    setToken(st, TOKEN_ARGS);
	    advanceToken(st);
	    analyzePostParens(st, &info);
	}
	else
	    setToken(st, TOKEN_NONE);
    }
}

/*----------------------------------------------------------------------------
*-	Token parsing functions
----------------------------------------------------------------------------*/

static void addContext( st, token )
    statementInfo *const st;
    const tokenInfo *const token;
{
    if (isType(token, TOKEN_NAME))
    {
	if (vStringLength(st->context->name) > 0)
	{
	    if (isLanguage(LANG_C)  ||  isLanguage(LANG_CPP))
		vStringCatS(st->context->name, "::");
	    else if (isLanguage(LANG_JAVA))
		vStringCatS(st->context->name, ".");
	}
	vStringCat(st->context->name, token->name);
	st->context->type = TOKEN_NAME;
    }
}

static void processColon( st )
    statementInfo *const st;
{
    const int c = skipToNonWhite();
    const boolean doubleColon = (boolean)(c == ':');

    if (doubleColon)
    {
	setToken(st, TOKEN_DOUBLE_COLON);
	st->haveQualifyingName = FALSE;
    }
    else
    {
	cppUngetc(c);
	if (isLanguage(LANG_CPP)  && (
	    st->declaration == DECL_CLASS  ||  st->declaration == DECL_STRUCT))
	{
	    readParents(st, ':');
	}
    }
}

/*  Skips over any initializing value which may follow an '=' character in a
 *  variable definition.
 */
static int skipInitializer( st )
    statementInfo *const st;
{
    boolean done = FALSE;
    int c;

    while (! done)
    {
	c = skipToNonWhite();

	if (c == EOF)
	    longjmp(Exception, (int)ExceptionFormattingError);
	else switch (c)
	{
	    case ',':
	    case ';': done = TRUE; break;

	    case '0':
		if (st->implementation == IMP_VIRTUAL)
		    st->implementation = IMP_PURE_VIRTUAL;
		break;

	    case '[': skipToMatch("[]"); break;
	    case '(': skipToMatch("()"); break;
	    case '{': skipToMatch("{}"); break;

	    case '}':
		if (insideEnumBody(st))
		    done = TRUE;
		else if (! isBraceFormat())
		{
		    if (Option.verbose)
		       printf("%s: unexpected closing brace at line %lu\n",
			      getFileName(), getFileLine());
		    longjmp(Exception, (int)ExceptionBraceFormattingError);
		}
		break;

	    default: break;
	}
    }
    return c;
}

static void processInitializer( st )
    statementInfo *const st;
{
    const boolean inEnumBody = insideEnumBody(st);
    const int c = skipInitializer(st);

    if (c == ';')
	setToken(st, TOKEN_SEMICOLON);
    else if (c == ',')
	setToken(st, TOKEN_COMMA);
    else if ('}'  &&  inEnumBody)
    {
	cppUngetc(c);
	setToken(st, TOKEN_COMMA);
    }
    if (st->scope == SCOPE_EXTERN)
	st->scope = SCOPE_GLOBAL;
}

static void parseIdentifier( st, c )
    statementInfo *const st;
    const int c;
{
    tokenInfo *const token = activeToken(st);

    readIdentifier(token, c);
    if (! isType(token, TOKEN_NONE))
	processToken(token, st);
}

static void parseGeneralToken( st, c )
    statementInfo *const st;
    const int c;
{
    const tokenInfo *const prev = prevToken(st, 1);

    if (isident1(c))
    {
	parseIdentifier(st, c);
	if (isType(st->context, TOKEN_NAME) &&
	    isType(activeToken(st), TOKEN_NAME) && isType(prev, TOKEN_NAME))
	{
	    initToken(st->context);
	}
    }
    else if (isExternCDecl(st, c))
    {
	st->declaration = DECL_NOMANGLE;
	st->scope = SCOPE_GLOBAL;
    }
}

/*  Reads characters from the pre-processor and assembles tokens, setting
 *  the current statement state.
 */
static void nextToken( st )
    statementInfo *const st;
{
    tokenInfo *token = activeToken(st);

    do
    {
	int c = skipToNonWhite();

	switch (c)
	{
	    case EOF: longjmp(Exception, (int)ExceptionEOF);		break;
	    case '(': analyzeParens(st);  token = activeToken(st);	break;
	    case '*': st->haveQualifyingName = FALSE;			break;
	    case ',': setToken(st, TOKEN_COMMA);			break;
	    case ':': processColon(st);					break;
	    case ';': setToken(st, TOKEN_SEMICOLON);			break;
	    case '<': skipToMatch("<>");				break;
	    case '=': processInitializer(st);				break;
	    case '[': skipToMatch("[]");				break;
	    case '{': setToken(st, TOKEN_BRACE_OPEN);			break;
	    case '}': setToken(st, TOKEN_BRACE_CLOSE);			break;
	    default:  parseGeneralToken(st, c);				break;
	}
    } while (isType(token, TOKEN_NONE));
}

/*----------------------------------------------------------------------------
*-	Scanning support functions
----------------------------------------------------------------------------*/

static statementInfo *CurrentStatement = NULL;

static statementInfo *newStatement( parent )
    statementInfo *const parent;
{
    statementInfo *const st = (statementInfo *)eMalloc(sizeof(statementInfo));
    unsigned int i;

    for (i = 0  ;  i < (unsigned int)NumTokens  ;  ++i)
	st->token[i] = newToken();

    st->context = newToken();
    st->blockName = newToken();
    st->parentClasses = vStringNew();

    initStatement(st, parent);
    CurrentStatement = st;

    return st;
}

static void deleteStatement()
{
    statementInfo *const st = CurrentStatement;
    statementInfo *const parent = st->parent;
    unsigned int i;

    for (i = 0  ;  i < (unsigned int)NumTokens  ;  ++i)
    {
	deleteToken(st->token[i]);        st->token[i] = NULL;
    }
    deleteToken(st->blockName);           st->blockName = NULL;
    deleteToken(st->context);             st->context = NULL;
    vStringDelete(st->parentClasses);     st->parentClasses = NULL;
    eFree(st);
    CurrentStatement = parent;
}

static void deleteAllStatements()
{
    while (CurrentStatement != NULL)
	deleteStatement();
}

static boolean isStatementEnd( st )
    const statementInfo *const st;
{
    const tokenInfo *const token = activeToken(st);
    boolean isEnd;

    if (isType(token, TOKEN_SEMICOLON))
	isEnd = TRUE;
    else if (isType(token, TOKEN_BRACE_CLOSE))
	isEnd = (boolean)(! isContextualStatement(st));
    else
	isEnd = FALSE;

    return isEnd;
}

static void checkStatementEnd( st )
    statementInfo *const st;
{
    const tokenInfo *const token = activeToken(st);

    if (isType(token, TOKEN_COMMA))
	reinitStatement(st, TRUE);
    else if (isStatementEnd(st))
    {
	DebugStatement( if (debug(DEBUG_PARSE)) printf("<ES>"); )
	reinitStatement(st, FALSE);
	cppEndStatement();
    }
    else
    {
	cppBeginStatement();
	advanceToken(st);
    }
}

static void nest( st, nestLevel )
    statementInfo *const st;
    const unsigned int nestLevel;
{
    switch (st->declaration)
    {
	case DECL_CLASS:
	case DECL_ENUM:
	case DECL_INTERFACE:
	case DECL_NAMESPACE:
	case DECL_NOMANGLE:
	case DECL_STRUCT:
	case DECL_UNION:
	    createTags(nestLevel, st);
	    break;

	default:
	    skipToMatch("{}");
	    break;
    }
    advanceToken(st);
    setToken(st, TOKEN_BRACE_CLOSE);
}

static void tagCheck( st )
    statementInfo *const st;
{
    const tokenInfo *const token = activeToken(st);
    const tokenInfo *const prev  = prevToken(st, 1);
    const tokenInfo *const prev2 = prevToken(st, 2);

    switch (token->type)
    {
	case TOKEN_NAME:
	    if (insideEnumBody(st))
		qualifyEnumeratorTag(st, token);
	    break;

	case TOKEN_PACKAGE:
	    if (st->haveQualifyingName)
		makeTag(token, st, FALSE, TAG_PACKAGE);
	    break;

	case TOKEN_BRACE_OPEN:
	    if (isType(prev, TOKEN_ARGS))
	    {
		if (st->haveQualifyingName)
		{
		    st->declaration = DECL_FUNCTION;
		    if (isType (prev, TOKEN_NAME))
			copyToken(st->blockName, prev2);
		    qualifyFunctionTag(st, prev2);
		}
	    }
	    else if (isContextualStatement(st))
	    {
		if (isType (prev, TOKEN_NAME))
		    copyToken(st->blockName, prev);
		qualifyBlockTag(st, prev);
	    }
	    break;

	case TOKEN_SEMICOLON:
	case TOKEN_COMMA:
	    if (insideEnumBody(st))
		;
	    else if (isType(prev, TOKEN_NAME))
	    {
		if (isContextualKeyword(prev2))
		    st->scope = SCOPE_EXTERN;
		qualifyVariableTag(st, prev);
	    }
	    else if (isType(prev, TOKEN_ARGS)  &&  isType(prev2, TOKEN_NAME))
	    {
		if (st->isPointer)
		    qualifyVariableTag(st, prev2);
		else
		    qualifyFunctionDeclTag(st, prev2);
	    }
	    break;

	default: break;
    }
}

/*  Parses the current file and decides whether to write out and tags that
 *  are discovered.
 */
static void createTags( nestLevel, parent )
    const unsigned int nestLevel;
    statementInfo *const parent;
{
    statementInfo *const st = newStatement(parent);

    DebugStatement( if (nestLevel > 0) debugParseNest(TRUE, nestLevel); )
    while (TRUE)
    {
	tokenInfo *token;

	nextToken(st);
	token = activeToken(st);
	if (isType(token, TOKEN_BRACE_CLOSE))
	{
	    if (nestLevel > 0)
		break;
	    else
	    {
		if (Option.verbose)
		    printf("%s: unexpected closing brace at line %lu\n",
			   getFileName(), getFileLine());
		longjmp(Exception, (int)ExceptionBraceFormattingError);
	    }
	}
	else if (isType(token, TOKEN_DOUBLE_COLON))
	{
	    addContext(st, prevToken(st, 1));
	    advanceToken(st);
	}
	else
	{
	    tagCheck(st);
	    if (isType(token, TOKEN_BRACE_OPEN))
		nest(st, nestLevel + 1);
	    checkStatementEnd(st);
	}
    }
    deleteStatement();
    DebugStatement( if (nestLevel > 0) debugParseNest(FALSE, nestLevel - 1); )
}

static void buildCKeywordHash()
{
    const size_t count = sizeof(KeywordTable) / sizeof(KeywordTable[0]);
    size_t i;

    for (i = 0  ;  i < count  ;  ++i)
    {
	const keywordDesc *p = &KeywordTable[i];

	if (p->isValid[0])
	    addKeyword(p->name, LANG_C, (int)p->id);
	if (p->isValid[1])
	    addKeyword(p->name, LANG_CPP, (int)p->id);
	if (p->isValid[2])
	    addKeyword(p->name, LANG_JAVA, (int)p->id);
    }
}

static void init()
{
    static boolean isInitialized = FALSE;

    if (! isInitialized)
    {
	buildCKeywordHash();
	isInitialized = TRUE;
    }
}

extern boolean createCTags( passCount )
    const unsigned int passCount;
{
    exception_t exception;
    boolean retry;

    Assert(passCount < 3);
    init();
    cppInit((boolean)(passCount > 1));

    exception = (exception_t)setjmp(Exception);
    retry = FALSE;
    if (exception == ExceptionNone)
	createTags(0, NULL);
    else
    {
	deleteAllStatements();
	if (exception == ExceptionBraceFormattingError  &&  passCount == 1)
	{
	    retry = TRUE;
	    if (Option.verbose)
		printf(
		   "%s: retrying file with fallback brace matching algorithm\n",
		    getFileName());
	}
    }
    cppTerminate();
    return retry;
}

/* vi:set tabstop=8 shiftwidth=4: */
