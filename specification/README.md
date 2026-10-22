# Specification

The Pfy 1.0 specification is split into four documents:

1. [Syntax](syntax.md) — source encoding, values, statements, expressions, scoping, built-ins, and limits.
2. [Grammar](grammar.ebnf) — the concrete EBNF used by the reference parser.
3. [Binary format](binary-format.md) — the `PFYB` header, node encoding, checksums, and bounds.
4. [Playfy backup profile](backup-profile.md) — the data-only `.pfy` document used by Playfy.

## Conformance

An implementation is conforming for language version 1 if it:

- accepts the grammar in this directory,
- rejects unsupported header or image versions,
- implements the documented value and operator semantics,
- enforces resource limits that prevent unbounded parsing or execution,
- does not evaluate a document merely by reading it.

The C++ code in `parser/` and `runtime/` is the reference implementation. The VS Code grammar and snippets are editor tooling; they are not part of the execution contract.

## Versioning

Language version 1 covers both the source header (`pfy 1.0;`) and binary image version 1. Source-compatible additions that do not change existing programs may be made within version 1. Breaking changes, including changed node meanings or removed syntax, require a new version and a migration note.

The Playfy backup profile has its own compatibility rules in [backup-profile.md](backup-profile.md).
