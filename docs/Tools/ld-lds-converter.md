## ld-lds-converter
This application accepts 10-bit packed `.lds` files and converts them to FLAC by default. It also supports uncompressed signed 16-bit (`.s16`) output and legacy RIFF/WAV output. A basic Qt6 GUI is available for drag/drop conversion.

Syntax:

ld-lds-converter \<options>

Options:
```
-h, --help                 Displays this help
-v, --version              Displays version information
-d, --debug                Show debug output
-g, --gui                  Launch GUI mode
-i, --input <file>         Specify input laserdisc sample file (default: stdin)
-o, --output <file>        Specify output file (default: inferred from input path)
-u, --unpack               Unpack 10-bit data (default mode)
-p, --pack                 Pack 16-bit data into 10-bit
--format <format>          Unpack output format: flac (default), s16, riff
--s16                      Shortcut for --format s16
--flac                     Shortcut for --format flac
-r, --riff                 Shortcut for --format riff (legacy mode)
--sample-rate <hz>         FLAC sample rate in Hz (default: 40000)
--compression-level <0-8>  FLAC compression level (default: 8)
```

Examples:
```
# GUI mode (also default with no explicit CLI mode)
ld-lds-converter

# Convert input.lds -> input.flac (default)
ld-lds-converter -i input.lds

# Convert input.lds -> input.s16
ld-lds-converter -i input.lds --format s16
```
