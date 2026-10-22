# Pfy binary image format (PFYB)

A `.pfyb` file is a compiled Pfy program. The reference compiler serializes the checked AST and keeps a copy of the source for diagnostics. The runtime verifies the complete image before executing any node.

All integers are little-endian.

## Header

The header is 24 bytes:

| Offset | Size | Field | Value |
| ---: | ---: | --- | --- |
| 0 | 4 | Magic | ASCII `PFYB` |
| 4 | 2 | Version | `1` for this specification |
| 6 | 2 | Flags | `0`; other values are rejected |
| 8 | 4 | Source length | Bytes, maximum 16 MiB |
| 12 | 4 | Source CRC-32 | CRC-32 of the source bytes |
| 16 | 4 | Payload length | Bytes, maximum 64 MiB |
| 20 | 4 | Payload CRC-32 | CRC-32 of the AST payload |
| 24 | *source length* | Source | UTF-8 source text |
| 24 + source length | *payload length* | AST payload | Encoded nodes |

The file length must be exactly `24 + sourceLength + payloadLength`. Trailing bytes are an error.

CRC-32 uses the standard reflected polynomial `0xEDB88320`, initial value `0xFFFFFFFF`, and final XOR `0xFFFFFFFF` (the same checksum used by ZIP and PNG).

## Node encoding

Each node starts with:

| Size | Field |
| ---: | --- |
| 1 | Node tag |
| 4 | Source line (`int32`) |
| 4 | Source column (`int32`) |

Then follows the tag-specific payload. Strings are encoded as a `uint32` byte length followed by UTF-8 bytes. Child counts are `uint32`; every count is checked against the global node limit before allocation.

## Node tags

| Tag | Name | Payload |
| ---: | --- | --- |
| 1 | Program | `uint32` child count, then that many statement nodes |
| 2 | Block | `uint32` child count, then statement nodes |
| 3 | Null | none |
| 4 | Bool | `uint8` (`0` false, non-zero true) |
| 5 | Number | IEEE-754 `float64` |
| 6 | String | string |
| 7 | Identifier | string |
| 8 | Array | `uint32` element count, then element nodes |
| 9 | Object | `uint32` field count, then for each: string key, value node |
| 10 | Unary | string operator, then one operand node |
| 11 | Binary | string operator, then left and right operand nodes |
| 12 | Call | `uint32` child count (callee first, then arguments), then child nodes |
| 13 | Index | object node, index node |
| 14 | Member | string field name, object node |
| 15 | Function | string name, `uint32` parameter count, parameter strings, body node |
| 16 | Let | string name, initializer node |
| 17 | Set | target node, value node |
| 18 | If | condition node, then node, else node (`Null` when absent) |
| 19 | While | condition node, body node |
| 20 | For | string loop variable, iterable node, body node |
| 21 | Return | value node (`Null` for a bare return) |
| 22 | Break | none |
| 23 | Continue | none |
| 24 | Expression | expression node |

Tags are stable. A future version may append new tags, but must increment the header version before doing so. This runtime rejects tags it does not understand rather than guessing at their meaning.

## Decoding order

A conforming loader must, in order:

1. Check the file length and magic.
2. Check the version and flags.
3. Check the source and payload lengths against limits.
4. Verify both CRC-32 values.
5. Decode one root node.
6. Verify that the root is a `Program`.
7. Verify that no payload bytes remain.

A loader must not allocate from an untrusted length before checking it against the remaining input and the configured limits.

## Limits

| Limit | Value |
| --- | ---: |
| Source length | 16 MiB |
| Payload length | 64 MiB |
| Single string | 16 MiB |
| AST depth | 256 |
| AST nodes | 1,000,000 |
| Parameters per function | 255 |
| Call arity | 1,000,000 (checked as part of node count) |

## Determinism

Compiling the same source with the same compiler version produces the same bytes. The image contains no timestamps, machine paths, or locale-dependent data. Object fields preserve source order in the AST, even though the runtime uses a sorted map for lookup.

## Compatibility with Playfy

Playfy settings backups are **source documents**, not `.pfyb` images. They are parsed directly and never compiled by the app. The binary format is for executable scripts and tooling; it is not the format of a settings backup.

See [backup-profile.md](backup-profile.md) for the backup document contract.
