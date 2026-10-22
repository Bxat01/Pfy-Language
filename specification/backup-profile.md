# Playfy backup profile

Playfy stores a portable backup as a Pfy **source document** with the `.pfy` extension. The profile is intentionally a subset of the language: it contains literals, arrays, and objects, with top-level fields. It never contains functions or executable statements.

The application reads this profile with a dedicated TypeScript parser, not with the C++ interpreter. The C++ `pfy` command can open the same file for inspection:

```sh
pfy run backup.pfy --json
```

## Document header

```pfy
pfy 1.0;

application = "Playfy";
format = "playfy-backup";
version = "25.10.8";
exported = "2026-10-22T05:48:40.685Z";
```

| Field | Type | Required by importer | Meaning |
| --- | --- | --- | --- |
| `application` | string | yes | Must be `Playfy`. |
| `format` | string | yes | Must be `playfy-backup`. |
| `version` | string | no | Version of Playfy that produced the file. Informational. |
| `exported` | string | no | ISO-8601 export timestamp. Informational. |

Unknown top-level fields are ignored. They are preserved only if a future importer explicitly understands them; the current importer does not.

## `settings`

`settings` is required and is an object. Missing fields are filled from the application defaults.

```pfy
settings = {
    mode: "night",
    night: "dark",
    accent: "#0062FF",
    surface: "classic",
    glassBlur: 0,
    glassOpacity: 0.74,
    font: "\"Segoe UI Variable Text\", \"Segoe UI\", sans-serif",
    language: "en",
    showConsole: true,
    defaultInterval: 300,
    queueLoop: true,
    background: {
        enabled: true,
        id: "sgljdqw5hicrv484aygm",
        url: "https://res.cloudinary.com/ddqedxovk/image/upload/v1792535184/sgljdqw5hicrv484aygm.jpg",
        opacity: 0.83,
        blur: 0,
        dim: 0,
        shadow: 0.45,
    },
};
```

| Field | Allowed values |
| --- | --- |
| `mode` | `"day"`, `"night"` |
| `night` | `"onyx"`, `"dark"`, `"ash"` |
| `accent` | `#RRGGBB` or `#RGB` |
| `surface` | `"glass"`, `"classic"` |
| `glassBlur` | 0–32 |
| `glassOpacity` | 0.25–1 |
| `font` | CSS font-family string, at most 240 characters |
| `language` | `en`, `ru`, `hi`, `zh`, `ar` |
| `showConsole` | boolean |
| `defaultInterval` | 15–86400 seconds |
| `queueLoop` | boolean |
| `background.enabled` | boolean |
| `background.id` | string, at most 120 characters |
| `background.url` | `https://`, `http://`, or `data:image/` URL, at most 2048 characters; `null` is allowed |
| `background.opacity` | 0–1 |
| `background.blur` | 0–40 |
| `background.dim` | 0–0.9 |
| `background.shadow` | 0–1 |

Invalid values are replaced with the current/default value rather than partially applied.

## `shortcuts`

`shortcuts` is an object mapping action identifiers to key combinations. Only known action identifiers are imported; unknown identifiers are ignored.

```pfy
shortcuts = {
    console.toggle: "Alt+Backquote",
    search.focus: "Alt+KeyF",
    nav.home: "Alt+Digit1",
    nav.queue: "Alt+Digit2",
    nav.settings: "Alt+Digit3",
};
```

Keys may be identifiers or strings. The dotted identifiers shown above are the canonical form.

## `games`

`games` is an array of game records. It is the user's library, not the full catalogue.

```pfy
games = [
    {
        id: "357607133254254632",
        name: "Dead by Daylight",
        is_installed: true,
        executables: [
            {
                name: "deadbydaylight.exe",
                os: "win32",
                filename: "deadbydaylight.exe",
                is_launcher: true,
                is_installed: true,
            },
        ],
    },
];
```

A game requires string `id` and `name`. `executables` is an array; each executable requires a `name`, and may include `os`, `filename`, `path`, `segments`, `is_launcher`, and `is_installed`. Runtime-only fields such as PIDs and `is_running` are not exported.

The importer caps the library at 500 games and 80 executables per game. Unknown fields are ignored.

## `queue`

`queue` is optional. If absent, the importer starts with an empty queue.

```pfy
queue = {
    intervalSec: 300,
    loop: true,
    entries: [
        {
            uid: "TyhLIua2PnBITiTO",
            game: {
                id: "357607133254254632",
                name: "Dead by Daylight",
                executables: [ /* same shape as games[] */ ],
            },
        },
    ],
};
```

`intervalSec` is clamped to 15–86400. `entries` is capped at 500. A queue entry without a valid game is skipped. Queue game records use the same validation as library games.

## Import behaviour

1. The file is read by the native file dialog. The application rejects files larger than 4 MiB.
2. The Pfy source is tokenized and parsed. The parser accepts only the data subset: literals, arrays, objects, dotted keys, comments, and trailing separators.
3. `application` and `format` must match. `settings` must be an object.
4. Every field is validated and bounded. Unknown fields are discarded.
5. The user confirms the replacement.
6. Settings, shortcuts, library, and queue are written. The UI reloads.

A failed import leaves the current data untouched. The file is never executed, and no field is used as a script, expression, file include, or template.

## Bounds

| Item | Limit |
| --- | ---: |
| File size | 4 MiB |
| Games | 500 |
| Queue entries | 500 |
| Executables per game | 80 |
| URL length | 2048 characters |
| Font string | 240 characters |

The C++ parser has larger, separate limits for general source files. The app's importer intentionally uses the tighter data-only bounds above.

## Compatibility

The profile is versioned by the `pfy 1.0` header. A future application release may add optional fields; older importers ignore them. Removing or changing the meaning of a field requires a new header version and an explicit migration. Playfy does not silently reinterpret unknown data.

The `.pfy` extension is shared with executable Pfy source. The application distinguishes them by the `format` marker and never evaluates a backup as a program.
