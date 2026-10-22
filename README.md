# Pfy

Pfy is a compact data and scripting language with a reference parser and runtime written in C++17. It is the settings format used by [Playfy](https://github.com/Bxat01/playfy), and it is also a small general-purpose language that can be run on its own.

The repository contains the language specification, a C++ parser, an interpreter, a typed binary image format, a VS Code extension, and examples that cover both executable programs and configuration documents.

## Status

- Language version: **1.0**
- Binary format: **1**
- Reference runtime: **C++17**
- Editor support: **VS Code 1.85+**
- License: **MIT**

## Quick start

Build with CMake:

```sh
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
```

On Linux or macOS:

```sh
cmake -S . -B build
cmake --build build -j
```

Run the tour:

```sh
./build/Release/pfy.exe run examples/main.pfy
```

Compile it to the binary format and run the image:

```sh
./build/Release/pfy.exe build examples/main.pfy -o examples/main.pfyb
./build/Release/pfy.exe run examples/main.pfyb
```

The same commands work on Unix with `./build/pfy`.

## Command line

| Command | Description |
| --- | --- |
| `pfy run <file> [--json]` | Parses or loads a program and executes it. `--json` prints the module object. |
| `pfy check <file>` | Parses a source file or verifies a `.pfyb` image without executing it. |
| `pfy build <file> [-o <file.pfyb>]` | Compiles source to a checked binary image. |
| `pfy version` | Prints the language and image version. |
| `pfy help` | Prints usage information. |

A `.pfyb` file is validated before execution: signature, format version, source checksum, payload checksum, node depth, string lengths, and node count are all checked.

## Language at a glance

```pfy
pfy 1.0;

fn greet(name) {
    return "Hello, " + name + "!";
}

let app = {
    name: "Playfy",
    channels: ["stable", "beta"],
};

print(greet("world"));

for (channel in app.channels) {
    print(channel);
}
```

Values are `null`, booleans, numbers, strings, arrays, objects, functions, and native functions. Statements include `let`, `set`, `fn`, `if`, `else`, `while`, `for`, `return`, `break`, and `continue`.

A document can also be completely declarative:

```pfy
pfy 1.0;

application = "Playfy";
settings = {
    accent: "#0062FF",
    language: "en",
};
```

The top-level keys become members of the module object. This is the shape used by Playfy backups, so the same parser can inspect an exported settings file:

```sh
pfy run settings.pfy --json
```

## Playfy integration

Playfy writes backups with a `.pfy` extension. The file starts with the standard header and stores the application name, format marker, version, appearance settings, shortcuts, game library, and queue as Pfy values.

The application's importer deliberately uses a **data-only parser**. It validates every field and never evaluates a script from a backup. The C++ runtime is the general language implementation; the two share the same lexical grammar and value model.

See [`specification/backup-profile.md`](specification/backup-profile.md) for the profile and the security rules.

## VS Code extension

The `vscode-pfy` directory is a standalone extension with:

- TextMate syntax highlighting for source and backup documents.
- Comment, bracket, folding, and indentation rules.
- Snippets for functions, loops, conditionals, and configuration documents.
- Optional file icons for `.pfy` and `.pfyb`.

To try it in a local VS Code installation:

```sh
cd vscode-pfy
npx @vscode/vsce package
code --install-extension pfy-language-1.0.0.vsix
```

## Repository layout

```text
Pfy-Language/
├── parser/          C++ lexer, parser, AST and binary codec
├── runtime/         C++ interpreter and CLI
├── specification/   Normative syntax, grammar and binary formats
├── vscode-pfy/      VS Code language extension
├── examples/        Executable and declarative examples
└── CMakeLists.txt
```

## Design constraints

- No implicit global state in the language runtime; each process gets one module environment.
- All loops and calls have hard limits so malformed or hostile input cannot run forever.
- Binary images are checksummed and bounded before any node is allocated.
- Parsing never opens files, evaluates code, or performs I/O.
- UTF-8 source is accepted with or without a BOM.

## Building the VS Code extension

The extension has no build step. It is plain JSON, JSONC, and SVG plus one PNG icon. Package it with `@vscode/vsce`, or open `vscode-pfy` as an extension development host.

## Documentation

- [Language syntax](specification/syntax.md)
- [EBNF grammar](specification/grammar.ebnf)
- [Binary format](specification/binary-format.md)
- [Playfy backup profile](specification/backup-profile.md)
- [Examples](examples/README.md)

## License

MIT. See [LICENSE](LICENSE).
