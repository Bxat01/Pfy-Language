# Pfy tutorial

This short tutorial introduces Pfy 1.0. Save the examples as UTF-8 files with
the `.pfy` extension and include the version header at the top:

```pfy
pfy 1.0;
```

Run a source file with `pfy run file.pfy`. Run `pfy check file.pfy` to parse
and validate it without executing the program.

## Values and bindings

Pfy values include `null`, booleans, numbers, strings, arrays, objects, and
functions. Use `let` to create a binding and `set` to update one:

```pfy
pfy 1.0;

let player = {
    name: "Mira",
    level: 3,
    tags: ["scout", "healer"],
};

set player.level = player.level + 1;
print(player.name, player.level);
print(player.tags[0]);
```

Arrays are zero-indexed. An object key can be accessed with dot notation when
it is an identifier, or with brackets and a string:

```pfy
print(player.name);
print(player["level"]);
```

## Conditions and loops

Conditions use parentheses and blocks use braces. `for` can iterate arrays,
object keys, and strings:

```pfy
pfy 1.0;

let scores = [8, 12, 5];
let total = 0;

for (score in scores) {
    if (score >= 10) {
        print("high score:", score);
    }
    set total = total + score;
}

print("total:", total);
```

`while` loops are also available. Use `break` to exit a loop and `continue` to
skip to its next iteration. Loops are bounded by the runtime step limit.

## Functions

Declare a function with `fn` and return a value with `return`:

```pfy
pfy 1.0;

fn greet(name) {
    return "Hello, " + name + "!";
}

print(greet("Pfy"));
```

Functions can call themselves recursively, subject to the runtime's call-depth
limit. A function without a `return` produces `null`.

## Configuration documents

Pfy can be used as a data format without executing script statements. At the
top level, `name = value;` creates a document field:

```pfy
pfy 1.0;

application = "Example";
settings = {
    language: "en",
    showConsole: true,
    shortcuts: {
        search.focus: "Alt+KeyF",
    },
};
```

Inspect the document as JSON with:

```sh
pfy run config.pfy --json
```

Reading a document with the CLI runs top-level program statements if present.
Applications that import untrusted settings should use a data-only parser and
validate their schema rather than executing arbitrary source.

## Useful built-ins

```pfy
pfy 1.0;

let values = range(1, 4);
push(values, 4);
print("length:", len(values));
print("type:", type(values));
print("JSON:", json(values));
```

The built-ins include `print`, `len`, `type`, `str`, `num`, `keys`, `has`,
`push`, `range`, `json`, and `error`. See the
[syntax reference](../specification/syntax.md) for exact behavior, operators,
scoping rules, and limits.

## Compile a binary image

`.pfyb` is Pfy's checked binary image format:

```sh
pfy build program.pfy -o program.pfyb
pfy check program.pfyb
pfy run program.pfyb
```

The CLI verifies the image header, version, checksums, and resource bounds
before running it.
