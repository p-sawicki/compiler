## ps-lang compiler
Compiler of a custom programming language ps-lang into LLVM IR.

### Language
The language in its current form is quite simple. It supports functions, variables, conditional statements, loops, and basic arithmetic and logical operators.

One feature not commonly seen in programming languages is built-in support for complex numbers.

#### Functions
Function definition consists of its name, return type and list of names and types of arguments. Function body is enclosed in curly braces, for example:
```
fun main :int ()
{
    int s = sum(1, 2);
}
fun sum :int (a :int, b :int) 
{
    return a + b;
}
```
Program execution begins with the `main` function. Currently, in every file there must be a `main` function, which returns type `int` and takes no arguments.

#### Blocks
Blocks are lists of instructions enclosed in curly braces.

Variables may be defined in any block. Once defined, they are visible in the block in which they are defined as well as in any block contained by the block in which they are defined.

Variables may be global, which means they are defined outside of any block. Functions may only be defined outside of any block.

A variable definition shadows variables of the same name defined in outer blocks or globally. A shadowing persists from the variable definition to the end of the block in which it is defined.

#### Variables
A variable definition consists of a type, name, and an initial value. Variables can be used in expressions, provided they are defined beforehand, for example:
```
int a = 4;
int b = 20;
int mul = a * b;
```

#### Types
ps-lang provides 4 types:
- `int` - 64-bit signed integer
- `double` - 64-bit real number
- `complex` - complex number, consisting of two 64-bit real numbers
- `string` - list of ASCII characters

As of now, there is no support for manipulating `string`s, however it is possible to use them in C functions.

If variables of different types are used in an expression, implicit conversion is performed:
- `int` -> `double`
- `int` -> `complex`
- `double` -> `complex`

If types in an expression cannot be unified, compiler reports an error.

#### Complex numbers
ps-lang allows usage of complex numbers in canonical form, eg. `a + bi`, where `a` and `b` may be any expression of type `int` or `double`, for example:
```
complex c = (x + y) + z * w i;
```
The number `c` will have its real part equal to `x + y` and its imaginary part equal to `z * w`.

As the literal `i` is, at the same time, the imaginary unit as well as one of programmers' favorite variable names, `i` may have different meaning depending of where it is used.

`i` is assumed to be the imaginary unit if it used immediately after a closing bracket (`)`), an absolute value delimiter (`|`), a number or a variable name. In all other cases, `i` is assumed to be a variable name. As a result, the following expression is valid:
```
complex c = i + i i;
```
Both the real and imaginary parts of the complex number `c` will have their value equal to `i`.

Several functions are built-in to support complex numbers:
- `Re(z)` - real part of a complex number of value of a real number.
- `Im(z)` - imaginary part of a complex number or 0 in case of a real number.
- `|z|` - absolute value

#### Conditional statements
Conditional statements are made with the `if` instruction, for example:
```
if (a > b)
{
    c = a;
}
```
Instruction blocks beginning after the `if` instruction are executed only if the expression in parenthesis is true.

#### Loops
Loops are made with the `while` instruction, for example:
```
while (i < 10)
{
    a = a + 2 * i;
    i = i + 1;
}
```
Instruction blocks beginning after the `while` instruction are executed as long as the expression in parenthesis is true.

#### Operators
Meaning of available operators is presented in the table below. Operators are listed by priority, from the highest. Expressions in parenthesis will be evaluated first.
Operator | Meaning 
--- | ---
`() \|\|` | function call / absolute value
`+ -` | positive / negative number
`* /` | multiplication / division
`+ -` | addition / subtraction
`< <= > >= == !=` | comparison
`not` | negation
`and` | conjunction
`or` | disjunction
`=` | assignment

#### Formal definition
ps-lang is formally defined in EBNF notation in the file `grammar.ebnf`.

### Run
- Get all prerequisites:
  - CMake 3.10+
  - LLVM 11+
  - zlib
  - C++ compiler with C++17 support
- Configure:
```
cmake -B build
```
- Build:
```
cmake --build build
```
- Run tests:
```
cd build
ctest
```
- Compilation:
  - write code into a text file, for example `test.txt`
  - compile to LLVM IR: `build/compiler test.txt -o test.ll`
  - compile to machine code: `llc test.ll -o test.s`
  - compile to exe: `gcc test.s -o test.exe -no-pie`
  - run: `./text.exe`

Sample programs to compile are available in `parser/tests`.