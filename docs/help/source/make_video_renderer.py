"""Setup video for the TLOU Remastered UWP port: stills drawn with PIL, flite narration, captions.

Each scene is a list of beats: one narrated sentence and the picture shown while it is spoken (the
pictures build up step by step). Output: help.mp4 (soft CC track), help.vtt, help.srt."""
import json
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

W, H, FPS = 1920, 1080, 30
OUT = Path(__file__).parent / "build3"
OUT.mkdir(exist_ok=True)
VOICE, SPEED, GAP = "af_heart", 1.0, 0.38

BG = (14, 18, 16)
PANEL = (24, 31, 27)
PANEL2 = (32, 41, 36)
TEXT = (235, 240, 230)
DIM = (140, 154, 142)
ACCENT = (84, 158, 97)
WARN = (219, 107, 82)

FONT_DIR = "/usr/share/fonts/truetype/dejavu/"
def font(size, bold=False, mono=False):
    name = "DejaVuSansMono.ttf" if mono else ("DejaVuSans-Bold.ttf" if bold else "DejaVuSans.ttf")
    return ImageFont.truetype(FONT_DIR + name, size)

F_TITLE, F_H, F_BODY, F_SMALL = font(84, True), font(56, True), font(40), font(30)
F_MONO, F_MONO_S = font(36, mono=True), font(28, mono=True)


def canvas(step, total, heading):
    im = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W, 8], fill=ACCENT)
    if heading:
        d.text((120, 90), heading, font=F_H, fill=TEXT)
        d.rectangle([120, 172, 300, 178], fill=ACCENT)
    if step:
        label = f"Step {step} of {total}"
        tw = d.textlength(label, font=F_SMALL)
        d.rounded_rectangle([W - 160 - tw, 92, W - 120, 142], 25, fill=PANEL2)
        d.text((W - 140 - tw, 99), label, font=F_SMALL, fill=DIM)
    d.text((120, H - 70), "The Last of Us Remastered \u2014 UWP port  \u00b7  renderer build (v0.2.2-uwp.3)", font=F_SMALL, fill=DIM)
    return im, d


def bullets(d, items, shown, x=120, y=250, gap=92, f=F_BODY):
    for i, item in enumerate(items[:shown]):
        d.ellipse([x, y + i * gap + 16, x + 20, y + i * gap + 36], fill=ACCENT)
        d.text((x + 48, y + i * gap), item, font=f, fill=TEXT)


def tree(d, lines, shown, x, y, highlight=None, f=F_MONO, gap=58):
    labels = [("    " * indent) + ("\u251c\u2500 " if indent else "") + name for indent, name, _ in lines]
    name_w = max(d.textlength(l, font=f) for l in labels)
    note_w = max(d.textlength(n, font=F_SMALL) for _, _, n in lines)
    d.rounded_rectangle([x - 40, y - 40, x + name_w + 60 + note_w + 40, y + gap * len(lines) + 20], 22, fill=PANEL)
    for i, label in enumerate(labels[:shown]):
        color = ACCENT if highlight is not None and i == highlight else TEXT
        d.text((x, y + i * gap), label, font=f, fill=color)
        note = lines[i][2]
        if note:
            d.text((x + name_w + 60, y + i * gap + (f.size - F_SMALL.size) / 2 + 2), note, font=F_SMALL, fill=DIM)
    return x + name_w + 60 + note_w + 40


def window(d, x, y, w, h, title):
    d.rounded_rectangle([x, y, x + w, y + h], 18, fill=PANEL)
    d.rounded_rectangle([x, y, x + w, y + 64], 18, fill=PANEL2)
    d.rectangle([x, y + 46, x + w, y + 64], fill=PANEL2)
    for i, c in enumerate([(200, 90, 80), (210, 170, 70), (90, 170, 100)]):
        d.ellipse([x + 26 + i * 34, y + 22, x + 46 + i * 34, y + 42], fill=c)
    d.text((x + 150, y + 14), title, font=F_SMALL, fill=DIM)


def button(d, x, y, label, on=False, f=F_SMALL):
    tw = d.textlength(label, font=f)
    d.rounded_rectangle([x, y, x + tw + 60, y + 62], 14, fill=ACCENT if on else PANEL2)
    d.text((x + 30, y + 13), label, font=f, fill=TEXT)
    return x + tw + 60


def pad_button(d, x, y, letter, color):
    d.ellipse([x, y, x + 64, y + 64], fill=color)
    tw = d.textlength(letter, font=F_BODY)
    d.text((x + 32 - tw / 2, y + 8), letter, font=F_BODY, fill=(255, 255, 255))


# Scenes of the third help video: the renderer build (v0.2.2-uwp.3, Mesa Dozen).
# Each beat: (caption, spoken text or None for "same as caption", picture).

TOTAL = 5
SCENES = []


def scene(name, beats):
    SCENES.append((name, beats))


def card_title(sub):
    im, d = canvas(0, TOTAL, None)
    d.text((120, 300), "The Last of Us Remastered", font=F_TITLE, fill=TEXT)
    d.text((120, 410), "The renderer build", font=font(64), fill=ACCENT)
    d.rectangle([120, 515, 420, 523], fill=ACCENT)
    if sub:
        d.text((120, 570), "v0.2.2-uwp.3  ·  a Vulkan driver for the Xbox", font=F_BODY, fill=DIM)
        d.text((120, 640), "Help video 3 — videos 1 and 2 cover the USB and Wi-Fi setup", font=F_SMALL, fill=DIM)
    return im


scene("title", [
    ("Hey again! This is help video number three, for the newest build of The Last of Us Remastered on Xbox.",
     None, card_title(0)),
    ("This one is all about the big new piece: a way for the Xbox to actually draw the game.", None, card_title(1)),
])


def card_stack(n):
    im, d = canvas(0, TOTAL, "What's new: a Vulkan driver in the package")
    layers = [("The game's renderer", "speaks Vulkan", PANEL2, TEXT),
              ("Mesa Dozen  (vulkan_dzn.dll)", "turns Vulkan into Direct3D 12", ACCENT, (255, 255, 255)),
              ("Direct3D 12", "built into the Xbox", PANEL2, TEXT),
              ("Your TV", "through the app's window", PANEL, DIM)]
    for i, (head, body, fill, color) in enumerate(layers[:n]):
        y = 240 + i * 165
        d.rounded_rectangle([420, y, 1500, y + 130], 18, fill=fill)
        d.text((470, y + 20), head, font=F_H, fill=color)
        d.text((470, y + 86), body, font=F_SMALL, fill=color if fill == ACCENT else DIM)
        if i:
            d.polygon([(950, y - 30), (970, y - 30), (960, y - 10)], fill=DIM)
            d.rectangle([957, y - 34, 963, y - 30], fill=DIM)
    return im


scene("new", [
    ("Here's the problem it solves. The game draws with Vulkan, and the Xbox doesn't have a Vulkan driver.",
     None, card_stack(1)),
    ("So this build brings its own: Mesa's Dozen driver. It turns every Vulkan call into Direct3D 12.",
     "So this build brings its own: Mesa's Dozen driver. It turns every Vulkan call into Direct 3D twelve.",
     card_stack(2)),
    ("And Direct3D 12 is something the Xbox speaks fluently.",
     "And Direct 3D twelve is something the Xbox speaks fluently.", card_stack(3)),
    ("We patched Dozen so it draws straight into the app's own window, which is how Xbox apps have to do it.",
     None, card_stack(4)),
])

pieces = ["vulkan_dzn.dll  —  Mesa 26.3.0, Dozen only",
          "dxil.dll  —  signs the shaders Dozen makes",
          "Built by GitHub Actions on every update",
          "Nothing extra to install"]


def card_pieces(n):
    im, d = canvas(0, TOTAL, "What's inside the package")
    for i, item in enumerate(pieces[:n]):
        d.ellipse([120, 296 + i * 115, 140, 316 + i * 115], fill=ACCENT)
        d.text((168, 280 + i * 115), item, font=F_BODY, fill=TEXT)
    return im


scene("pieces", [
    ("Two new files ride along inside the app package.", None, card_pieces(0)),
    ("The driver itself, vulkan dzn dot dll, built from Mesa 26.3.0.",
     "The driver itself, vulkan D Z N dot D L L, built from Mesa twenty-six point three.", card_pieces(1)),
    ("And dxil.dll, from Microsoft, which signs the shaders Dozen creates.",
     "And D X I L dot D L L, from Microsoft, which signs the shaders Dozen creates.", card_pieces(2)),
    ("GitHub builds both automatically, so there's nothing extra for you to install.", None, card_pieces(4)),
])


def card_update(n):
    im, d = canvas(1, TOTAL, "Update the app")
    window(d, 120, 240, 1680, 560, "https://<your-xbox-ip>:11443  ·  Xbox Device Portal")
    d.text((180, 340), "My games & apps  →  Add", font=F_BODY, fill=TEXT)
    if n >= 1:
        d.rounded_rectangle([180, 420, 1740, 500], 12, fill=PANEL2)
        d.text((210, 438), "TLOUUwp_0.3.0.0_x64.msix", font=F_MONO, fill=ACCENT)
        d.text((1240, 444), "from the Xbox zip", font=F_SMALL, fill=DIM)
    if n >= 2:
        d.rounded_rectangle([180, 530, 1740, 610], 12, fill=PANEL2)
        d.text((210, 548), "Version 0.3.0.0 installs over 0.2.0.0", font=F_SMALL, fill=TEXT)
        d.text((1240, 548), "saves and game files stay", font=F_SMALL, fill=DIM)
    if n >= 3:
        d.text((180, 830), "New here? Video 2 shows the whole first-time setup.", font=F_SMALL, fill=DIM)
    return im


scene("update", [
    ("Step one: update the app. Download the Xbox zip from release v0.2.2-uwp.3 on GitHub.",
     "Step one: update the app. Download the Xbox zip from release zero point two point two, U W P point three, "
     "on GitHub.", card_update(0)),
    ("In Device Portal, add the new .msix package, just like before.",
     "In Device Portal, add the new M S I X package, just like before.", card_update(1)),
    ("It's version 0.3.0.0, so it installs right over the old one. Your game files and saves stay put.",
     "It's version zero point three, so it installs right over the old one. Your game files and saves stay put.",
     card_update(2)),
    ("If this is your very first time, video two walks you through the whole setup.", None, card_update(3)),
])


def launcher(selected, info, step=2, heading="Check the renderer"):
    im, d = canvas(step, TOTAL, heading)
    d.rounded_rectangle([120, 220, 1800, 960], 20, fill=(14, 18, 16), outline=PANEL2, width=3)
    d.text((170, 250), "The Last of Us Remastered — UWP", font=font(42, True), fill=TEXT)
    d.rectangle([170, 312, 390, 316], fill=ACCENT)
    items = [("Play", ""), ("Receive files over Wi-Fi", "Off"), ("Game folder", "LocalState\\TLOU"),
             ("Motion blur", "Off"), ("Vulkan driver", "Auto (Mesa Dozen)")]
    for i, (k, v) in enumerate(items):
        y = 350 + i * 66
        if i == selected:
            d.rounded_rectangle([156, y - 6, 1010, y + 50], 10, fill=ACCENT)
        d.text((170, y), k, font=F_SMALL, fill=TEXT)
        d.text((640, y), v, font=F_SMALL, fill=TEXT if i == selected else DIM)
    d.rounded_rectangle([1050, 346, 1770, 900], 14, fill=PANEL)
    for i, (line, color) in enumerate(info):
        d.text((1080, 370 + i * 48), line, font=F_MONO_S, fill=color)
    return im


checked = [("CUSA00552 ........ found", DIM), ("CUSA00552-patch .. found", DIM), ("stage\\boot.json .. found", DIM),
           ("file check: OK", ACCENT), ("", DIM), ("Renderer:", TEXT), ("Mesa Dozen", ACCENT),
           ("(Vulkan on Direct3D 12)", ACCENT)]

scene("renderer", [
    ("Step two: open the app. In the side panel, look for the renderer line.", None, launcher(-1, checked)),
    ("It should say Mesa Dozen, Vulkan on Direct3D 12. That means the driver made it into the package.",
     "It should say Mesa Dozen, Vulkan on Direct 3D twelve. That means the driver made it into the package.",
     launcher(-1, checked)),
    ("There's also a new setting called Vulkan driver. Leave it on Auto, and the app uses Dozen.",
     None, launcher(4, checked)),
    ("The other choice, Khronos loader, is only for testing. Most Xboxes won't have what it needs.",
     None, launcher(4, checked)),
])

recap = ["Xbox: Receive files over Wi-Fi → address + code",
         "PC: Setup-TLOU-Xbox.cmd → 1) Over Wi-Fi",
         "Already sent before? It skips what's there",
         "Launcher: file check says OK"]


def card_recap(n):
    im, d = canvas(3, TOTAL, "Game files: same as before")
    for i, item in enumerate(recap[:n]):
        d.text((120, 270 + i * 115), f"{i + 1}", font=F_H, fill=ACCENT)
        d.text((190, 278 + i * 115), item, font=F_BODY, fill=TEXT)
    return im


scene("recap", [
    ("Step three: the game files. Nothing changed here.", None, card_recap(0)),
    ("If you haven't sent them yet, turn on Receive files over Wi-Fi on the Xbox,", None, card_recap(1)),
    ("and run Setup TLOU Xbox on your PC, choosing Wi-Fi.",
     "and run Setup, T L O U, Xbox on your PC, choosing Wi-Fi.", card_recap(2)),
    ("Already sent them with the last build? Great. They're still there, and the setup skips them anyway.",
     None, card_recap(3)),
    ("Either way, wait for the file check to say OK.", None, card_recap(4)),
])

expect = [("First launch is slow", "Dozen builds and signs every shader once, then caches it"),
          ("It's experimental", "Never tested on a console yet: you're the first!"),
          ("It may stop early", "a missing Vulkan feature, or memory (about 5 GiB on Xbox)")]


def card_expect(n):
    im, d = canvas(4, TOTAL, "Press Play: what to expect")
    for i, (head, body) in enumerate(expect[:n]):
        y = 250 + i * 170
        d.rounded_rectangle([120, y, 1800, y + 140], 18, fill=PANEL)
        d.rectangle([120, y, 132, y + 140], fill=WARN if i == 2 else ACCENT)
        d.text((170, y + 22), head, font=F_H, fill=TEXT)
        d.text((170, y + 90), body, font=F_SMALL, fill=DIM)
    return im


scene("expect", [
    ("Step four: press Play. And here's an honest look at what to expect.", None, card_expect(0)),
    ("The first launch is slow. Dozen builds and signs every shader once, and then caches it for next time.",
     None, card_expect(1)),
    ("This is experimental. Nobody has run it on a console yet, so you're the first to try it!",
     None, card_expect(2)),
    ("It might stop early, maybe on a Vulkan feature Dozen doesn't have, or because Xbox gives games "
     "only about five gigabytes of memory.", None, card_expect(3)),
    ("If it does, that's not a dead end. The logs tell us exactly what to fix next.", None, card_expect(3)),
])

logs = [(0, "LocalState\\", ""), (1, "boot.log", "what the game did"), (1, "app.log", "what the app did"),
        (1, "user\\cache\\mesa\\", "Dozen's shader cache")]


def card_logs(n, send=False):
    im, d = canvas(5, TOTAL, "Send the logs")
    window(d, 120, 240, 1100, 520, "Device Portal  ·  File explorer → LocalAppData → the app")
    for i, (indent, name, note) in enumerate(logs[:n]):
        color = ACCENT if 0 < i < 3 else TEXT
        d.text((180 + indent * 50, 340 + i * 80), ("├─ " if indent else "") + name, font=F_MONO_S, fill=color)
        if note:
            d.text((620, 344 + i * 80), note, font=F_SMALL, fill=DIM)
    if send:
        d.rounded_rectangle([1280, 240, 1800, 760], 18, fill=PANEL2)
        d.text((1320, 280), "Send these two", font=F_BODY, fill=TEXT)
        d.text((1320, 360), "boot.log", font=F_MONO, fill=ACCENT)
        d.text((1320, 420), "app.log", font=F_MONO, fill=ACCENT)
        d.text((1320, 510), "Also handy from the zip:", font=F_SMALL, fill=DIM)
        d.text((1320, 555), "dozen-imports.txt", font=F_MONO_S, fill=TEXT)
        d.text((1320, 600), "runtime-imports.txt", font=F_MONO_S, fill=TEXT)
    return im


scene("logs", [
    ("So, step five: grab the logs. In Device Portal, open the file explorer, then Local App Data, then the app.",
     None, card_logs(1)),
    ("Inside Local State, you'll find boot dot log, which shows what the game did,",
     None, card_logs(2)),
    ("and app dot log, which shows what the app did.", None, card_logs(3)),
    ("Send those two, and we can see exactly how far it got, and what to fix next.", None, card_logs(4, True)),
])

trouble = [("Renderer: no Vulkan driver", "You have an older package: install v0.2.2-uwp.3"),
           ("Error 126 on Play", "A system DLL is missing: send app.log"),
           ("Stops at the first frame", "Send boot.log: it names the failing step"),
           ("Very slow at first", "Shaders being built: the next start is faster")]


def card_trouble(n):
    im, d = canvas(0, TOTAL, "Quick fixes")
    for i, (head, body) in enumerate(trouble[:n]):
        y = 260 + i * 130
        d.text((120, y), head, font=F_BODY, fill=ACCENT)
        d.text((800, y + 6), body, font=F_SMALL, fill=TEXT)
    return im


scene("trouble", [
    ("A few quick fixes.", None, card_trouble(0)),
    ("If the renderer line says no Vulkan driver, you're on an older package. Install the new one.",
     None, card_trouble(1)),
    ("Error 126 when you press Play means a system file is missing on the console. app dot log says which.",
     None, card_trouble(2)),
    ("If it stops right at the first frame, boot dot log names the exact step that failed.",
     None, card_trouble(3)),
    ("And if it's just slow at first, that's the shaders being built. It gets faster on the next start.",
     None, card_trouble(4)),
])


def card_outro():
    im, d = canvas(0, TOTAL, None)
    d.text((120, 330), "Thanks for testing!", font=F_TITLE, fill=TEXT)
    d.rectangle([120, 440, 420, 448], fill=ACCENT)
    d.text((120, 500), "All three help videos, the newest builds, and where to send logs:", font=F_BODY, fill=DIM)
    d.text((120, 570), "github.com/NJB3300-art/tlou", font=font(48, mono=True), fill=ACCENT)
    return im


scene("outro", [
    ("And that's the renderer build! All three help videos, and every new build, are on the GitHub page. "
     "Thanks for testing, and good luck out there!", None, card_outro()),
])


sys.path.insert(0, "/home/claude/tts")
import kokoro_tts  # noqa: E402


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        sys.exit(" ".join(map(str, cmd)) + "\n" + r.stderr[-3000:])
    return r.stdout


def ts(t, vtt=False):
    ms = int(round(t * 1000))
    h, ms = divmod(ms, 3600000)
    m, ms = divmod(ms, 60000)
    s, ms = divmod(ms, 1000)
    return f"{h:02d}:{m:02d}:{s:02d}{'.' if vtt else ','}{ms:03d}"


def main(preview_only=False):
    import numpy as np
    cues, scene_files = [], []
    voice = []  # 24 kHz float samples of the whole narration
    clock = 0.0
    for si, (name, beats) in enumerate(SCENES):
        parts = []
        for bi, (shown, spoken, image) in enumerate(beats):
            img = OUT / f"{si:02d}_{name}_{bi}.png"
            image.save(img)
            if preview_only:
                continue
            lead = 0.5 if bi == 0 else 0.0
            audio = kokoro_tts.synthesize(spoken or shown, VOICE, SPEED)
            # trim the model's leading/trailing near-silence, then a natural gap after the line
            loud = np.flatnonzero(np.abs(audio) > 0.01)
            if len(loud):
                audio = audio[max(0, loud[0] - 600):loud[-1] + 2400]
            spoken_s = len(audio) / 24000
            seconds = lead + spoken_s + GAP
            voice += [np.zeros(int(lead * 24000), np.float32), audio, np.zeros(int(GAP * 24000), np.float32)]
            cues.append((clock + lead, clock + lead + spoken_s, shown))
            clock += seconds
            parts.append((img, seconds))
        if preview_only:
            continue
        total = sum(s for _, s in parts)
        inputs, chains = [], []
        for i, (img, seconds) in enumerate(parts):
            inputs += ["-framerate", str(FPS), "-i", str(img)]
            chains.append(f"[{i}:v]format=yuv420p,loop=-1:1,trim=duration={seconds:.3f},setpts=PTS-STARTPTS[v{i}]")
        concat = "".join(f"[v{i}]" for i in range(len(parts))) + f"concat=n={len(parts)}:v=1:a=0"
        fade = f",fade=t=in:st=0:d=0.4,fade=t=out:st={total - 0.4:.3f}:d=0.4[out]"
        tmp, final = OUT / f"scene_{si:02d}.tmp.mp4", OUT / f"scene_{si:02d}.mp4"
        run(["ffmpeg", "-y", "-loglevel", "error", *inputs, "-filter_complex", ";".join(chains) + ";" + concat + fade,
             "-map", "[out]", "-r", str(FPS), "-c:v", "libx264", "-crf", "20", "-preset", "veryfast",
             "-pix_fmt", "yuv420p", "-t", f"{total:.3f}", str(tmp)])
        tmp.rename(final)
        scene_files.append(final)
        print(f"scene {name}: {total:.1f}s", flush=True)
    if preview_only:
        return
    # the frame-exact length of each scene file decides the timeline; the voice is laid on that
    kokoro_tts.write_wav(OUT / "voice.wav", np.concatenate(voice))
    (OUT / "scenes.txt").write_text("".join(f"file '{f.name}'\n" for f in scene_files))
    run(["ffmpeg", "-y", "-loglevel", "error", "-f", "concat", "-safe", "0", "-i", str(OUT / "scenes.txt"),
         "-c", "copy", str(OUT / "video.mp4")])
    # gentle polish: a touch of warmth and compression, normalized loudness, 48 kHz stereo AAC
    run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(OUT / "voice.wav"), "-af",
         "highpass=f=70,equalizer=f=200:t=q:w=1:g=1.5,equalizer=f=4500:t=q:w=1:g=1.5,"
         "acompressor=threshold=-20dB:ratio=2.5:attack=8:release=120,loudnorm=I=-16:TP=-1.5:LRA=9,aresample=48000",
         "-ac", "2", "-c:a", "aac", "-b:a", "160k", str(OUT / "voice.m4a")])
    srt = "".join(f"{i + 1}\n{ts(a)} --> {ts(b)}\n{t}\n\n" for i, (a, b, t) in enumerate(cues))
    vtt = "WEBVTT\n\n" + "".join(f"{ts(a, True)} --> {ts(b, True)}\n{t}\n\n" for a, b, t in cues)
    (OUT / "help-renderer.srt").write_text(srt)
    (OUT / "help-renderer.vtt").write_text(vtt)
    run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(OUT / "video.mp4"), "-i", str(OUT / "voice.m4a"),
         "-i", str(OUT / "help-renderer.srt"), "-map", "0:v", "-map", "1:a", "-map", "2:s", "-c:v", "copy", "-c:a", "copy",
         "-c:s", "mov_text", "-metadata:s:s:0", "language=eng", "-metadata:s:s:0", "title=English (CC)",
         "-disposition:s:0", "default", "-movflags", "+faststart", str(OUT / "help-renderer.tmp.mp4")])
    (OUT / "help-renderer.tmp.mp4").rename(OUT / "help-renderer.mp4")
    print(json.dumps({"seconds": round(clock, 1), "cues": len(cues)}))


if __name__ == "__main__":
    main(preview_only="--preview" in sys.argv)
