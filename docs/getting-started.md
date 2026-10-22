# Getting started

This guide builds the Pfy reference CLI, runs the included examples, and opens
`.pfy` files in an editor.

## Requirements

- CMake 3.20 or newer.
- A C++17 compiler:
  - Windows: Visual Studio 2022 or newer with the C++ workload.
  - macOS: Xcode Command Line Tools.
  - Linux: GCC or Clang and the platform's standard build tools.
- Git, if cloning the source repository.

## Get the source

The repository is currently private. Sign in to an account with access, then
clone it:

```sh
git clone https://github.com/Bxat01/Pfy-Language.git
cd Pfy-Language
```

If you have no repository access, download the `v1.0.0` source archive from
[GitHub Releases](https://github.com/Bxat01/Pfy-Language/releases). A private
repository's release downloads also require access.

## Build with CMake

These commands let CMake choose the native generator and compiler:

```sh
cmake -S . -B build
cmake --build build --config Release --parallel
ctest --test-dir build --build-config Release --output-on-failure
```

On a single-configuration generator, the executable is usually at
`build/pfy` (or `build/pfy.exe` on Windows). With a Visual Studio generator, it
is usually at `build/Release/pfy.exe`.

## Run Pfy

Run the executable-tour program:

```sh
pfy run examples/main.pfy
```

Inspect the declarative configuration as JSON:

```sh
pfy check examples/config.pfy
pfy run examples/config.pfy --json
```

Compile source into a checked `.pfyb` image and run it:

```sh
pfy build examples/main.pfy -o examples/main.pfyb
pfy check examples/main.pfyb
pfy run examples/main.pfyb
```

`pfy` must either be on `PATH` or invoked by its full path. The Windows
archive includes `pfy.exe`; extract it and add its folder to `PATH` if you want
to run `pfy` from any terminal.

## Install syntax highlighting in VS Code

1. Download `pfy-language-1.0.0.vsix` from the
   [Pfy 1.0.0 release](https://github.com/Bxat01/Pfy-Language/releases/tag/v1.0.0).
2. In VS Code, open **Extensions** → **…** → **Install from VSIX...** and choose
   the downloaded file.
3. Open a `.pfy` file. The language mode should show **Pfy** in the status
   bar.

Or install from a terminal with the VS Code command-line tool:

```sh
code --install-extension pfy-language-1.0.0.vsix
```

This VSIX is attached to a private GitHub Release. It is not yet listed in the
public Visual Studio Marketplace.

## Next

- Follow the [Pfy tutorial](tutorial.md).
- Consult the [language reference](../specification/syntax.md).
- Learn about [editors, colors, and language detection](editor-support.md).
