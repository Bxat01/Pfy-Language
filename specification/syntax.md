# Pfy syntax

This document is the language reference for Pfy 1.0. It describes the reference parser and runtime in this repository. The EBNF grammar is in [grammar.ebnf](grammar.ebnf).

## Source files

- Source is UTF-8. A leading UTF-8 byte-order mark is ignored.
- Files use the `.pfy` extension. Compiled images use `.pfyb`.
- Line endings may be LF, CRLF, or CR.
- Every source file starts with the language header:

```pfy
pfy 1.0;
```

The semicolon is optional. Any other version is rejected. The header is intentionally small: the format and runtime version live in the binary image and command line, not in source files.

## Comments

```pfy
# A line comment
// Another line comment

/*
    A block comment.
*/
```

Block comments do not nest. A `#`, `//`, or `/*` inside a string is ordinary text.

## Identifiers and keys

Identifiers start with an ASCII letter or `_` and continue with ASCII letters, digits, or `_`.

```text
name    _count    item2    HTTPStatus
```

Object keys and document fields can be written as identifiers or strings. A dotted key such as `console.toggle` is allowed **in key position only**:

```pfy
shortcuts = {
    console.toggle: "Alt+Backquote",
    "nav.home": "Alt+Digit1",
};
```

In an expression, a dot is member access: `console.toggle` reads the `toggle` field of `console`. Use a string key when a field name contains spaces or other punctuation.

## Values

| Type | Literal |
| --- | --- |
| Null | `null` |
| Boolean | `true`, `false` |
| Number | `42`, `-3.5`, `1.2e9`, `0xFF` |
| String | `"text"`, `"line\nbreak"` |
| Array | `[1, 2, 3]` |
| Object | `{ name: "Playfy", stable: true }` |
| Function | Named declaration `fn name(...) { ... }` |

Numbers are IEEE-754 doubles. Non-finite results cannot be produced by the arithmetic operators except through overflow; `json()` emits `null` for non-finite values.

Strings support these escapes:

```text
\"  \\  \/  \b  \f  \n  \r  \t  \uXXXX
```

A UTF-16 surrogate pair written as two `\uXXXX` escapes is combined. An unpaired surrogate becomes U+FFFD. Newlines inside strings are rejected.

Arrays and objects allow trailing commas and semicolons as separators:

```pfy
let values = [
    1,
    2,
];

let object = {
    first: 1,
    second: 2,
};
```

## Statements

### Header

```pfy
pfy 1.0;
```

### Declarations

```pfy
let count = 0;
let config = { language: "en" };
```

`let` creates a binding in the current scope. A declaration with the same name in the same scope replaces the previous value.

### Assignment

```pfy
set count = count + 1;
set config.language = "ar";
set values[0] = "first";
```

`set` updates the nearest existing binding, or creates a module binding if the name does not exist. Member and index assignment require an object or array.

A top-level `name = value;` statement is a document field. It creates or updates a module binding:

```pfy
pfy 1.0;

application = "Playfy";
settings = { accent: "#0062FF" };
```

Dotted document fields are allowed:

```pfy
shortcuts.console = "Alt+Backquote";
```

This creates a module field whose literal name is `shortcuts.console`. It is not member assignment; use `set shortcuts.console = ...;` after declaring `shortcuts` for that.

### Functions

```pfy
fn add(a, b) {
    return a + b;
}

print(add(2, 3));
```

Functions are values and can be stored and passed:

```pfy
let operation = add;
print(operation(4, 5));
```

Functions close over the environment in which they are declared. Missing arguments are bound to `null`. Extra arguments are ignored. Recursion is supported up to the runtime call-depth limit.

`return` outside a function is an error. A function that reaches the end of its body without `return` produces `null`.

### Conditionals

```pfy
if (ready) {
    print("ready");
} else if (pending) {
    print("pending");
} else {
    print("waiting");
}
```

Conditions use truthiness:

- `null`, `false`, `0`, `""`, `[]`, and `{}` are false.
- All other values are true.

### Loops

```pfy
while (i < 10) {
    i = i + 1;
}

for (item in values) {
    print(item);
}

for (key in object) {
    print(key);
}

for (character in "text") {
    print(character);
}
```

`for` copies the array, iterates object keys in lexicographic order, and iterates strings by Unicode code point. `break` exits the nearest loop; `continue` starts the next iteration.

`while` and `for` are bounded by the runtime step limit.

### Expression statements

A function call can be used as a statement:

```pfy
print("hello");
push(items, 4);
```

Any other expression is evaluated and discarded. There is no implicit output.

## Expressions

Precedence from lowest to highest:

| Level | Operators | Associativity |
| --- | --- | --- |
| 1 | `\|\|` | Left |
| 2 | `&&` | Left |
| 3 | `==`, `!=` | Left |
| 4 | `<`, `<=`, `>`, `>=` | Left |
| 5 | `+`, `-` | Left |
| 6 | `*`, `/`, `%` | Left |
| 7 | unary `!`, `-`, `+` | Right |
| 8 | call `()`, index `[]`, member `.` | Left |

`&&` and `||` short-circuit and produce booleans. `==` and `!=` compare deeply for arrays and objects. `<`, `<=`, `>`, and `>=` accept two numbers or two strings.

`+` adds numbers, concatenates strings, or concatenates arrays. If either operand is a string, the other is converted with the same rules as `str()`. Other arithmetic operators require numbers. Division or remainder by zero is an error.

Indexing:

```pfy
let first = values[0];
let name = object["name"];
let letter = "play"[1];
```

Member access supports object fields plus `length` on arrays and strings:

```pfy
print(items.length);
print("hello".length);
```

Missing fields, missing keys, and out-of-range indexes are errors rather than `null`.

## Built-in functions

| Function | Result |
| --- | --- |
| `print(...)` | Writes arguments separated by spaces, followed by a newline. Returns `null`. |
| `len(value)` | Length of a string (Unicode code points), array, or object. |
| `type(value)` | One of `null`, `bool`, `number`, `string`, `array`, `object`, `function`, `native`. |
| `str(value)` | Strings are returned unchanged; other values use compact JSON-like text. |
| `num(value)` | Converts a number, boolean, or numeric string. Invalid strings are errors. |
| `keys(object)` | Array of field names in lexicographic order. |
| `has(object, key)` | Whether an object contains a string key. |
| `push(array, value)` | Appends in place and returns the same array. |
| `range(end)`, `range(start, end)`, `range(start, end, step)` | Array of numbers. The end is exclusive. |
| `json(value)` | Pretty-printed JSON string. |
| `error(...)` | Raises a runtime error with the stringified arguments. |

## Module output

The CLI's `--json` flag prints the module object after execution. It contains top-level bindings created by `let`, `set`, function declarations, and document fields. Built-ins are excluded. Function values are rendered as `"<function name>"` so the output is always valid JSON.

```sh
pfy run settings.pfy --json
```

## Errors and limits

Source locations are reported as `path:line:column`. The reference runtime enforces:

- Maximum call depth: 128.
- Maximum execution steps: 5,000,000.
- Maximum array size in a single `range`: 1,000,000 values.
- Maximum string length in a binary image: 16 MiB.
- Maximum source size: 16 MiB in a binary image; 64 MiB when read from disk.
- Maximum compiled payload: 64 MiB.
- Maximum AST depth: 256.
- Maximum AST nodes: 1,000,000.

These limits are part of the reference implementation. A different implementation should document equivalent limits because `.pfy` is also used for untrusted configuration files.

## Security model

Pfy has no file, network, process, or environment access. `print` writes to standard output, and `error` raises an exception. The parser does not execute anything while reading a document.

Playfy's backup importer uses a data-only parser and does not run the C++ runtime on imported files. The runtime is intended for trusted scripts and command-line use.
