# Help!

**Help page with the video guide (narrated, with captions):**
https://njb3300-art.github.io/tlou/help/

The same page in this repository: [`docs/help/index.html`](docs/help/index.html)

| Video | For | Files |
| --- | --- | --- |
| **2. Wi-Fi setup** | build v0.2.2-uwp.2 and newer | [`help-wifi.mp4`](docs/help/help-wifi.mp4) · captions [`help-wifi.vtt`](docs/help/help-wifi.vtt) |
| 1. USB setup | the first builds | [`help.mp4`](docs/help/help.mp4) · captions [`help.vtt`](docs/help/help.vtt) |

## Quick steps

1. **Download** the newest [release](https://github.com/NJB3300-art/tlou/releases): `tlou-uwp-xbox-….zip` and `tlou-uwp-stage-kit-….zip`.
2. **Install** the `.msix` (with `Microsoft.VCLibs.x64.14.00.appx`) from Xbox Device Portal → My games & apps → Add, and set its type to **Game** in Dev Home.
3. **On the Xbox:** start the app, turn on **Receive files over Wi-Fi**; note the address and code on the TV.
4. **On the PC:** unzip the kit, double-click **`Setup-TLOU-Xbox.cmd`**. It finds your PS4 dump (CUSA00552 + update 1.11), prepares it and sends it to the Xbox (or copies it to a USB drive).
5. **On the Xbox:** the launcher checks the files; choose **Play**.

Something wrong? Read `app.log` and `boot.log` in the app's LocalState folder (Device Portal → File explorer → LocalAppData).
More detail: [`uwp-app/README.md`](uwp-app/README.md).
