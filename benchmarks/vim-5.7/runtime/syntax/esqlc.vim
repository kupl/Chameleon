" Vim syntax file
" Language:	ESQL-C
" Maintainer:	Jonathan A. George <jageorge@tel.gte.com>
" Last Change:	1998 Aug 12

" Remove any old syntax stuff hanging around
syntax clear

" Read the C++ syntax to start with
source <sfile>:p:h/cpp.vim

" ESQL-C extentions

syntax keyword esqlcPreProc	EXEC SQL INCLUDE

syntax case ignore

syntax keyword esqlcPreProc	begin end declare section database open execute
syntax keyword esqlcPreProc	prepare fetch goto continue found sqlerror work

syntax keyword esqlcKeyword	access add as asc by check cluster column
syntax keyword esqlcKeyword	compress connect current decimal
syntax keyword esqlcKeyword	desc exclusive file from group
syntax keyword esqlcKeyword	having identified immediate increment index
syntax keyword esqlcKeyword	initial into is level maxextents mode modify
syntax keyword esqlcKeyword	nocompress nowait of offline on online start
syntax keyword esqlcKeyword	successful synonym table then to trigger uid
syntax keyword esqlcKeyword	unique user validate values view whenever
syntax keyword esqlcKeyword	where with option order pctfree privileges
syntax keyword esqlcKeyword	public resource row rowlabel rownum rows
syntax keyword esqlcKeyword	session share size smallint

syntax keyword esqlcOperator	not and or
syntax keyword esqlcOperator	in any some all between exists
syntax keyword esqlcOperator	like escape
syntax keyword esqlcOperator	intersect minus
syntax keyword esqlcOperator	prior distinct
syntax keyword esqlcOperator	sysdate

syntax keyword esqlcStatement	alter analyze audit comment commit create
syntax keyword esqlcStatement	delete drop explain grant insert lock noaudit
syntax keyword esqlcStatement	rename revoke rollback savepoint select set
syntax keyword esqlcStatement	truncate update

if !exists("did_esqlc_syntax_inits")
  let did_esqlc_syntax_inits = 1
  highlight link esqlcOperator	Operator
  highlight link esqlcStatement	Statement
  highlight link esqlcKeyword	esqlcSpecial
  highlight link esqlcSpecial	Special
  highlight link esqlcPreProc	PreProc
endif

let b:current_syntax = "esqlc"
