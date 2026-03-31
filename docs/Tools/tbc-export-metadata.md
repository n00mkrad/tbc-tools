## tbc-export-metadata
This application reads ld-decode metadata (`.db` or `.json`) and exports information in standard formats that other tools can read. It can also write decode-export JSON (`.export.json`) directly from SQLite metadata. At present, it can export:

- Decode export metadata JSON (`--export-json` / `--output-json`)
- Per-frame signal quality information from the VITS test signals, as CSV
- Per-frame LaserDisc VBI control signals, as CSV
- LaserDisc navigation information, as Audacity labels
- LaserDisc navigation information, as FFMETADATA1 (which FFmpeg can use to add chapter navigation to a video file, including VITC `timecode` when available)
- FFmpeg readvitc filter style VITC text, as `lavfi.readvitc.*` key lines for every frame
- Closed Captions, as SCC format (which tools like [ttconv](https://github.com/sandflow/ttconv) can read)

Syntax:

tbc-export-metadata \<options> \<input>

```
Options:
  -h, --help                Displays help on commandline options.
  --help-all                Displays help including Qt specific options.
  -v, --version             Displays version information.
  -d, --debug               Show debug
  -q, --quiet               Suppress info and warning messages
  -g, --gui                 Launch metadata export GUI
  --input <file>            Specify input metadata file
  --input-sqlite <file>     Alias for --input
  --export-json             Write decode export metadata JSON (<input>.export.json by default)
  --output-json <file>      Specify decode export metadata JSON output file (implies --export-json)
  --vits-csv <file>         Write VITS information as CSV
  --vbi-csv <file>          Write VBI information as CSV
  --audacity-labels <file>  Write navigation information as Audacity labels
  --ffmetadata <file>       Write navigation information as FFMETADATA1 (includes VITC timecode when available)
  --ffmpeg-vitc <file>      Write FFmpeg readvitc filter style VITC text (lavfi.readvitc.*)
  --closed-captions <file>  Write closed captions as Scenarist SCC V1.0 format

Arguments:
  input                     Specify input metadata file
```
