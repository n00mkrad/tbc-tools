# ld-lds-converter
`ld-lds-converter` converts packed 10-bit `.lds` samples into:

- **FLAC** (default output mode, encoded with **libFLAC**)
- **s16 raw PCM** (`.s16`) as a secondary uncompressed option

It also keeps legacy modes:

- `--riff` / `--format riff` (16-bit RIFF/WAV wrapper output)
- `--pack` (pack 16-bit back to 10-bit)

## GUI mode
The tool now has a basic Qt6 dialog designed for quick conversion:

- launch with no explicit CLI mode (or with `--gui`)
- drag/drop `.lds` files into the dialog
- choose output format (`FLAC` default, or `s16 uncompressed`)
- click **Convert**

## CLI usage
```bash
ld-lds-converter [options]
```

### Main options
- `-g, --gui` Launch GUI mode
- `-i, --input <file>` Input `.lds` (or stdin if omitted)
- `-o, --output <file>` Output path (auto-derived from input if omitted)
- `-u, --unpack` Unpack 10-bit data (default mode)
- `-p, --pack` Pack 16-bit data into 10-bit

### Output format options (unpack mode)
- `--format <flac|s16|riff>` Select unpack output format (default: `flac`)
- `--flac` Shortcut for `--format flac`
- `--s16` Shortcut for `--format s16`
- `-r, --riff` Shortcut for `--format riff` (legacy)
- `--sample-rate <hz>` FLAC sample rate (default: `40000`)
- `--compression-level <0-8>` FLAC compression level (default: `5`)

## Examples
```bash
# Launch GUI
ld-lds-converter

# Convert input.lds -> input.flac (default)
ld-lds-converter -i input.lds

# Convert input.lds -> input.s16
ld-lds-converter -i input.lds --format s16

# Convert using explicit FLAC settings
ld-lds-converter -i input.lds -o output.flac --sample-rate 40000 --compression-level 8

# Legacy unpack with RIFF header
ld-lds-converter -i input.lds --format riff -o output.wav
```