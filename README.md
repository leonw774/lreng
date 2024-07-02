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
  - You can represent integers in binary, heximal: `0b110011`, `0xc0de`
  - You can represent the code of printable and escapable ASCIIs. For example, `'A'` evaluates to `65`, `'\\'` is `92`, and `'\n'` is `10`
- Pair: store the references to two objects
- Function: store an executable code and an optional argument identifier
- Null: same as python None type

## Operators

### Arithmetics

The `+`, `-`, `*`, `\`, `%`, `^` only accept numbers and work just as you expect.

### Comparison

The comparison operators compare two objects by their values. The `>`, `<`, `>=`, `<=` can only compare numbers. The `==`, `!=` can compare all data types.

For pair objects, it compares their referenced objects recursively. They return number `1` if the comparison result is true, otherwise `0`.

### Logic

The logic operators `!` (NOT), `|` (OR) and `&` (AND) return `0` and `1` like comparison operators. All data types have their rule to evaluate to boolean. The boolean value of a number object is true if it is not 0, otherwise false. The null object is always false. Pair and function objects are always true.

They do *not* short-circuit. If you want something like an if-statement, use `?`.

### Assignment

assignment operator `x = expr` requires the `x` at the left hand side to be a single variable. It assigns the evaluated value of right hand side expression to the left hand side variable and evaluates the same right hand side value.

### Pair maker, left getter and right getter

The pair maker `,` is right-associative, so that it can be used to make a linked list. See example codes `fizzbuzz.txt` and `hello_world.txt`. Theoretically, you can also use pair objects to make a binary tree, and thus a set and a map.

Use `` `x `` and `~x` to get the left and right element of the pair.

### Function maker and argument setter

The function maker `{}` turns the wrapped codes into a function (no argument by default). You can use the argument setter `:` to bind *one* argument identifier to a function. Use currying or pair to pass more arguments.

### If operator

The "if" operator `?` is the logical-or operation that short-circuits. If the left hand side is false in boolean, it stops and evaluates to the left hand side, otherwise, the right hand side will then be evaluated and the whole expression evaluates to the same as the right hand side.

For example, `(1 == 2) ? x = 3` evaluates to `0` and the assignment of `3` to variable `x` does not happen because the right hand side is ignored. The expression `(1 == 1) ? x = 3` evaluates to `3` and the variable `x` equals to `3` after evaluation.

### Function caller

The function caller are `()` and `$`. The syntax is `func_name(expr)` and `func_name $ expr`. It assigns the evaluated result of the expression to the argument variable of the function (if any) and evaluates the function code. The syntax `func_name()` is valid and will be parsed as `func_name(null)`.

### Expression connector

The expression connector is `;`. It connects two expressions into one and evaluates to the later one.

### stdout writer

The `<<` operation outputs a number as a byte to the stdout. This operator always evaluates to null.

## Variables

Variable identifiers should match regex `[_A-Za-z][_A-Za-z0-9]+`. Variables can be used without being assigned first and are default to be null. For example, code `b = 2; a == b` is valid and evaluates to `0` because `null` does not equal the number `2`.

## Expression

All the codes, including those wrapped in function maker `{}`, should be a single expression and evaluated to one object eventually.   

## Closure

Functions hold a reference of the outer layer of frame when it is called. It effects the currying if the deeper function use the variables outside of it. For example, in `example/frames.txt` we have this code:

```
foo = a : {
  b : {
    a + b + c 
  }
};
c = 3;
bar = foo(1);
a = 8;
b = 5;
c = 2;
<< bar(2) + '0'
```

The function `bar = foo(1)` obtain the reference of the outer frames when `c` equals `3`. But when we call `bar` after setting `a` to `8`, `b` to `5`, and `c` to `2`, we get `5`. It is because the function finds variable value from bottom to up, which means that `bar` find variable `a`, set to `1` as the argument, in the `foo(1)` level, `b` in this own level, set to `2` as the argument, and `c` in the highest level where we modified the value from `3` to `2`.
