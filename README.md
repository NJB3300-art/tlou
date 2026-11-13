# Moonlight for Xbox (blade theme) - build and release

Builds the open-source [moonlight-xbox](https://github.com/TheElixZammuto/moonlight-xbox) client (GPL) for Xbox Dev Mode with a custom light/green theme, and publishes it as a GitHub Release.

This repo contains **no game files**. The app only streams from a Sunshine host (your PC or a cloud VM) that runs your own copy of a game.

## Get a build
1. Open the **Actions** tab, choose **Build and Release (Xbox UWP)**, click **Run workflow**, enter a tag such as `v1.0.0`.
2. When it finishes, download the zip from **Releases**.

## Install on Xbox (Dev Mode)
1. Enable Dev Mode on the console and open Device Portal.
2. Upload the `.msixbundle`, add the dependency `.appx` files from the release (VCLibs, UI.Xaml), install.
3. In the app, add your host's IP and pair using the PIN in Sunshine's web UI.

## Contents
- `.github/workflows/build-release.yml` - builds upstream source with the patch applied
- `patches/blade-theme.patch` - the theme changes (App.xaml, host and app pages)

Licensed consistent with upstream (GPL). Not affiliated with Microsoft, Sony or Naughty Dog.

---

# The Last of Us Remastered — UWP port (branch `tlou-uwp`)

A UWP (Xbox Developer Mode) version of [ravencaio/tlou-remastered-pc](https://github.com/ravencaio/tlou-remastered-pc):
its launcher and game start rebuilt as a packaged app, and its runtime built as a DLL that runs
inside it. See [`uwp-app/README.md`](uwp-app/README.md) for what changed, how to stage your own
game dump on a PC, how to install it, and the known limits (Vulkan on Xbox).

- `patches/tlou-uwp.patch` - the port's changes (runtime as `gow3-runtime.dll`, SDL3 stand-in, app-container memory calls, `probe_tlou.py --stage`)
- `uwp-app/` - the UWP app (C++/WinRT): launcher and host
- `.github/workflows/build-tlou-uwp.yml` - builds both and packages them; a `tlou-v*` tag publishes a release
