# Pfy documentation

Pfy is a small data and scripting language. This documentation explains how to
install the reference CLI, write and run `.pfy` files, and add editor support.

## Start here

1. [Getting started](getting-started.md) — build or install the CLI and run a
   Pfy file.
2. [Tutorial](tutorial.md) — learn the language with a complete example.
3. [Editor support](editor-support.md) — install syntax highlighting and learn
   what it takes to support more editors.
4. [Publishing](publishing.md) — release the project, publish its VS Code
   extension, and request language detection from code hosts.

## Reference

- [Syntax and runtime reference](../specification/syntax.md)
- [EBNF grammar](../specification/grammar.ebnf)
- [Binary format](../specification/binary-format.md)
- [Playfy backup profile](../specification/backup-profile.md)
- [Executable examples](../examples/README.md)

## What is available today?

- The C++17 reference parser, runtime, and `pfy` command-line interface.
- CMake build configuration and CTest smoke tests.
- A VS Code extension source package with TextMate syntax highlighting,
  snippets, language configuration, and optional file icons.
- A `v1.0.0` GitHub Release containing a Windows x64 CLI archive and a VS Code
  `.vsix` package.

The repository is currently private. GitHub Release downloads and the source
are therefore only accessible to people with repository access. The VS Code
extension has not yet been published to the Visual Studio Marketplace, and
the GHCR container workflow still needs a successful GitHub Actions run before
the image can be advertised as available.
