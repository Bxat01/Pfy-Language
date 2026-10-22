# Examples

| File | Purpose |
| --- | --- |
| `main.pfy` | Executable tour: functions, objects, loops, arrays, and built-ins. |
| `config.pfy` | Declarative document in the Playfy backup shape. |

Run the executable example:

```sh
pfy run examples/main.pfy
```

Compile it:

```sh
pfy build examples/main.pfy -o examples/main.pfyb
pfy run examples/main.pfyb
```

Inspect the configuration as JSON:

```sh
pfy run examples/config.pfy --json
```

The generated `.pfyb` and JSON output files are build artifacts and are not required for the source examples.
