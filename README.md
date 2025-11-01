# ɭeŋ

Lreng is a simple interpreted programming language. The main idea is to build a minimal language that works on binary tree: it is parsed into a binary AST and has "Pair" as its only container data type. It has strong, dynamic typing and supports first-class citizen anonymous function and closure.


## Build & Usage

`gcc` and `make` are required to build the program, and optionally `emscripten` if you want to build the web playground.

Download this repo and run `make` to build the binary.

Run a Lreng code with:

```
./lreng [-d] {code_file_path}
```

The optional `-d` flag outputs debug information only when you build with `make debug`.


## Variable

Variable identifiers should match regex `[_A-Za-z][_A-Za-z0-9]*`.

All variables are immutable.

### Types

There are 4 types:
- Null: The unit type. Has key word `null`.
- Number: Numbers in lreng are all rational numbers 
  - You can represent numbers in decimal with point: `3.14159`
  - You can represent integers in binary and hexadecimal: `0b110011`, `0xc0de`
  - You can represent the code of printable and escapable ASCII characters. For example, `'A'` is `65`, `'\\'` is `92`, and `'\n'` is `10`
- Pair: Store the reference to two data, tagged `left` and `right`;
- Callable: A expression that can be called or jumped to.
  - Function: A callable that create a stack frame when called and can pass in at most one argument.
  - Macro: A callable that does not create a stack frame when called and cannot pass in argument.

### Global and scoped

Variable is global when it is not initialized in a function, otherwise, it is scoped.


## Expression

Everything is expression. The code intepreted by lreng should be one big expression itself and will be evaluated to one object eventually.


## Operators

### Arithmetics

The `+`, `-`, `*`, `/`, `%` (modulo), `^` (exponent) only accept numbers.

- The division is operated on rational numbers so there is *no* rounding happening.
- The modulo of `a % b` returns the rational number `r = a - b * floor(a / b)` (Floor division definition).
- The exponent only allow *integer* power, since otherwise the result will not be rational.

### Ceiling and floor

The ceiling `>>` and floor `<<` operators only accept number and do what they normally do. They are quite useful since the there is no rounding with division and modulo. For example: `<<(8 / 3)` is `2`.

### Assignment

The assignment operator `x = expr` requires the left hand side `x` to be an uninitialized variable identifier. It initializes left hand side variable to the evaluated value of right hand side expression and the whole assignment expression evaluates to the assigned value.

### Comparison

The comparison operators compare two objects by their values. The `>`, `<`, `>=`, `<=` can only compare numbers. The `==`, `!=` can compare all data types.

- For pair objects, `==` and `!=` compares their referenced objects recursively.
- For callable objects, `==` and `!=` compares their referenced code position, frame, and argument identifier.

The comparison operators return number `1` if the result is true, otherwise `0`.

### Logic

The logic operators `!` (NOT), `&` (AND) and `|` (OR) return `0` and `1` like comparison operators.

There is an implicit boolean function that maps any object into boolean number `0` and `1`.
- Number `0` stays `0`; non-zero numbers become `1`.
- Null becomes `0`.
- Pairs and callables become `1`.

These operator do *not* short-circuit.

### Short-circuit logic

The `&&` (CONDITION-AND) and  `||` (CONDITION-OR) are the logical operations that short-circuit. The `&&` has higher precedence than the `||`.

| `x`'s boolean  | `x && y` evaluates to | is `y` evaluated  |
|----------------|-----------------------|-------------------|
| 1              | `y`                   | yes               |
| 0              | `x`                   | no                |

| `x`'s boolean  | <code>x &#124;&#124; y</code> evaluates to | is `y` evaluated |
|----------------|-----------------------|-------------------|
| 1              | `x`                   | no                |
| 0              | `y`                   | yes               |

For example:
- `x == 1 && 3` evaluates to `3` if `x` is `1`, otherwise it evaluates to `0`.
- `x == 1 && 0 || -1` always evaluates to `-1` because `x == 1 && 0` evaluates to `0` regardless of `x`.

The idiom `cond && t || f` does not work exactly the same as `if cond then t else f` since `t` could be `0` or `null`. The equivalent of "if-then-else" is `cond && t; !cond || f` or using conditional pair caller and macros `cond ? [ t ], [ f ]`.

### Expression connector

The expression connector is `;`. It connects two expressions into one and evaluates to the right hand side value.

### Pair maker, left getter, and right getter

The pair maker `,` is right-associative, so that it can be used to make linked list conveniently. You can use `` `p `` and `~p` to get the left and right element of the pair `p`. For example, in `scripts/hello_world.txt`:

```
hello_world = 'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '\n', null;
print = string => {
    string && (output(`string); print(~string))
};
print(hello_world)
```

The `` `string `` accesses the data and `~string` accesses the next element.

### Function maker and argument binder

The function makers `{ ... }` turns the wrapped expression into a function. The created function has no argument by default.

The argument binder `x => func` binds *one* argument identifier to a function. A function can have at most one argument.

### Macro maker

Like the function, the macro maker `[ ... ]` turns the wrapped expression into a macro. Macro do not use argument.

Another different between macro and function is that function has it own stack and macro does not. Therefore, its body expression executes in the context of its caller.

### Callers

The callers are `()` and `$`. The syntax is `foo(expr)` and `foo $ expr`. They assigns the evaluated result of the expression to the argument variable of the callable (if any) and evaluates the callable. If the callable `foo` is macro, it simply ignores the argument.

The `$` is right-associative, just like in Haskell, designed to apply multiple callables on a value without too much parenthese. The expressions: `f3 $ f2 $ f1 $ val` and `f3(f2(f1(val)))` are equivalent.

The syntax `foo()` is valid and will be parsed as `foo(null)`.

### Conditional pair caller

The `?` operator is designed to do proper conditional expression evaluation. The syntax is `cond ? callable_pair`. If `cond` is true, the `` `callable_pair `` is called, otherwise, the `~callable_pair` is called. The passed argument is always null.

### Map, filter, and reduce

The map `f $> x`, filter `f $| x`, and reduce `f $/ x` operators apply callable recursively to a pair. 

**Map**

The map operation is defined as:
```
map(f, x) =
    Pair(map(f, left(x)), map(f, right(x))) , if x is a Pair
    f(x)                                    , otherwise
```

**Filter**

The filter operation is defined as:
```
filter(f, x) =
    x                     , if x is not a Pair and boolean of f(x) == True
    empty                 , if x is not a Pair and boolean of f(x) == False
    merge(                , if x is a Pair
      filter(f, left(x)),
      filter(f, right(x))
    )
        
```
where the merge function is:
```
merge(x, y) =
    empty      , if x == empty and y == empty
    x          , if x != empty and y == empty
    y          , if x == empty and y != empty
    Pair(x, y) , otherwise
```
If the final result is `empty`, it becomes `null`.

**Reduce**

The reduce operation is defined as:
```
reduce(f, x) =
    x                                          , if x is not a Pair
    f(reduce(f, left(x)), reduce(f, right(x))) , otherwise
```


## Built-in functions

- Input function `input()` gets a byte from the `stdin` as a number. It returns a number in the range `1` to `255` (inclusive) if there is data to read in stdin, otherwise it blocks the program and wait for the input to come. You can execute the example program with the command `echo '!@' | ./lreng scripts/read_stdin.txt`. It would output

    ```
    a=!
    b=@
    a+b=a
    ```

- Output function `output(i)` writes a number `i` as one byte to the `stdout`. The acceptable value are integers in range `0` to `255` (inclusive). Any other value will cause runtime error. It always returns `null`.

- Error function `error(i)` are the same as output function, except it writes to the `stderr`. (Not implemented yet.)

- Type checker functions: `is_number`, `is_callable`, and `is_pair`. They return number `1` or `0` when true or false. Null type has only `null` so just use `x == nul`.


## Closure

Codes in a function can access the identifiers initialized or accessable in the scope where the function is defined at the time it is called.

It enables the currying since the deeper function can use the identifiers outside of it. For example, in `scripts/closure.txt` we have this code:

```
foo = a => {
  # initializing 'a' here is not allowed as the argument identifier is implicitly initialized as you enter the function
  #a = 3;
  b => {
    # initializing 'a' here is allowed since it is not in the same function
    #a = 3
    a + b + c
  }
};
bar = foo(1);
bar2 = foo(2);

# you can initialize the same name identifier as a function's argument as long as it is not initialized in the same scope
#a = 3;

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
