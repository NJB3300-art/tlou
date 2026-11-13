# The Last of Us Remastered — PC port, made into a UWP app (Xbox Developer Mode)

A copy of [ravencaio/tlou-remastered-pc](https://github.com/ravencaio/tlou-remastered-pc) (GPL-2.0),
the native PC port of the PS4 *The Last of Us Remastered* (CUSA00552), with its Windows pieces
(launcher, game start, runtime) rebuilt to run as a UWP app on an Xbox in Developer Mode.

**No game files are in this repository.** You need your own dump of the game and its update 1.11.
Not affiliated with Sony Interactive Entertainment, Naughty Dog or Microsoft.

- **UWP / Xbox:** [`uwp-app/README.md`](uwp-app/README.md): what changed, staging your dump on a
  PC (`Stage-TLOU-Xbox.cmd`), installing with Device Portal, known limits.
- **The original PC port** (still builds and runs as before): [`README-PC.md`](README-PC.md).

## Get a build

Every push to `main` runs **Build TLOU UWP (Xbox Dev Mode)** under Actions. Its artifacts are
`tlou-uwp-package` (the app package, its VCLibs dependency and the signing certificate) and
`stage-kit` (the PC staging tools). To publish a Release, run the workflow from the Actions tab
with a tag such as `tlou-v0.1.0`, or push such a tag.

## Layout

- `gow3_pc/`, `tools/`, `launcher/` - the port, as upstream, plus the UWP build (`GOW3_UWP`):
  `gow3_pc/uwp/` (the runtime as `gow3-runtime.dll`, an SDL3 stand-in, app-container memory calls)
- `uwp-app/` - the UWP app (C++/WinRT): launcher and host of the runtime
- `Stage-TLOU-Xbox.cmd` - prepares your dump on a PC for the console (`tools/probe_tlou.py --stage`)
- `.github/workflows/build-tlou-uwp.yml` - builds the runtime (MSYS2 CLANG64) and the app (MSVC)
