# ɭeŋ

A simple minimal interpreted functional programming language.

The main idea is to build a minimal functional language that works on binary tree: it is parsed into a binary AST and has "Pair" as its only container data type.

## Usage

Download this repo and run `make main` to build the program and then:

```
./lreng {code_file_path} [-d]
```

The optional `-d` flag outputs debug information only when you build the program with `make debug`.

## Variable

Variable identifiers should match regex `[_A-Za-z][_A-Za-z0-9]+`.

All variables are immutable.

### Types

There are 4 types:
- Null: The unit type. Has key word `null`.
- Number: Numbers in lreng are all rational numbers 
  - You can represent numbers in decimal with point: `3.14159`
  - You can represent integers in binary and heximal: `0b110011`, `0xc0de`
  - You can represent the code of printable and escapable ASCII characters. For example, `'A'` evaluates to `65`, `'\\'` is `92`, and `'\n'` is `10`
- Pair: Store the reference to two data, tagged `left` and `right`;
- Callable: A expression that can be called or jumped to.
  - Function: A callable that can pass in at most one argument.
  - Macro: A callable that does not have its own stack and cannot pass in argument.

### Global and Scoped

Variable is global when it is not initialized in a callable. Scoped variables are initialized in a callable.


## Operators

### Arithmetics

The `+`, `-`, `*`, `/`, `%` (modulo), `^` (exponent) only accept numbers.

- The division is operated on rational number so there is *no* rounding happening.
- The modulo of `a` and `b` returns the smallest rational number `r` such that `a / (b - r)` is a integer.
- The exponent only allow *integer* power, since otherwise the result will not be rational.

### Assignment

The assignment operator `x = expr` requires the left hand side `x` to be an uninitialized variable identifier. It initializes left hand side variable to the evaluated value of right hand side expression and the whole assignment expression evaluates to the assigned value.

### Comparison

The comparison operators compare two objects by their values. The `>`, `<`, `>=`, `<=` can only compare numbers. The `==`, `!=` can compare all data types.

For pair objects, `==` and `!=` compares their referenced objects recursively. 

The comparison operators return number `1` if the result is true, otherwise `0`.

### Logic

The logic operators `!` (NOT), `&` (AND) and `|` (OR) return `0` and `1` like comparison operators.

They will implicitly cast its operands into boolean number `0` and `1`.
- Number becomes `1` if it is not 0.
- Null becomes `0`.
- Pair and function become `1`.

These operator do *not* short-circuit.

### Short-circuit logic

The `&&` (CONDITION-AND) and  `||` (CONDITION-OR) are the logical operations that short-circuit. The `&&` has higher precedence than the `||`.

 `x`'s boolean  | `x && y` evaluates to | is `y` evaluated  |
----------------|-----------------------|----------------------
 1              | `y`                   | yes
 0              | `x`                   | no

 `x`'s boolean  | <code>x &#124;&#124; y</code> evaluates to | is `y` evaluated  
----------------|-----------------------|----------------------
 1              | `x`                   | no
 0              | `y`                   | yes

For example, `x == 1 && 3` evaluates to `3` if `x` is `1` and otherwise evaluates to `0`. Notice that `x == 1 && 0 || -1` always evaluates to `-1` because `x == 1 && 0` evaluated to `0` no matter `x` equals `0` or not. So the idiom `cond && t || f` does not work extactly the same as `if cond then t else f` since `t` could be `0` or `null`. The equivalent of "if-then-else" is `cond && t; !cond || f` or using conditional pair caller and macros `cond ? [ t ], [ f ]`.

### Pair maker, left getter and right getter

The pair maker `,` is right-associative, so that it can be used to make linked list conveniently. You can use `` `p `` and `~p` to get the left and right element of the pair `p`. For example, in `scripts/hello_world.txt`:

```
hello_world = 'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '\n', null;
print = string => {
    string && (output(`string); print(~string))
};
print(hello_world)
```

The `` `string `` is used to get data and `~string` is to get next.

### Function maker and argument binder

The function makers `{ ... }` and macro maker turns the wrapped expression into a function. The created function has no argument by default. Empty function is not allowed.

The argument binder `x => func` binds *one* argument identifier to a function. A function can have at most one argument.

### Macro maker

Like the function, the macro maker `[ ... ]` turns the wrapped expression into a macro. Macro do not use argument and empty macro is also not allowed.  

### Callers

The callers are `()` and `$`. The syntax is `foo(expr)` and `foo $ expr`. They assigns the evaluated result of the expression to the argument variable of the callable (if any) and evaluates the callable. If the callable `foo` is macro, it simply ignores the argument.

The `$` is right-associative, just like in Haskell, designed to apply multiple callables on a value without too much parenthese. The following codes are equivalent: `f3 $ f2 $ f1 $ val` and `f3(f2(f1(val)))`.

The syntax `foo()` is valid and will be parsed as `foo(null)`.

### Conditional pair caller

The `?` operator is designed to do proper conditional expression evaluation. The syntax is `cond ? callable_pair`. If `cond` is true, the `` `callable_pair `` is called, otherwise, the `~callable_pair` is called. The passed argument is always null.

### Expression connector

The expression connector is `;`. It connects two expressions into one and evaluates to the right hand side value.

## Expression

Everything is expression. The code intepreted by lreng should be one big expression itself and will be evaluated to one object eventually.

## Closure

Codes in a function can access the identifiers initialized or accessable in the scope where the function is defined at the time it is called.

It enables the currying since the deeper function can use the identifiers outside of it. For example, in `scripts/closure.txt` we have this code:

```
foo = a => {
  # initializing 'a' here is not allowed as the argument identifier is implicitly initialized as you enter the function
  #a = 3
  b => {
    # initializing 'a' here is allowed since it is not in the same function
    #a = 3
    a + b + c
  }
};
bar = foo(1);
bar2 = foo(2);

# you can initialize identifier with the same name as a function's argument as long as they would be initialized in the same scope
a = 2;

# you cannot access 'b' outside of the closure
#output $ b + '0';

# you can initialize a new identifier 'b' outside of the closure
#b = 3;
#output $ b + '0';

# there would be use-uninit error if the initialization of 'c' is removed
c = 3;

output $ bar(2) + '0'; # 6
output $ '\n';
output $ bar2(2) + '0'; # 7
output $ '\n'
```

## Built-in functions

- `output(i)`: Output a number `i` as one byte to the `stdout`. The acceptable value are integers in range `0` to `255` (inclusive). Any other value will cause runtime error. It always returns `null`.

- `input()`: Get a byte as number from the `stdin`. It return a number in range `1` to `255` (inclusive) if there are data to read in stdin, otherwise it blocks the program and wait for the input to come. You can execute the example program with the command `echo '!@' | ./lreng scripts/read_stdin.txt`. It would output:

```
a=!
b=@
a+b=a
```

