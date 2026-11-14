"""Setup video for the TLOU Remastered UWP port: stills drawn with PIL, flite narration, captions.

Each scene is a list of beats: one narrated sentence and the picture shown while it is spoken (the
pictures build up step by step). Output: help.mp4 (soft CC track), help.vtt, help.srt."""
import json
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

W, H, FPS = 1920, 1080, 30
OUT = Path(__file__).parent / "build2"
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
    d.text((120, H - 70), "The Last of Us Remastered \u2014 UWP port  \u00b7  Wi-Fi setup (v0.2.2-uwp.2)", font=F_SMALL, fill=DIM)
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


# Scenes of the second help video: the Wi-Fi build (v0.2.2-uwp.2).
# Each beat: (caption, spoken text or None for "same as caption", picture).

TOTAL = 5
SCENES = []


def scene(name, beats):
    SCENES.append((name, beats))


def card_title(sub):
    im, d = canvas(0, TOTAL, None)
    d.text((120, 300), "The Last of Us Remastered", font=F_TITLE, fill=TEXT)
    d.text((120, 410), "Wi-Fi setup on Xbox", font=font(64), fill=ACCENT)
    d.rectangle([120, 515, 420, 523], fill=ACCENT)
    if sub:
        d.text((120, 570), "Build v0.2.2-uwp.2  \u00b7  no USB drive needed", font=F_BODY, fill=DIM)
        d.text((120, 640), "Second help video \u2014 the first one covers the USB setup", font=F_SMALL, fill=DIM)
    return im


scene("title", [
    ("Hey, welcome back! This is the setup guide for the newest build of The Last of Us Remastered on Xbox.",
     None, card_title(0)),
    ("And this time, there's no USB drive needed. Your PC sends the whole game to the Xbox over Wi-Fi!",
     None, card_title(1)),
])

new_items = [("Wi-Fi transfer", "the launcher receives the game from your PC"),
             ("One-click setup", "Setup-TLOU-Xbox.cmd finds, prepares and sends it"),
             ("File check", "the launcher checks game, update and stage")]


def card_new(n):
    im, d = canvas(0, TOTAL, "What's new in this build")
    for i, (head, body) in enumerate(new_items[:n]):
        y = 260 + i * 170
        d.rounded_rectangle([120, y, 1800, y + 140], 18, fill=PANEL)
        d.rectangle([120, y, 132, y + 140], fill=ACCENT)
        d.text((170, y + 22), head, font=F_H, fill=TEXT)
        d.text((170, y + 90), body, font=F_SMALL, fill=DIM)
    return im


scene("new", [
    ("So, what's new? Three things.", None, card_new(0)),
    ("First: Wi-Fi transfer. The launcher on your Xbox can now receive the game straight from your PC.",
     None, card_new(1)),
    ("Second: a one-click setup. A single file on your PC finds your game, prepares it, and sends it over.",
     None, card_new(2)),
    ("And third: a file check. Before you play, the launcher makes sure everything matches.", None, card_new(3)),
])

need = ["Xbox Series X or S in Developer Mode",
        "A Windows PC on the same network, with Python 3.10+",
        "Your own PS4 dump: CUSA00552 + update 1.11",
        "Not the Steam game: Part I is a different game"]


def card_need(n):
    im, d = canvas(0, TOTAL, "What you need")
    for i, item in enumerate(need[:n]):
        color = WARN if i == 3 else ACCENT
        d.ellipse([120, 286 + i * 120, 140, 306 + i * 120], fill=color)
        d.text((168, 270 + i * 120), item, font=F_BODY, fill=TEXT)
    return im


scene("need", [
    ("Here's what you need.", None, card_need(0)),
    ("An Xbox Series X or S, in Developer Mode.", None, card_need(1)),
    ("A Windows PC on the same network, with Python 3.10 or newer.", None, card_need(2)),
    ("And your own PS4 dump of the game, CUSA00552, plus update 1.11.",
     "And your own PS4 dump of the game, C U S A, double-oh five five two, plus update one point eleven.",
     card_need(3)),
    ("Quick heads-up: the Steam version, The Last of Us Part I, is a different game, so it won't work here.",
     None, card_need(4)),
])


def card_install(n):
    im, d = canvas(1, TOTAL, "Install the app")
    window(d, 120, 240, 1680, 640, "https://<your-xbox-ip>:11443  \u00b7  Xbox Device Portal")
    d.text((180, 340), "My games & apps", font=F_BODY, fill=TEXT)
    if n >= 1:
        d.rounded_rectangle([180, 420, 1740, 500], 12, fill=PANEL2)
        d.text((210, 438), "TLOUUwp_0.2.0.0_x64.msix", font=F_MONO, fill=ACCENT)
        d.text((1240, 444), "from the Xbox zip", font=F_SMALL, fill=DIM)
        d.rounded_rectangle([180, 520, 1740, 600], 12, fill=PANEL2)
        d.text((210, 538), "Microsoft.VCLibs.x64.14.00.appx", font=F_MONO, fill=ACCENT)
        d.text((1240, 544), "dependency", font=F_SMALL, fill=DIM)
    if n >= 2:
        d.rounded_rectangle([180, 650, 1100, 830], 16, fill=PANEL2)
        d.text((210, 670), "Dev Home \u2192 app details", font=F_SMALL, fill=DIM)
        d.text((210, 715), "App type:", font=F_BODY, fill=TEXT)
        button(d, 430, 715, "Game", on=True)
    return im


scene("install", [
    ("Step one: install the app. Grab the Xbox zip from the newest release on GitHub.",
     None, card_install(0)),
    ("In Device Portal, under My games and apps, add the .msix package, along with the VCLibs dependency.",
     "In Device Portal, under My games and apps, add the M S I X package, along with the V C Libs dependency.",
     card_install(1)),
    ("Then, in Dev Home, set the app type to Game. That gives it way more memory, and it really needs it!",
     None, card_install(2)),
])


def launcher(selected, wifi_on, extra=None, check=None):
    im, d = canvas(2 if extra != "play" else 5, TOTAL,
                   "Turn on Wi-Fi transfer" if extra != "play" else "Check, then play")
    d.rounded_rectangle([120, 220, 1800, 960], 20, fill=(14, 18, 16), outline=PANEL2, width=3)
    d.text((170, 250), "The Last of Us Remastered \u2014 UWP", font=font(42, True), fill=TEXT)
    d.rectangle([170, 312, 390, 316], fill=ACCENT)
    items = [("Play", ""), ("Receive files over Wi-Fi", "On" if wifi_on else "Off"),
             ("Game folder", "LocalState\\TLOU"), ("Motion blur", "Off"), ("Align contact shadows", "On")]
    for i, (k, v) in enumerate(items):
        y = 350 + i * 66
        if i == selected:
            d.rounded_rectangle([156, y - 6, 1010, y + 50], 10, fill=ACCENT)
        d.text((170, y), k, font=F_SMALL, fill=TEXT)
        d.text((680, y), v, font=F_SMALL, fill=TEXT if i == selected else DIM)
    d.rounded_rectangle([1050, 346, 1770, 900], 14, fill=PANEL)
    lines = []
    if wifi_on:
        lines += [("Wi-Fi transfer", TEXT), ("On the PC, open:", DIM), ("http://192.168.1.20:8077", ACCENT),
                  ("Code:  482913", ACCENT), ("", DIM)]
        if extra == "receiving":
            lines += [("1,204 files, 18,330 MiB received", DIM), ("receiving CUSA00552/...", DIM)]
    if check:
        lines += [("CUSA00552 ........ found", DIM), ("CUSA00552-patch .. found", DIM), ("stage\\boot.json .. found", DIM),
                  ("versions: game 01.00,", DIM), ("          update 01.11", DIM), ("file check: OK", ACCENT)]
    for i, (line, color) in enumerate(lines):
        d.text((1080, 370 + i * 48), line, font=F_MONO_S, fill=color)
    return im


scene("wifi_on", [
    ("Step two happens on the Xbox. Open the app, and pick Receive files over Wi-Fi.",
     None, launcher(1, False)),
    ("Press A, and it switches on. The TV now shows an address, and a six-digit code.", None, launcher(1, True)),
    ("Keep this screen open. You'll type both of these on your PC in a second.", None, launcher(1, True)),
])

console_lines = [
    ("The Last of Us Remastered - setup for the Xbox app", TEXT),
    ("Looking for your PS4 dump (CUSA00552 and update 1.11) ...", DIM),
    ("Game:   D:\\PS4\\CUSA00552  (version 01.00)", ACCENT),
    ("Update: D:\\PS4\\CUSA00552-patch  (version 01.11)", ACCENT),
    ("Preparing the game for the console ...", DIM),
    ("How should the files get to the Xbox?", TEXT),
    ("  1) Over Wi-Fi    2) Copy to a USB drive   > 1", TEXT),
    ("Address shown on the TV: 192.168.1.20", ACCENT),
    ("Code shown on the TV: 482913", ACCENT),
]


def card_setup(n, kit=False):
    im, d = canvas(3, TOTAL, "Run the one-click setup on the PC")
    if kit:
        tree_lines = [(0, "tlou-uwp-stage-kit\\", ""), (1, "Setup-TLOU-Xbox.cmd", "double-click"),
                      (1, "Stage-TLOU-Xbox.cmd", ""), (1, "tools\\", "")]
        tree(d, tree_lines, 4, 160, 300, highlight=1, f=F_MONO_S, gap=64)
        return im
    d.rounded_rectangle([120, 240, 1800, 900], 18, fill=(8, 10, 9))
    for i, (line, color) in enumerate(console_lines[:n]):
        d.text((160, 270 + i * 66), line, font=F_MONO_S, fill=color)
    return im


scene("setup", [
    ("Step three, on your PC. Unzip the stage kit, and double-click Setup TLOU Xbox.",
     "Step three, on your PC. Unzip the stage kit, and double-click Setup, T L O U, Xbox.", card_setup(0, kit=True)),
    ("It looks through your drives for the game and the update, all by itself. The folder names don't matter.",
     None, card_setup(4)),
    ("Then it prepares the game for the console. The first time, that takes a few minutes.", None, card_setup(5)),
    ("When it asks how to send the files, choose one, for Wi-Fi.", None, card_setup(7)),
    ("Type in the address and the code from your TV, and off it goes!", None, card_setup(9)),
])


def card_transfer(pct, note=None):
    im, d = canvas(4, TOTAL, "Sit back while it sends")
    total = 46.8
    d.rounded_rectangle([120, 260, 1800, 560], 18, fill=PANEL)
    d.text((170, 300), f"{total * pct:5.2f} / {total:.2f} GiB", font=F_MONO, fill=TEXT)
    d.text((1120, 304), f"{int(pct * 100)}%", font=F_MONO, fill=ACCENT)
    d.rounded_rectangle([170, 380, 1750, 420], 20, fill=PANEL2)
    if pct > 0:
        d.rounded_rectangle([170, 380, 170 + int(1580 * pct), 420], 20, fill=ACCENT)
    d.text((170, 460), "CUSA00552/... \u00b7 sent in 32 MiB pieces", font=F_SMALL, fill=DIM)
    if note:
        d.rounded_rectangle([120, 620, 1800, 820], 18, fill=PANEL2)
        d.text((170, 650), note[0], font=F_BODY, fill=TEXT)
        d.text((170, 720), note[1], font=F_SMALL, fill=DIM)
    return im


scene("transfer", [
    ("Now the game travels over your network. It's a big one, so this part takes a while.",
     None, card_transfer(0.08)),
    ("A wired connection on either end makes it a lot faster.", None, card_transfer(0.31)),
    ("And if anything interrupts it, no problem. Just run the setup again.",
     None, card_transfer(0.55, ("Interrupted?", "Run Setup-TLOU-Xbox.cmd again: it skips what's there and continues."))),
    ("Files that are already on the Xbox get skipped, and a half-sent file carries on right where it stopped.",
     None, card_transfer(0.82, ("Interrupted?", "Run Setup-TLOU-Xbox.cmd again: it skips what's there and continues."))),
    ("And there it is. All done!", None, card_transfer(1.0)),
])


def card_browser(n):
    im, d = canvas(0, TOTAL, "No Python? Use your browser")
    window(d, 120, 240, 1680, 680, "http://192.168.1.20:8077  \u00b7  Send the game to the Xbox")
    d.text((180, 330), "Code shown on the TV", font=F_SMALL, fill=TEXT)
    d.rounded_rectangle([180, 375, 460, 435], 10, fill=PANEL2)
    if n >= 1:
        d.text((205, 385), "482913", font=F_MONO, fill=ACCENT)
    labels = ["Game folder (CUSA00552)", "Update folder (CUSA00552-patch)", "Stage folder (uwp-stage)"]
    for i, label in enumerate(labels):
        y = 470 + i * 100
        d.text((180, y), label, font=F_SMALL, fill=TEXT)
        button(d, 980, y - 10, "Choose folder", on=(n >= 2))
    if n >= 3:
        button(d, 180, 790, "Send", on=True)
    return im


scene("browser", [
    ("Don't want to use Python? That address on the TV is also a web page.", None, card_browser(0)),
    ("Open it in your browser, and type in the code.", None, card_browser(1)),
    ("Pick the game folder, the update folder, and a stage folder that you made on another PC.",
     None, card_browser(2)),
    ("Hit Send, and it does the exact same transfer.", None, card_browser(3)),
])

scene("play", [
    ("Last step: back to the Xbox. The launcher spots the new files automatically.",
     None, launcher(0, False, "play", check=True)),
    ("Its file check looks at the game, the update version, and the stage. Here, everything says OK.",
     None, launcher(0, False, "play", check=True)),
    ("So pick Play, or just press the menu button, and you're in!", None, launcher(0, False, "play", check=True)),
])

trouble = [("Wrong code?", "Use the six digits on the TV right now: they change each time"),
           ("PC can't connect?", "Same network for both; type the address exactly as shown"),
           ("File check problem?", "The launcher says what's wrong; run the setup again"),
           ("App closes?", "Read app.log and boot.log in LocalState (Device Portal)")]


def card_trouble(n, limit=False):
    im, d = canvas(0, TOTAL, "Quick fixes")
    for i, (head, body) in enumerate(trouble[:n]):
        y = 250 + i * 118
        d.text((120, y), head, font=F_BODY, fill=ACCENT)
        d.text((560, y + 6), body, font=F_SMALL, fill=TEXT)
    if limit:
        d.rounded_rectangle([120, 750, 1800, 900], 18, fill=PANEL2)
        d.text((160, 770), "Known limit", font=F_BODY, fill=WARN)
        d.text((160, 835), "Xbox has no Vulkan driver yet: the game may stop when it starts its renderer.",
               font=F_SMALL, fill=TEXT)
    return im


scene("trouble", [
    ("A few quick fixes, just in case.", None, card_trouble(0)),
    ("Wrong code? Use the six digits on the TV right now. They change every time you turn the receiver on.",
     None, card_trouble(1)),
    ("Can't connect? Make sure both are on the same network, and type the address exactly as shown.",
     None, card_trouble(2)),
    ("If the file check finds a problem, the launcher tells you exactly what's wrong.", None, card_trouble(3)),
    ("And if the app closes, the logs in its LocalState folder will tell you why.",
     "And if the app closes, the logs in its Local State folder will tell you why.", card_trouble(4)),
    ("One honest heads-up: the Xbox has no Vulkan driver yet, so the game may stop when it starts its renderer. "
     "The help page has the details.", None, card_trouble(4, True)),
])


def card_outro():
    im, d = canvas(0, TOTAL, None)
    d.text((120, 340), "That's it. Have fun!", font=F_TITLE, fill=TEXT)
    d.rectangle([120, 450, 420, 458], fill=ACCENT)
    d.text((120, 510), "Help page, both videos, and the newest builds:", font=F_BODY, fill=DIM)
    d.text((120, 580), "github.com/NJB3300-art/tlou", font=font(48, mono=True), fill=ACCENT)
    return im


scene("outro", [
    ("And that's it! Both help videos, and every new build, are on the GitHub page. Have fun out there!",
     None, card_outro()),
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
    (OUT / "help-wifi.srt").write_text(srt)
    (OUT / "help-wifi.vtt").write_text(vtt)
    run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(OUT / "video.mp4"), "-i", str(OUT / "voice.m4a"),
         "-i", str(OUT / "help-wifi.srt"), "-map", "0:v", "-map", "1:a", "-map", "2:s", "-c:v", "copy", "-c:a", "copy",
         "-c:s", "mov_text", "-metadata:s:s:0", "language=eng", "-metadata:s:s:0", "title=English (CC)",
         "-disposition:s:0", "default", "-movflags", "+faststart", str(OUT / "help-wifi.tmp.mp4")])
    (OUT / "help-wifi.tmp.mp4").rename(OUT / "help-wifi.mp4")
    print(json.dumps({"seconds": round(clock, 1), "cues": len(cues)}))


if __name__ == "__main__":
    main(preview_only="--preview" in sys.argv)
