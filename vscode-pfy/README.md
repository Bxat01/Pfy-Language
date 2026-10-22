# Pfy for VS Code

Editor support for the [Pfy language](https://github.com/Bxat01/Pfy-Language), including Playfy configuration backups.

## Features

- Syntax highlighting for the `pfy 1.0;` header, keywords, strings, numbers, comments, operators, built-ins, and object keys.
- Bracket matching, auto-closing, comments, folding regions, and indentation rules.
- Snippets for functions, conditionals, loops, objects, and Playfy backup documents.
- Optional `Pfy Icons` file icon theme for `.pfy` and `.pfyb` files.

## Installation

### From source

```sh
cd vscode-pfy
npm install
npx @vscode/vsce package
code --install-extension pfy-language-1.0.1.vsix
```

### Development host

Open this directory in VS Code and press `F5`. Select **Run Extension** if prompted. A new Extension Development Host opens with the language registered.

### File icons

The extension contributes a file icon theme, but VS Code uses one icon theme at a time. Enable it with **Preferences: File Icon Theme → Pfy Icons**. It can be combined with a product icon theme independently.

Installing the extension does not automatically switch your current file icon theme. To show the Pfy icon next to `.pfy` files, select **Pfy Icons** once from the File Icon Theme picker. This changes the file icon theme for the VS Code profile.

The extension also detects the `pfy 1.0` header in files without a `.pfy` extension. Files ending in `.pfy` are identified as Pfy by their extension.

## Language

Pfy is a small, deterministic data and scripting language. A document starts with:

```pfy
pfy 1.0;
```

A configuration document can stay entirely declarative:

```pfy
application = "Playfy";
format = "playfy-backup";

settings = {
    accent: "#0062FF",
    language: "en",
};
```

An executable document can use functions, conditionals, and loops:

```pfy
fn greet(name) {
    return "Hello, " + name + "!";
}

for (channel in ["stable", "beta"]) {
    print(greet(channel));
}
```

See the [language specification](https://github.com/Bxat01/Pfy-Language/tree/main/specification) for the complete grammar and runtime.

## Extension settings

This extension has no runtime settings. The grammar is declarative and does not start a language server, so it works in restricted workspaces and remote sessions without extra configuration.

## Development

The extension is plain JSON, JSONC, and SVG plus one PNG marketplace icon. There is no build step for the language support itself.

```text
vscode-pfy/
├── package.json
├── language-configuration.json
├── syntaxes/pfy.tmLanguage.json
├── snippets/pfy.json
├── icons/
│   ├── pfy.svg
│   ├── pfyb.svg
│   └── pfy-icon-theme.json
└── icon.png
```

Package a release with:

```sh
npx @vscode/vsce package
```

## License

MIT. See the repository license.
