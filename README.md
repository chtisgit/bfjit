This is one of my old projects; it was created in 2011.
It doesn't work on platforms that don't allow execution of heap.
Today I fixed it for 64bit Linux. Compile with -m32 flag. 

# BFJIT

bfjit is a fast, optimizing heap/aot compiler for brainfuck.
The program directly translates the brainfuck code into 80386 opcodes, 
writes these onto the heap and then executes them.

## INDEX

1. Usage
2. Brainfuck commands
3. Speed
4. Portability


## 1. USAGE

The program should be executed from the command line like this:

	C:\> bfjit [file] [-16|-32] [-0|-255] [-s start] [-m size]

	file            name of input file
	-m              size of programs memory (standard 32768)
	-s              start point of the memory pointer (>=0) (standard 16384)
	-16             set cell size to 16 bit
	-32             set cell size to 32 bit
	-0              writes 0 into cell, if eof is read
	-255            writes 255 into cell, if eof is read
	-hhelp message

- if you call bfjit without a filename, you can type the program into the
  commandline.
- the parameters don't have to be written in this order.


## 2. COMMANDS

The stock commands + - > < . , [ ] and additionally ! and # are supported.

bang (!) is only recognized when in console mode. It seperates code from data (that
would be impossible otherwise)

hash (#) enters debug query mode. Enter a single number or two numbers seperated by
a comma (note, that the second one must be greater than the first one).
Then the contents of this/these cell(s) are echoed. Example:

	-5,5

	[-5]    = 00   0
	[-4]    = 00   0
	[-3]    = 00   0
	[-2]    = 00   0
	[-1]    = 00   0
	[0]     = 00   0
	[1]     = 00   0
	[2]     = 00   0
	[3]     = 00   0
	[4]     = 00   0
	[5]     = 00   0


## 3. SPEED

bfjit is able to compete with the most brainfuck compilers and is about
24.000 and much more times faster than most interpreters. Well, how is this 
speed achieved? The biggest part of this is made up by the compilation but
bfjit's optimization does also speed the progress up a little.

Optimization is done by pooling connected ( +'s and -'s ) and ( >'s and <'s )
and combine them in one instruction (by the way: bfjit tries to generate
the shortest x86 Instruction possible). 
The constructs [-] [>>] and [<<] are replaced by a predefined opcode sequence.
You can deactivate the replacement of the latter two by putting commentation marks
before '#define USE_STRINGFUNCTIONS'.


## 4. PORTABILITY

Obviously, bfjit currently only works on 80386 processor family, because it generates
opcodes for this plattform and relies on some inline assembler instruction in its code.

Nevertheless it should run on EVERY 80386 32/64 bit OS! 
(64 bit OS'es may not support bfjit's 16 bit mode)
The OS only has to provide a C compiler a libc and allow execution of heap.

I tried to write bfjit so that it will be easy to port it to other plattforms.

