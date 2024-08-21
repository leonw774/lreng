# Lreŋ

A simple interpreted programming language written in native Python 3.10.

## Usage

```
python3 lreng.py {file_path}
```

or

```
python3 lreng.py -c "{some codes}"
```

## Data types

There are 4 types of objects:
- Number: rational number implemented by Python `Fraction` class
  - You can represent numbers in decimal with point: `3.14159`
  - You can represent integers in binary and heximal: `0b110011`, `0xc0de`
  - You can represent the code of printable and escapable ASCII characters. For example, `'A'` evaluates to `65`, `'\\'` is `92`, and `'\n'` is `10`
- Pair: store the references to two objects
- Function: store an executable code and an optional argument identifier
- Null: the unit type. Use key word `null`

## Variables

Variable identifiers should match regex `[_A-Za-z][_A-Za-z0-9]+`. All variables are immutable after initialization by the assignment operator.

## Operators

### Arithmetics

The `+`, `-`, `*`, `/`, `%` (modulo), `^` (exponent) only accept numbers and work just as you expect.

### Comparison

The comparison operators compare two objects by their values. The `>`, `<`, `>=`, `<=` can only compare numbers. The `==`, `!=` can compare all data types.

For pair objects, it compares their referenced objects recursively. They return number `1` if the comparison result is true, otherwise `0`.

### Logic

The logic operators `!` (NOT), `&` (AND) and `|` (OR) return `0` and `1` like comparison operators. The boolean value of a number object is true if it is not 0, otherwise false. The null object is always false. Pair and function objects are always true.

They do *not* short-circuit. If you want something like an if-statement, use conditional operators `&&` and `||`.

### Conditional

The conditional operator are `&&` (IF-AND) and  `||` (IF-OR) is the logical operations that short-circuit. Kinda like how they do in Bash. The reverse-if-and operator is right-associative and is created to make if-then-else expression. The `&&` has higher precedence than the `||`.

 `x`'s boolean  | `x && y` evaluates to | is `y` evaluated  |
----------------|-----------------------|----------------------
 true           | `y`                   | yes
 false          | `x`                   | no

 `x`'s boolean  | <code>x &#124;&#124; y</code> evaluates to | is `y` evaluated  
----------------|-----------------------|----------------------
 true           | `x`                   | no
 false          | `y`                   | yes

For example, `x == 1 && 3` evaluates to `3` if `x` is `1` and otherwise evaluates to `0`. Notice that `x == 1 && 0 || -1` always evaluates to `-1` because `x == 1 && 0` evaluated to `0` no matter `x` equals `0` or not. So in idiom `c && t || f` beware that `t` could be `0` or `null`. A safer way to phrase this is `c && t; !c || f`. 

### Assignment

assignment operator `x = expr` requires the `x` at the left hand side to be an uninitialized variable identifier. It initialized the evaluated value of right hand side expression to the left hand side variable and the whole assignment expression evaluates to the right hand side value.

### Pair maker, left getter and right getter

The pair maker `,` is right-associative, so that it can be used to make a linked list. You can use `` `p `` and `~p` to get the left and right element of the pair `p`. For example, in `examples/hello_world.txt`:

```
hello_world = 'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '\n', null;
print = string : {
    string != null && (
        << `string;
        print(~string)
    );
    null
};
print(hello_world)
```

The `` `string `` is used to get data and `~string` is to get next.

You can also use pair objects to make a binary tree.

### Function maker and argument setter

The function maker `{}` turns the wrapped codes into a function (no argument by default). You can use the argument setter `:` to bind *one* argument identifier to a function. Use currying or pair to pass more arguments.

### Function caller

The function caller are `()` and `$`. The syntax is `func_name(expr)` and `func_name $ expr`. They assigns the evaluated result of the expression to the argument variable of the function (if any) and evaluates the function code. Note that `$` is right-associative, just like in Haskell, designed to apply multiple functions on a value without too much parenthese. The following code are equivalent: `func3 $ func2 $ func1 $ val` and `func3(func2(func1(val)))`.

The syntax `func_name()` is valid and will be parsed as `func_name(null)`.

### Expression connector

The expression connector is `;`. It connects two expressions into one and evaluates to the right hand side value.

### `stdout` writer

The `<<` operation outputs a number as a byte to the stdout. This operator always evaluates to null.

## Expression

All the codes, including those wrapped in function maker `{}`, should be a single expression and evaluated to one object eventually.

## Closure

Functions hold a reference of the outer layer of frame when it is called. It effects the currying if the deeper function use the variables outside of it. For example, in `examples/frames.txt` we have this code:

```
foo = a : {
  b : {
    a + b + c
  }
};
bar = foo(1);
a = 2;
c = 3; # there would be an error if this line is removed
<< bar(2) + '0'
```

The function `bar = foo(1)` set its argument `a` to `1`. Setting `a` to `2` in the next line doesn't change the `a` in the function `bar` and `bar(2)` evaluates to `6` rather than `7`.
