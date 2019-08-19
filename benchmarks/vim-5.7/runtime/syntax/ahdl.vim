" Vim syn file
" Language:	Altera AHDL
" Maintainer:	John Cook <john.cook@kla-tencor.com>
" Last Change:	1999 Sep 28

" Remove any old syn stuff hanging around
syn clear

"this language is oblivious to case.
syn case ignore

" a bunch of keywords
syn keyword ahdlKeyword assert begin bidir bits buried case clique
syn keyword ahdlKeyword connected_pins constant defaults define design
syn keyword ahdlKeyword device else elsif end for function generate
syn keyword ahdlKeyword gnd help_id if in include input is machine
syn keyword ahdlKeyword node of options others output parameters
syn keyword ahdlKeyword returns states subdesign table then title to
syn keyword ahdlKeyword tri_state_node variable vcc when with

" a bunch of types
syn keyword ahdlIdentifier carry cascade dffe dff exp global
syn keyword ahdlIdentifier jkffe jkff latch lcell mcell memory opendrn
syn keyword ahdlIdentifier soft srffe srff tffe tff tri wire x

syn keyword ahdlMegafunction lpm_and lpm_bustri lpm_clshift lpm_constant
syn keyword ahdlMegafunction lpm_decode lpm_inv lpm_mux lpm_or lpm_xor
syn keyword ahdlMegafunction busmux mux

syn keyword ahdlMegafunction divide lpm_abs lpm_add_sub lpm_compare
syn keyword ahdlMegafunction lpm_counter lpm_mult

syn keyword ahdlMegafunction altdpram csfifo dcfifo scfifo csdpram lpm_ff
syn keyword ahdlMegafunction lpm_latch lpm_shiftreg lpm_ram_dq lpm_ram_io
syn keyword ahdlMegafunction lpm_rom lpm_dff lpm_tff clklock pll ntsc

syn keyword ahdlTodo contained TODO

" String contstants
syn region ahdlString start=+"+  skip=+\\"+  end=+"+

" valid integer number formats (decimal, binary, octal, hex)
syn match ahdlNumber '\<\d\+\>'
syn match ahdlNumber '\<b"\(0\|1\|x\)\+"'
syn match ahdlNumber '\<\(o\|q\)"\o\+"'
syn match ahdlNumber '\<\(h\|x\)"\x\+"'

" operators
syn match   ahdlOperator "[!&#$+\-<>=?:\^]"
syn keyword ahdlOperator not and nand or nor xor xnor
syn keyword ahdlOperator mod div log2 used ceil floor

" one line and multi-line comments
" (define these after ahdlOperator so -- overrides -)
syn match  ahdlComment "--.*" contains=ahdlNumber,ahdlTodo
syn region ahdlComment start="%" end="%" contains=ahdlNumber,ahdlTodo

" other special characters
syn match   ahdlSpecialChar "[\[\]().,;]"

syn sync minlines=1

if !exists("did_ahdl_syn_inits")
  let did_ahdl_syn_inits = 1
  " The default methods for highlighting. Can be overridden later
  hi link ahdlNumber	   ahdlString
  hi link ahdlMegafunction ahdlIdentifier
  hi link ahdlSpecialChar  SpecialChar
  hi link ahdlKeyword	   Statement
  hi link ahdlString	   String
  hi link ahdlComment	   Comment
  hi link ahdlIdentifier   Identifier
  hi link ahdlOperator	   Operator
  hi link ahdlTodo	   Todo
endif

let b:current_syntax = "ahdl"
" vim:ts=8
