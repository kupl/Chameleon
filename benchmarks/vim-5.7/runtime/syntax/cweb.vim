" Vim syntax file
" Language:	CWEB
" Maintainer:	Andreas Scherer <andreas.scherer@pobox.com>
" Last Change:	August 23, 1999

" Details of the CWEB language can be found in the article by Donald E. Knuth
" and Silvio Levy, "The CWEB System of Structured Documentation", included as
" file "cwebman.tex" in the standard CWEB distribution, available for
" anonymous ftp at ftp://labrea.stanford.edu/pub/cweb/.

" TODO: Section names and C/C++ comments should be treated as TeX material.
" TODO: The current version switches syntax highlighting off for section
" TODO: names, and leaves C/C++ comments as such. (On the other hand,
" TODO: switching to TeX mode in C/C++ comments might be colour overkill.)

" Remove any old syntax stuff hanging around
syntax clear

" For starters, read the TeX syntax; TeX syntax items are allowed at the top
" level in the CWEB syntax, e.g., in the preamble.  In general, a CWEB source
" code can be seen as a normal TeX document with some C/C++ material
" interspersed in certain defined regions.
source <sfile>:p:h/tex.vim

" Read the C/C++ syntax too; C/C++ syntax items are treated as such in the
" C/C++ section of a CWEB chunk or in inner C/C++ context in "|...|" groups.
syntax include @webIncludedC <sfile>:p:h/cpp.vim

" Inner C/C++ context (ICC) should be quite simple as it's comprised of
" material in "|...|"; however the naive definition for this region would
" hickup at the innocious "\|" TeX macro.  Note: For the time being we expect
" that an ICC begins either at the start of a line or after some white space.
syntax region webInnerCcontext start="\(^\|[ \t\~`(]\)|" end="|" contains=@webIncludedC,webSectionName,webRestrictedTeX,webIgnoredStuff

" Genuine C/C++ material.  This syntactic region covers both the definition
" part and the C/C++ part of a CWEB section; it is ended by the TeX part of
" the next section.
syntax region webCpart start="@[dfscp<(]" end="@[ \*]" contains=@webIncludedC,webSectionName,webRestrictedTeX,webIgnoredStuff

" Section names contain C/C++ material only in inner context.
syntax region webSectionName start="@[<(]" end="@>" contains=webInnerCcontext contained

" The contents of "control texts" is not treated as TeX material, because in
" non-trivial cases this completely clobbers the syntax recognition.  Instead,
" we highlight these elements as "strings".
syntax region webRestrictedTeX start="@[\^\.:t=q]" end="@>" oneline

" Double-@ means single-@, anywhere in the CWEB source.  (This allows e-mail
" address <someone@@fsf.org> without going into C/C++ mode.)
syntax match webIgnoredStuff "@@"

if !exists("did_cweb_syntax_inits")
  let did_cweb_syntax_inits = 1
  " The default method for highlighting. Can be overridden later.
  hi link webRestrictedTeX String
endif

let b:current_syntax = "cweb"

" vim: ts=8
