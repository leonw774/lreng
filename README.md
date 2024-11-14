# ɭeŋ

The interpreter of lreng/ɭeŋ, a simple programming language, written in C.

## Usage

```
./lreng {file_path} [-d]
```

The optional `-d` flag outputs debug information.

## Data types

Everything is object. There are 4 types of objects:
- Number: Numbers in lreng are all rational numbers 
  - You can represent numbers in decimal with point: `3.14159`
  - You can represent integers in binary and heximal: `0b110011`, `0xc0de`
  - You can represent the code of printable and escapable ASCII characters. For example, `'A'` evaluates to `65`, `'\\'` is `92`, and `'\n'` is `10`
- Pair: Store the references to two objects, tagged `left` and `right`;
- Function: Store executable code and an optional argument identifier
- Null: The unit type. Has key word `null`.

## Variables

Variable identifiers should match regex `[_A-Za-z][_A-Za-z0-9]+`. All variables are immutable after initialization by the assignment operator.

## Operators

### Arithmetics

The `+`, `-`, `*`, `/`, `%` (modulo), `^` (exponent) only accept numbers.

- The division is real number division so there is *no* rounding happening.
- The module of `a` and `b` returns a smallest real number `r` such that `a / (b - r)` is a integer.
- The exponent only allow *integer* power, since otherwise the result will not be rational.

### Comparison

The comparison operators compare two objects by their values. The `>`, `<`, `>=`, `<=` can only compare numbers. The `==`, `!=` can compare all data types.

For pair objects, it compares their referenced objects recursively. They return number `1` if the comparison result is true, otherwise `0`.

### Logic

The logic operators `!` (NOT), `&` (AND) and `|` (OR) return `0` and `1` like comparison operators.

The boolean value objects:
- a number object is true if it is not 0, otherwise false
- The null object is always false.
- Pair and function objects are always true.

They do *not* short-circuit. If you want something like an if-statement, use conditional operators `&&` and `||`.

### Short-circuit logic

The `&&` (IF-AND) and  `||` (IF-OR) are the logical operations that short-circuit. Kinda like how they do in Bash. The `&&` has higher precedence than the `||`.

 `x`'s boolean  | `x && y` evaluates to | is `y` evaluated  |
----------------|-----------------------|----------------------
 true           | `y`                   | yes
 false          | `x`                   | no

 `x`'s boolean  | <code>x &#124;&#124; y</code> evaluates to | is `y` evaluated  
----------------|-----------------------|----------------------
 true           | `x`                   | no
 false          | `y`                   | yes

For example, `x == 1 && 3` evaluates to `3` if `x` is `1` and otherwise evaluates to `0`. Notice that `x == 1 && 0 || -1` always evaluates to `-1` because `x == 1 && 0` evaluated to `0` no matter `x` equals `0` or not. So in idiom `cond && t || f` beware that `t` could be `0` or `null`. A safer way to phrase this is `cond && t; !cond || f` or you can use conditional function pair caller `cond ? { t }, { f }`.

### Assignment

The assignment operator `x = expr` requires the `x` at the left hand side to be an uninitialized variable identifier. It initialized the evaluated value of right hand side expression to the left hand side variable and the whole assignment expression evaluates to the right hand side value.

### Pair maker, left getter and right getter

The pair maker `,` is right-associative, so that it can be used to make a linked list. You can use `` `p `` and `~p` to get the left and right element of the pair `p`. For example, in `tests/inputs/hello_world.txt`:

```
hello_world = 'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '\n', null;
print = string => {
    string && ( output $ `string; print(~string) );
    null
};
print(hello_world)
```

The `` `string `` is used to get data and `~string` is to get next.

### Function maker and argument binder

The function maker `{}` turns the wrapped codes into a function (no argument by default).

The argument binder `x => {}` binds *one* argument identifier to a function. Like assignment operator, it also requires `x` to be an uninitialized variable identifier.

### Function caller

The function caller are `()` and `$`. The syntax is `func_name(expr)` and `func_name $ expr`. They assigns the evaluated result of the expression to the argument variable of the function (if any) and evaluates the function code. Note that `$` is right-associative, just like in Haskell, designed to apply multiple functions on a value without too much parenthese. The following codes are equivalent: `func3 $ func2 $ func1 $ val` and `func3(func2(func1(val)))`.

The syntax `func_name()` is valid and will be parsed as `func_name(null)`.

### Conditional function pair caller

The `?` operator is designed to do proper conditional expression evaluation. The syntax is `cond ? func_pair`. If `cond` is true, the `` `func_pair `` is called, otherwise, the `~func_pair` is called. The passed argument is always null.

### Expression connector

The expression connector is `;`. It connects two expressions into one and evaluates to the right hand side value.

## Expression

Everything is expression. The code intepreted by lreng should be one big expression itself and will be evaluated to one object eventually.

## Closure

Functions hold a reference of the frame where it is called. It effects the currying if the deeper function use the variables outside of it. For example, in `tests/inputs/closure.txt` we have this code:

```
foo = a => {
  b => {
    a + b + c
  }
};
bar = foo(1);

# this would cause repeated-init error if un-commented
#a = 2;

# you cannot access 'b' outside of the closure
#output $ b + '0';

# however, you can initialize a new identifier 'b' outside of the closure
#b = 3;
#output $ b + '0';

# there would be use-without-init error if the initialization of 'c' is removed
c = 3;

output $ bar(2) + '0';
output $ '\n'
```

## Built-in functions

- `output(i)`: Output a number `i` as a byte to the `stdout`. The acceptable value are integers in range `0` to `255` (inclusive). Any other value will cause runtime error. It always returns `null`.

- `input()`: Get a byte as number from the `stdin`. It return a number in range `1` to `255` (inclusive) if there are data to read in stdin, otherwise it blocks the program and wait for the input to come. You can execute the example program `tests/inputs/read_stdin.txt` with the command `echo '!@' | ./lreng.py tests/inputs/read_stdin.txt`. It would output:

```
a=!
b=@
a+b=a
```

