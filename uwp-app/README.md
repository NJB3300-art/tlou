# The Last of Us Remastered — UWP (Xbox Developer Mode)

A UWP version of [ravencaio/tlou-remastered-pc](https://github.com/ravencaio/tlou-remastered-pc)
(GPL-2.0), which runs the PS4 game's own x86-64 code with a runtime that supplies the PS4
libraries and a Vulkan renderer (shadPS4's video core). **No game files are included**: you need
your own dump of CUSA00552 and its update 1.11.

**Status: experimental, untested on a console.** The app, its launcher and the runtime DLL are
built by GitHub Actions; whether the game starts on an Xbox depends on the limits below.

## What changed for UWP

| PC version | UWP version |
| --- | --- |
| `TLOU Launcher.exe` / `tlou_launcher.pyw` | The app's own launcher (controller-driven): game folder, motion blur, align contact shadows, background shaders, draw pipeline, frame statistics, Vulkan driver |
| `TLOU Game.exe` / `run_game.py` | **Play** in the app (or Menu on the controller) |
| `gow3-probe.exe`, started by `probe_tlou.py` | `gow3-runtime.dll`, loaded and run inside the app (`gow3_uwp_run`) |
| Python preparation at every start | Done once on the PC: `Stage-TLOU-Xbox.cmd` (`probe_tlou.py --stage`) |
| SDL3 window, gamepad, keyboard, audio | The app's CoreWindow, Windows.Gaming.Input, CoreWindow keys and XAudio2, behind an SDL3 stand-in (`gow3_pc/uwp/sdl`) |
| `VirtualAlloc`, `MapViewOfFile3`, … | The `*FromApp` versions (the `codeGeneration` capability allows executable pages) |
| Restart from the in-game menu | Not possible in an app: close the app and start it again |

This repository is a copy of upstream commit `fdaaccaf` with these changes made in place (the new
code is in `gow3_pc/uwp/` and `uwp-app/`; the edits to existing files are marked `GOW3_UWP`).

## Install

1. Put the console in Developer Mode and open Xbox Device Portal.
2. From the release (or the `tlou-uwp-package` artifact of a workflow run, under Actions), install the `.msix` with the
   dependency `.appx` files (Microsoft.VCLibs x64).
3. In Dev Home, set the app's type to **Game** (games get far more memory and GPU than apps; the
   runtime needs about 6 GB).

## Prepare the game (on a PC, once)

1. Unzip `tlou-uwp-stage-kit.zip` (Python 3.10+ needed). Put your game folder `CUSA00552` and the
   update as `CUSA00552-patch` next to the kit's folder (as for the PC version), or pass
   `--game <folder>`.
2. Run `Stage-TLOU-Xbox.cmd`. It prepares the game exactly as the PC version does at each start
   and writes `uwp-stage\` (`boot.json` and the prepared files). The render resolution, aim assist,
   PS4 Pro shadows, job sleep and v0.2.2's contact shadows at half resolution are fixed here (they
   patch the game): set them with `TLOU_RESOLUTION=1920x1080`, `TLOU_AIM_ASSIST=0`,
   `TLOU_AMBIENT_SHADOWS=quarter`, … before running it.
3. Copy to an external drive (exFAT/NTFS), in a folder `TLOU`:
   ```
   E:\TLOU\CUSA00552\          the game
   E:\TLOU\CUSA00552-patch\    update 1.11
   E:\TLOU\stage\              the contents of uwp-stage
   ```
   The app also looks in its own `LocalState\TLOU` folder (upload with Device Portal's file
   explorer) and on every drive letter.

## Play

Start the app. The launcher shows which parts it found. **Play** starts the game. Saves,
settings (`tlou.ini`), the shader cache and `boot.log` are in the app's `LocalState` folder
(Device Portal → File explorer); after the game stops, the launcher shows the end of `boot.log`.

## Known limits

- **Vulkan on Xbox.** The renderer is Vulkan; the console has no Vulkan driver of its own. The
  package carries the Khronos loader (`vulkan-1.dll`) and the runtime also loads a driver DLL
  from the package directly (*Vulkan driver* → Mesa Dozen loads `vulkan_dzn.dll`, Mesa's Vulkan on
  Direct3D 12). Neither is built here yet: a Dozen build for UWP whose swap chain targets a
  CoreWindow has to be added to the package (put `vulkan_dzn.dll`, or `vulkan_icd.json` + the
  driver, in the runtime folder before packaging). Until then the game stops when it creates
  the Vulkan instance, with the reason in `boot.log`.
- **Write+execute memory.** If the console refuses pages that are writable and executable at
  once, the runtime falls back to execute-read and says so in `boot.log`.
- **DLLs the console may not have.** `runtime-imports.txt` in the release lists what each packaged
  DLL imports from the system. If the app reports error 126 when you press Play, one of those is
  missing on the console.
- No mouse, no touchpad; keyboard input works when a keyboard is connected.

## Build it yourself

The workflow `.github/workflows/build-tlou-uwp.yml` does all of it on `windows-2022` at every push to `main`:

1. `gow3-runtime.dll`: MSYS2 CLANG64 with the packages in the port's
   `gow3_pc/packaging/windows/README.md` (no SDL3), then
   `cmake -S gpu -B out/gpu -G Ninja -DGOW3_UWP=ON && ninja -C out/gpu gow3-runtime` in `gow3_pc`.
2. The app: `cmake -S uwp-app -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_NAME=WindowsStore
   -DCMAKE_SYSTEM_VERSION=10.0 -DGOW3_UWP_INCLUDE=<port>/gow3_pc/uwp -DGOW3_RUNTIME_DIR=<runtime DLLs>`,
   then `msbuild` with a certificate whose subject is `CN=TLOU-UWP`.

Not affiliated with Sony Interactive Entertainment, Naughty Dog or Microsoft.
