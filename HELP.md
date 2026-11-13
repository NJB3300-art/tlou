# Help!

**Help page with the video guide (narrated, with captions):**
https://njb3300-art.github.io/tlou/help/

The same page in this repository: [`docs/help/index.html`](docs/help/index.html) ·
video: [`docs/help/help.mp4`](docs/help/help.mp4) · captions: [`docs/help/help.vtt`](docs/help/help.vtt)

## Quick steps

1. **Download** the newest [release](https://github.com/NJB3300-art/tlou/releases): `tlou-uwp-xbox-….zip` and `tlou-uwp-stage-kit-….zip`.
2. **On the PC:** unzip the stage kit, put `CUSA00552` and `CUSA00552-patch` next to it, double-click `Stage-TLOU-Xbox.cmd`.
3. **USB drive:** `E:\TLOU\CUSA00552`, `E:\TLOU\CUSA00552-patch`, and `uwp-stage` copied as `E:\TLOU\stage`.
4. **Install** the `.msix` (with `Microsoft.VCLibs.x64.14.00.appx`) from Xbox Device Portal → My games & apps → Add.
5. **Dev Home:** set the app type to **Game**, then start the app and choose **Play**.

Something wrong? Read `app.log` and `boot.log` in the app's LocalState folder (Device Portal → File explorer → LocalAppData).
More detail: [`uwp-app/README.md`](uwp-app/README.md).
