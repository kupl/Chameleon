" Vim syntax file
" Language:	CHILL
" Maintainer:	YoungSang Yoon <image@lgic.co.kr>
" Last change:	2000 May 7
"

" first created by image@lgic.co.kr & modified by paris@lgic.co.kr

" CHILL (CCITT High Level Programming Language) is used for
" developing software of ATM switch at LGIC (LG Information
" & Communications LTd.)


" Remove any old syntax stuff hanging around
syn clear

" A bunch of useful CHILL keywords
syn keyword	chStatement	goto GOTO return RETURN returns RETURNS
syn keyword	chLabel		CASE case ESAC esac
syn keyword	chConditional	if IF else ELSE elsif ELSIF switch SWITCH THEN then FI fi
syn keyword	chLogical	NOT not
syn keyword	chRepeat	while WHILE for FOR do DO od OD TO to
syn keyword 	chProcess	START start STACKSIZE stacksize PRIORITY priority THIS this STOP stop
syn keyword 	chBlock		PROC proc PROCESS process
syn keyword	chSignal	RECEIVE receive SEND send NONPERSISTENT nonpersistent PERSISTENT peristent SET set EVER ever

syn keyword	chTodo		contained TODO FIXME XXX

" String and Character constants
" Highlight special characters (those which have a backslash) differently
syn match	chSpecial	contained "\\x\x\+\|\\\o\{1,3\}\|\\.\|\\$"
syn region	chString	start=+"+ skip=+\\\\\|\\"+ end=+"+ contains=chSpecial
syn match	chCharacter	"'[^\\]'"
syn match	chSpecialCharacter "'\\.'"
syn match	chSpecialCharacter "'\\\o\{1,3\}'"

"when wanted, highlight trailing white space
if exists("ch_space_errors")
  syn match	chSpaceError	"\s*$"
  syn match	chSpaceError	" \+\t"me=e-1
endif

"catch errors caused by wrong parenthesis
syn cluster	chParenGroup	contains=chParenError,chIncluded,chSpecial,chTodo,chUserCont,chUserLabel,chBitField
syn region	chParen		transparent start='(' end=')' contains=ALLBUT,@chParenGroup
syn match	chParenError	")"
syn match	chInParen	contained "[{}]"

"integer number, or floating point number without a dot and with "f".
syn case ignore
syn match	chNumber		"\<\d\+\(u\=l\=\|lu\|f\)\>"
"floating point number, with dot, optional exponent
syn match	chFloat		"\<\d\+\.\d*\(e[-+]\=\d\+\)\=[fl]\=\>"
"floating point number, starting with a dot, optional exponent
syn match	chFloat		"\.\d\+\(e[-+]\=\d\+\)\=[fl]\=\>"
"floating point number, without dot, with exponent
syn match	chFloat		"\<\d\+e[-+]\=\d\+[fl]\=\>"
"hex number
syn match	chNumber		"\<0x\x\+\(u\=l\=\|lu\)\>"
"syn match chIdentifier	"\<[a-z_][a-z0-9_]*\>"
syn case match
" flag an octal number with wrong digits
syn match	chOctalError	"\<0\o*[89]"

if exists("ch_comment_strings")
  " A comment can contain chString, chCharacter and chNumber.
  " But a "*/" inside a chString in a chComment DOES end the comment!  So we
  " need to use a special type of chString: chCommentString, which also ends on
  " "*/", and sees a "*" at the start of the line as comment again.
  " Unfortunately this doesn't very well work for // type of comments :-(
  syntax match	chCommentSkip	contained "^\s*\*\($\|\s\+\)"
  syntax region chCommentString	contained start=+"+ skip=+\\\\\|\\"+ end=+"+ end=+\*/+me=s-1 contains=chSpecial,chCommentSkip
  syntax region chComment2String	contained start=+"+ skip=+\\\\\|\\"+ end=+"+ end="$" contains=chSpecial
  syntax region chComment	start="/\*" end="\*/" contains=chTodo,chCommentString,chCharacter,chNumber,chFloat,chSpaceError
  syntax match  chComment	"//.*" contains=chTodo,chComment2String,chCharacter,chNumber,chSpaceError
else
  syn region	chComment	start="/\*" end="\*/" contains=chTodo,chSpaceError
  syn match	chComment	"//.*" contains=chTodo,chSpaceError
endif
syntax match	chCommentError	"\*/"

syn keyword	chOperator	SIZE size
syn keyword	chType		dcl DCL int INT char CHAR bool BOOL REF ref LOC loc INSTANCE instance
syn keyword	chStructure	struct STRUCT enum ENUM newmode NEWMODE synmode SYNMODE
"syn keyword	chStorageClass
syn keyword	chBlock		PROC proc END end
syn keyword 	chScope		GRANT grant SEIZE seize
syn keyword 	chEDML		select SELECT delete DELETE update UPDATE in IN seq SEQ WHERE where INSERT insert include INCLUDE exclude EXCLUDE
syn keyword 	chBoolConst	true TRUE false FALSE

syn region	chPreCondit	start="^\s*#\s*\(if\>\|ifdef\>\|ifndef\>\|elif\>\|else\>\|endif\>\)" skip="\\$" end="$" contains=chComment,chString,chCharacter,chNumber,chCommentError,chSpaceError
syn region	chIncluded	contained start=+"+ skip=+\\\\\|\\"+ end=+"+
syn match	chIncluded	contained "<[^>]*>"
syn match	chInclude	"^\s*#\s*include\>\s*["<]" contains=chIncluded
"syn match chLineSkip	"\\$"
syn cluster	chPreProcGroup	contains=chPreCondit,chIncluded,chInclude,chDefine,chInParen,chUserLabel
syn region	chDefine		start="^\s*#\s*\(define\>\|undef\>\)" skip="\\$" end="$" contains=ALLBUT,@chPreProcGroup
syn region	chPreProc	start="^\s*#\s*\(pragma\>\|line\>\|warning\>\|warn\>\|error\>\)" skip="\\$" end="$" contains=ALLBUT,@chPreProcGroup

" Highlight User Labels
syn cluster	chMultiGroup	contains=chIncluded,chSpecial,chTodo,chUserCont,chUserLabel,chBitField
syn region	chMulti		transparent start='?' end=':' contains=ALLBUT,@chMultiGroup
" Avoid matching foo::bar() in C++ by requiring that the next char is not ':'
syn match	chUserCont	"^\s*\I\i*\s*:$" contains=chUserLabel
syn match	chUserCont	";\s*\I\i*\s*:$" contains=chUserLabel
syn match	chUserCont	"^\s*\I\i*\s*:[^:]"me=e-1 contains=chUserLabel
syn match	chUserCont	";\s*\I\i*\s*:[^:]"me=e-1 contains=chUserLabel

syn match	chUserLabel	"\I\i*" contained

" Avoid recognizing most bitfields as labels
syn match	chBitField	"^\s*\I\i*\s*:\s*[1-9]"me=e-1
syn match	chBitField	";\s*\I\i*\s*:\s*[1-9]"me=e-1

syn match	chBracket	contained "[<>]"
if !exists("ch_minlines")
  let ch_minlines = 15
endif
exec "syn sync ccomment chComment minlines=" . ch_minlines

if !exists("did_ch_syntax_inits")
  let did_ch_syntax_inits = 1
  " The default methods for highlighting.  Can be overridden later
  hi link chLabel	Label
  hi link chUserLabel	Label
  hi link chConditional	Conditional
"  hi chConditional	term=bold ctermfg=red guifg=red gui=bold

  hi link chRepeat	Repeat
  hi link chProcess	Repeat
  hi link chSignal	Repeat
  hi link chCharacter	Character
  hi link chSpecialCharacter chSpecial
  hi link chNumber	Number
  hi link chFloat	Float
  hi link chOctalError	chError
  hi link chParenError	chError
  hi link chInParen	chError
  hi link chCommentError	chError
  hi link chSpaceError	chError
  hi link chOperator	Operator
  hi link chStructure	Structure
  hi link chBlock 	Operator
  hi link chScope 	Operator
  "hi chEDML     term=underline ctermfg=DarkRed guifg=Red
  hi link chEDML	PreProc
  "hi chBoolConst	term=bold ctermfg=brown guifg=brown
  hi link chBoolConst	Constant
  "hi chLogical	term=bold ctermfg=brown guifg=brown
  hi link chLogical	Constant
  hi link chStorageClass	StorageClass
  hi link chInclude	Include
  hi link chPreProc	PreProc
  hi link chDefine	Macro
  hi link chIncluded	chString
  hi link chError	Error
  hi link chStatement	Statement
  hi link chPreCondit	PreCondit
  hi link chType	Type
  hi link chCommentError	chError
  hi link chCommentString chString
  hi link chComment2String chString
  hi link chCommentSkip	chComment
  hi link chString	String
  hi link chComment	Comment
"  hi chComment	term=None ctermfg=lightblue guifg=lightblue
  hi link chSpecial	SpecialChar
  hi link chTodo	Todo
  hi link chBlock	Statement
  "hi link chIdentifier	Identifier
  hi link chBracket 	Delimiter
endif

let b:current_syntax = "ch"

" vim: ts=8
