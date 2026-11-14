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
| Copying the game by hand | **Setup-TLOU-Xbox.cmd** on the PC + **Receive files over Wi-Fi** in the app, or a USB drive |
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

## Prepare the game and send it to the console (one click)

You need your own PS4 dump: the base game CUSA00552 and its update 1.11 (any folder names). The
Steam game, *The Last of Us Part I*, is a different game and cannot be used.

1. Unzip `tlou-uwp-stage-kit-….zip` on a Windows PC with Python 3.10+.
2. On the Xbox, start the app and turn on **Receive files over Wi-Fi**: the TV shows an address
   (`http://192.168.…:8077`) and a six-digit code. (Or skip this and use a USB drive.)
3. On the PC, double-click **`Setup-TLOU-Xbox.cmd`**. It
   - finds the game and the update on your drives by their `param.sfo` (or ask it with
     `--game` / `--update`),
   - prepares the game (`probe_tlou.py --stage`): the render resolution, aim assist, PS4 Pro
     shadows, job sleep and half-resolution contact shadows are fixed here, as environment
     variables such as `TLOU_RESOLUTION=1920x1080` set before you run it,
   - sends everything to the Xbox over Wi-Fi (enter the address and code), or copies it to a USB
     drive as `<drive>:\TLOU\CUSA00552`, `CUSA00552-patch` and `stage`.

   Interrupted? Run it again: files already on the console are skipped, a half-sent file continues.

**Without Python on the PC:** the address shown on the TV is also a web page. Open it in a browser,
enter the code and pick the three folders (game, update, and a `uwp-stage` folder made on another PC
with `Stage-TLOU-Xbox.cmd`).

Files sent over Wi-Fi go to the app's own storage (`LocalState\TLOU`); the launcher looks there
and on every drive. The launcher's **file check** reads each `param.sfo` and the stage, and tells
you if the game, the update version or the stage do not match; Play waits until they do.

## Play

Start the app. The launcher shows which parts it found. **Play** starts the game. Saves,
settings (`tlou.ini`), the shader cache and `boot.log` are in the app's `LocalState` folder
(Device Portal → File explorer); after the game stops, the launcher shows the end of `boot.log`.

## Known limits

- **Rendering on Xbox (experimental).** The game draws with Vulkan, and the console has no Vulkan
  driver. From v0.2.2-uwp.3 the package carries one: Mesa's **Dozen** (`vulkan_dzn.dll`, Vulkan on
  Direct3D 12, Mesa 26.3.0), built by the workflow with
  [`uwp-app/mesa/mesa-uwp-corewindow.patch`](mesa/mesa-uwp-corewindow.patch): the swapchain is made
  for the app's CoreWindow (the app passes it as `MESA_WSI_COREWINDOW`), with no desktop window or
  DirectComposition, and packaged DLLs load with `LoadPackagedLibrary`. `dxil.dll` (DirectX Shader
  Compiler) validates and signs the shaders. *Vulkan driver* in the launcher: **Auto** uses Dozen;
  *Khronos loader* tries `vulkan-1.dll`. Untested on a console: Dozen implements Vulkan 1.1 plus
  extensions on top of Direct3D 12, and features the renderer needs may be missing there; `boot.log`
  says what failed. `dozen-imports.txt` in the release lists what the driver needs from the system.
- **Memory.** The PC port needs about 6 GB; Developer Mode games get about 5 GiB. Expect the game to
  run out of memory until its settings are reduced for the console.
- **Write+execute memory.** If the console refuses pages that are writable and executable at
  once, the runtime falls back to execute-read and says so in `boot.log`.
- **DLLs the console may not have.** `runtime-imports.txt` in the release lists what each packaged
  DLL imports from the system. If the app reports error 126 when you press Play, one of those is
  missing on the console.
- No mouse, no touchpad; keyboard input works when a keyboard is connected.

## If nothing happens

The app writes `app.log` (each start, each step, what failed) and the game writes `boot.log`,
both in its `LocalState` folder: Device Portal → File explorer → LocalAppData → the app → LocalState.
When the game stops with an error the app closes; the next start shows the end of `boot.log`.

## Build it yourself

The workflow `.github/workflows/build-tlou-uwp.yml` does all of it on `windows-2022` at every push to `main`:

1. `gow3-runtime.dll`: MSYS2 CLANG64 with the packages in the port's
   `gow3_pc/packaging/windows/README.md` (no SDL3), then
   `cmake -S gpu -B out/gpu -G Ninja -DGOW3_UWP=ON && ninja -C out/gpu gow3-runtime` in `gow3_pc`.
2. The app: `cmake -S uwp-app -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_NAME=WindowsStore
   -DCMAKE_SYSTEM_VERSION=10.0 -DGOW3_UWP_INCLUDE=<port>/gow3_pc/uwp -DGOW3_RUNTIME_DIR=<runtime DLLs>`,
   then `msbuild` with a certificate whose subject is `CN=TLOU-UWP`.

Not affiliated with Sony Interactive Entertainment, Naughty Dog or Microsoft.
