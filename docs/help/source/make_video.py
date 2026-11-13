"""Setup video for the TLOU Remastered UWP port: stills drawn with PIL, flite narration, captions.

Each scene is a list of beats: one narrated sentence and the picture shown while it is spoken (the
pictures build up step by step). Output: help.mp4 (soft CC track), help.vtt, help.srt."""
import json
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

W, H, FPS = 1920, 1080, 30
OUT = Path(__file__).parent / "build"
OUT.mkdir(exist_ok=True)
VOICE, TEMPO, GAP = "slt", 0.92, 0.45

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
    d.text((120, H - 70), "The Last of Us Remastered \u2014 UWP port  \u00b7  setup guide", font=F_SMALL, fill=DIM)
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


TOTAL = 5
SCENES = []  # (name, [(sentence, image)])


def scene(name, beats):
    SCENES.append((name, beats))


# ---- 1. title ----
def title_card(sub_shown):
    im, d = canvas(0, TOTAL, None)
    d.text((120, 330), "The Last of Us Remastered", font=F_TITLE, fill=TEXT)
    d.text((120, 440), "on Xbox (Developer Mode)", font=font(64), fill=ACCENT)
    d.rectangle([120, 545, 420, 553], fill=ACCENT)
    if sub_shown:
        d.text((120, 600), "Setup guide: from your own game dump to pressing Play", font=F_BODY, fill=DIM)
        d.text((120, 670), "UWP port of ravencaio/tlou-remastered-pc  \u00b7  no game files included", font=F_SMALL, fill=DIM)
    return im

scene("title", [
    ("Welcome. This video shows how to set up The Last of Us Remastered on an Xbox in Developer Mode.", title_card(0)),
    ("It uses the U W P port of the PC version. No game files are included, so you need your own copy of the game.", title_card(1)),
])

# ---- 2. what you need ----
need = ["An Xbox Series X or S in Developer Mode",
        "A Windows PC with Python 3.10 or newer",
        "Your own dump of CUSA00552 and update 1.11",
        "A USB drive, formatted exFAT or NTFS"]
def need_card(n):
    im, d = canvas(0, TOTAL, "What you need")
    bullets(d, need, n, y=270, gap=110)
    return im
scene("need", [
    ("First, what you need.", need_card(0)),
    ("An Xbox Series X or S that is in Developer Mode.", need_card(1)),
    ("A Windows P C with Python 3.10 or newer.", need_card(2)),
    ("Your own dump of the game, C U S A 0 0 5 5 2, and its update 1.11.", need_card(3)),
    ("And a U S B drive, formatted as exFAT or N T F S.", need_card(4)),
])

# ---- 3. download ----
def download_card(n):
    im, d = canvas(1, TOTAL, "Download the release")
    window(d, 120, 240, 1680, 620, "github.com/NJB3300-art/tlou/releases")
    d.text((180, 340), "v0.2.2-uwp.1", font=F_H, fill=TEXT)
    d.text((180, 420), "Assets", font=F_SMALL, fill=DIM)
    files = [("tlou-uwp-xbox-v0.2.2-uwp.1.zip", "the app, for the Xbox"),
             ("tlou-uwp-stage-kit-v0.2.2-uwp.1.zip", "the setup tools, for the PC")]
    for i, (f, note) in enumerate(files[:n]):
        y = 480 + i * 130
        d.rounded_rectangle([180, y, 1740, y + 100], 14, fill=PANEL2)
        d.text((220, y + 26), "\u2193  " + f, font=F_MONO, fill=ACCENT)
        d.text((1240, y + 32), note, font=F_SMALL, fill=DIM)
    return im
scene("download", [
    ("Step one. Open the releases page of the GitHub repository, and pick the newest release.", download_card(0)),
    ("Download the Xbox zip. It holds the app package.", download_card(1)),
    ("Then download the stage kit zip. It holds the tools that prepare the game on your P C.", download_card(2)),
])

# ---- 4. stage on the PC ----
pc_tree = [(0, "Games\\", ""), (1, "CUSA00552\\", "your game"), (1, "CUSA00552-patch\\", "update 1.11"),
           (1, "tlou-uwp-stage-kit\\", "unzipped kit"), (2, "Stage-TLOU-Xbox.cmd", "double-click"),
           (2, "uwp-stage\\", "written by the kit")]
def stage_card(n, hl=None, console=False):
    im, d = canvas(2, TOTAL, "Prepare the game on the PC")
    right = tree(d, pc_tree, n, 160, 300, highlight=hl, f=F_MONO_S, gap=64)
    if console:
        cx = max(right + 40, 1180)
        d.rounded_rectangle([cx, 260, 1800, 740], 20, fill=(8, 10, 9))
        lines = ["> Stage-TLOU-Xbox.cmd", "Preparing CUSA00552 ...", "Linking modules ...",
                 "Writing patches ...", "Staged for the UWP app:", "  uwp-stage (5 files)"]
        for i, line in enumerate(lines):
            d.text((cx + 30, 290 + i * 66), line, font=F_MONO_S, fill=ACCENT if i >= 4 else DIM)
    return im
scene("stage", [
    ("Step two happens on the P C. Unzip the stage kit into a folder.", stage_card(4, 3)),
    ("Put your game folder, C U S A 0 0 5 5 2, and the update folder, C U S A 0 0 5 5 2 dash patch, next to it.", stage_card(4, 2)),
    ("Now double-click Stage T L O U Xbox dot c m d.", stage_card(5, 4)),
    ("It prepares the game once, the same way the P C version does at every start, and writes a folder called u w p stage.", stage_card(6, 5, True)),
    ("Settings that change the game itself, like the render resolution, are chosen here. Set them before you run it.", stage_card(6, 5, True)),
])

# ---- 5. copy to USB ----
usb_tree = [(0, "E:\\TLOU\\", ""), (1, "CUSA00552\\", "the game"), (1, "CUSA00552-patch\\", "update 1.11"),
            (1, "stage\\", "contents of uwp-stage")]
def usb_card(n, hl=None):
    im, d = canvas(3, TOTAL, "Copy everything to the USB drive")
    right = tree(d, usb_tree, n, 160, 310, highlight=hl, gap=86)
    if n >= 4:
        bx = max(right + 50, 1240)
        d.rounded_rectangle([bx, 270, 1800, 640], 20, fill=PANEL2)
        d.text((bx + 40, 310), "Names matter:", font=F_BODY, fill=TEXT)
        d.text((bx + 40, 380), "The app looks for a folder", font=F_SMALL, fill=DIM)
        d.text((bx + 40, 425), "named TLOU on every drive.", font=F_SMALL, fill=DIM)
        d.text((bx + 40, 500), "Rename uwp-stage to stage.", font=F_SMALL, fill=ACCENT)
    return im
scene("usb", [
    ("Step three. On the U S B drive, make a folder called T L O U.", usb_card(1, 0)),
    ("Copy the game folder and the update folder into it.", usb_card(3, 2)),
    ("Then copy the u w p stage folder there too, and rename it to stage.", usb_card(4, 3)),
    ("Plug the drive into the Xbox. The app looks for the T L O U folder on every drive.", usb_card(4)),
])

# ---- 6. install with Device Portal ----
def portal_card(n):
    im, d = canvas(4, TOTAL, "Install the app with Device Portal")
    window(d, 120, 240, 1680, 650, "https://<your-xbox-ip>:11443  \u00b7  Xbox Device Portal")
    d.text((180, 340), "My games & apps", font=F_BODY, fill=TEXT)
    if n >= 1:
        button(d, 180, 420, "Add", on=(n == 1))
    if n >= 2:
        d.rounded_rectangle([180, 520, 1740, 600], 12, fill=PANEL2)
        d.text((210, 538), "TLOUUwp_0.1.1.0_x64.msix", font=F_MONO, fill=ACCENT)
        d.text((1280, 544), "app package", font=F_SMALL, fill=DIM)
    if n >= 3:
        d.rounded_rectangle([180, 620, 1740, 700], 12, fill=PANEL2)
        d.text((210, 638), "Microsoft.VCLibs.x64.14.00.appx", font=F_MONO, fill=ACCENT)
        d.text((1280, 644), "dependency", font=F_SMALL, fill=DIM)
    if n >= 4:
        button(d, 180, 740, "Start install", on=True)
    return im
scene("portal", [
    ("Step four. On the Xbox, open Dev Home. It shows the address of Device Portal. Open that address in a browser on your P C.", portal_card(0)),
    ("Under My games and apps, choose Add.", portal_card(1)),
    ("Unzip the Xbox zip, and select the app package, the file ending in dot m s i x.", portal_card(2)),
    ("When it asks for dependencies, add the V C Libs x 64 package from the same zip.", portal_card(3)),
    ("Then start the install, and wait for it to finish.", portal_card(4)),
])

# ---- 7. set as Game + play ----
def game_card(stage):
    im, d = canvas(5, TOTAL, "Set it as a Game, then play")
    if stage < 2:
        window(d, 120, 240, 820, 560, "Dev Home  \u00b7  app details")
        d.text((180, 340), "TLOU Remastered (UWP port)", font=F_SMALL, fill=TEXT)
        d.text((180, 420), "App type", font=F_SMALL, fill=DIM)
        x = button(d, 180, 470, "App", on=False)
        button(d, x + 20, 470, "Game", on=(stage >= 1))
        if stage >= 1:
            d.text((180, 580), "Games get much more memory and GPU.", font=F_SMALL, fill=ACCENT)
    else:
        # the app's own launcher, as it looks
        d.rounded_rectangle([120, 220, 1800, 940], 20, fill=(14, 18, 16), outline=PANEL2, width=3)
        d.text((180, 250), "The Last of Us Remastered \u2014 UWP", font=font(46, True), fill=TEXT)
        d.rectangle([180, 316, 400, 320], fill=ACCENT)
        items = [("Play", ""), ("Game folder", "E:\\TLOU"), ("Motion blur", "Off"),
                 ("Align contact shadows", "On"), ("Shaders in background", "On")]
        for i, (k, v) in enumerate(items):
            y = 360 + i * 70
            if i == 0:
                d.rounded_rectangle([166, y - 6, 1000, y + 52], 10, fill=ACCENT)
            d.text((180, y), k, font=F_SMALL, fill=TEXT)
            d.text((640, y), v, font=F_SMALL, fill=TEXT if i == 0 else DIM)
        d.rounded_rectangle([1060, 356, 1760, 720], 14, fill=PANEL)
        for i, line in enumerate(["CUSA00552 ........ found", "CUSA00552-patch .. found",
                                  "stage\\boot.json .. found", "staged, game 01.11"]):
            d.text((1090, 380 + i * 60), line, font=F_MONO_S, fill=DIM)
        pad_button(d, 180, 820, "A", (60, 150, 70))
        d.text((262, 830), "select", font=F_SMALL, fill=DIM)
        d.rounded_rectangle([460, 826, 540, 878], 26, fill=PANEL2)
        d.text((474, 830), "\u2261", font=F_BODY, fill=TEXT)
        d.text((560, 830), "play", font=F_SMALL, fill=DIM)
    return im
scene("game", [
    ("Step five. In Dev Home, select the app, and open its details.", game_card(0)),
    ("Set the app type to Game. Games get much more memory, and the runtime needs about six gigabytes.", game_card(1)),
    ("Now start the app. The launcher shows whether it found the game, the update, and the stage folder.", game_card(2)),
    ("Press A to change a setting. Choose Play, or press the menu button, to start the game.", game_card(2)),
])

# ---- 8. troubleshooting ----
trouble = ["Nothing happens?  Read app.log",
           "The game stopped?  Read boot.log",
           "Both are in the app's LocalState folder",
           "Device Portal \u2192 File explorer \u2192 LocalAppData"]
def trouble_card(n, limit=False):
    im, d = canvas(0, TOTAL, "If something goes wrong")
    bullets(d, trouble, n, y=270, gap=100)
    if limit:
        d.rounded_rectangle([120, 720, 1800, 900], 18, fill=PANEL2)
        d.text((160, 745), "Known limit", font=F_BODY, fill=WARN)
        d.text((160, 810), "Xbox has no Vulkan driver yet: the game may stop when it starts its renderer.",
               font=F_SMALL, fill=TEXT)
    return im
scene("trouble", [
    ("If something goes wrong, check the logs.", trouble_card(0)),
    ("If nothing happens when you start the app, read app dot log.", trouble_card(1)),
    ("If the game stops, read boot dot log. The launcher also shows its last lines the next time it opens.", trouble_card(2)),
    ("Both files are in the app's Local State folder. Open it from Device Portal, in the file explorer, under Local App Data.", trouble_card(4)),
    ("One known limit: the Xbox has no Vulkan driver yet, so the game may stop when it starts its renderer. The help page has the details.", trouble_card(4, True)),
])

# ---- 9. outro ----
def outro_card():
    im, d = canvas(0, TOTAL, None)
    d.text((120, 360), "That's it.", font=F_TITLE, fill=TEXT)
    d.rectangle([120, 470, 420, 478], fill=ACCENT)
    d.text((120, 530), "Help page, logs and updates:", font=F_BODY, fill=DIM)
    d.text((120, 600), "github.com/NJB3300-art/tlou", font=font(48, mono=True), fill=ACCENT)
    return im
scene("outro", [
    ("That's it. For updates and help, visit the GitHub repository. Enjoy the game.", outro_card()),
])


# What the captions show, for words spelled out for the voice.
CAPTION_FIXES = [("C U S A 0 0 5 5 2 dash patch", "CUSA00552-patch"), ("C U S A 0 0 5 5 2", "CUSA00552"),
                 ("Stage T L O U Xbox dot c m d", "Stage-TLOU-Xbox.cmd"), ("u w p stage", "uwp-stage"),
                 ("U W P", "UWP"), ("P C", "PC"), ("U S B", "USB"), ("N T F S", "NTFS"), ("T L O U", "TLOU"),
                 ("dot m s i x", ".msix"), ("V C Libs x 64", "VCLibs x64"), ("app dot log", "app.log"),
                 ("boot dot log", "boot.log"), ("Local State", "LocalState"), ("Local App Data", "LocalAppData"),
                 ("My games and apps", "My games & apps")]
def caption(text):
    for spoken, shown in CAPTION_FIXES:
        text = text.replace(spoken, shown)
    return text


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        sys.exit(" ".join(map(str, cmd)) + "\n" + r.stderr[-3000:])
    return r.stdout


def duration(path):
    return float(run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", str(path)]))


def ts(t, vtt=False):
    h, rem = divmod(t, 3600)
    m, s = divmod(rem, 60)
    sep = "." if vtt else ","
    return f"{int(h):02d}:{int(m):02d}:{int(s):02d}{sep}{int(round((s % 1) * 1000)) % 1000:03d}"


def main(preview_only=False):
    cues, scene_files, audio_files = [], [], []
    clock = 0.0
    for si, (name, beats) in enumerate(SCENES):
        parts = []  # (image path, seconds)
        for bi, (sentence, image) in enumerate(beats):
            img = OUT / f"{si:02d}_{name}_{bi}.png"
            image.save(img)
            if preview_only:
                continue
            txt = OUT / f"{si:02d}_{bi}.txt"
            txt.write_text(sentence)
            wav = OUT / f"{si:02d}_{bi}.wav"
            run(["ffmpeg", "-y", "-loglevel", "error", "-f", "lavfi", "-i", f"flite=textfile={txt}:voice={VOICE}",
                 "-af", f"atempo={TEMPO},aresample=48000,volume=2.2,apad=pad_dur={GAP}", "-ac", "1", str(wav)])
            # the first beat of a scene waits for the fade-in
            lead = 0.5 if bi == 0 else 0.0
            if lead:
                lw = OUT / f"{si:02d}_{bi}_lead.wav"
                run(["ffmpeg", "-y", "-loglevel", "error", "-f", "lavfi", "-i",
                     "anullsrc=r=48000:cl=mono", "-t", str(lead), str(lw)])
                audio_files.append(lw)
            spoken = duration(wav) - GAP
            audio_files.append(wav)
            seconds = lead + duration(wav)
            cues.append((clock + lead, clock + lead + spoken, caption(sentence)))
            clock += seconds
            parts.append((img, seconds))
        if preview_only:
            continue
        # the scene: its pictures held for their sentences, faded in and out, one file
        total = sum(s for _, s in parts)
        inputs, chains = [], []
        for i, (img, seconds) in enumerate(parts):
            inputs += ["-framerate", str(FPS), "-i", str(img)]
            chains.append(f"[{i}:v]format=yuv420p,loop=-1:1,trim=duration={seconds:.3f},setpts=PTS-STARTPTS[v{i}]")
        concat = "".join(f"[v{i}]" for i in range(len(parts))) + f"concat=n={len(parts)}:v=1:a=0"
        fade = f",fade=t=in:st=0:d=0.4,fade=t=out:st={total - 0.4:.3f}:d=0.4[out]"
        tmp = OUT / f"scene_{si:02d}.tmp.mp4"
        final = OUT / f"scene_{si:02d}.mp4"
        run(["ffmpeg", "-y", "-loglevel", "error", *inputs, "-filter_complex", ";".join(chains) + ";" + concat + fade,
             "-map", "[out]", "-r", str(FPS), "-c:v", "libx264", "-crf", "20", "-preset", "veryfast",
             "-pix_fmt", "yuv420p", "-t", f"{total:.3f}", str(tmp)])
        tmp.rename(final)
        scene_files.append(final)
        print(f"scene {name}: {total:.1f}s", flush=True)
    if preview_only:
        return
    (OUT / "scenes.txt").write_text("".join(f"file '{f.name}'\n" for f in scene_files))
    (OUT / "audio.txt").write_text("".join(f"file '{f.name}'\n" for f in audio_files))
    run(["ffmpeg", "-y", "-loglevel", "error", "-f", "concat", "-safe", "0", "-i", str(OUT / "scenes.txt"),
         "-c", "copy", str(OUT / "video.mp4")])
    run(["ffmpeg", "-y", "-loglevel", "error", "-f", "concat", "-safe", "0", "-i", str(OUT / "audio.txt"),
         "-c:a", "aac", "-b:a", "128k", str(OUT / "voice.m4a")])
    srt = "".join(f"{i + 1}\n{ts(a)} --> {ts(b)}\n{t}\n\n" for i, (a, b, t) in enumerate(cues))
    vtt = "WEBVTT\n\n" + "".join(f"{ts(a, True)} --> {ts(b, True)}\n{t}\n\n" for a, b, t in cues)
    (OUT / "help.srt").write_text(srt)
    (OUT / "help.vtt").write_text(vtt)
    run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(OUT / "video.mp4"), "-i", str(OUT / "voice.m4a"),
         "-i", str(OUT / "help.srt"), "-map", "0:v", "-map", "1:a", "-map", "2:s", "-c:v", "copy", "-c:a", "copy",
         "-c:s", "mov_text", "-metadata:s:s:0", "language=eng", "-metadata:s:s:0", "title=English (CC)",
         "-disposition:s:0", "default", "-movflags", "+faststart", "-shortest", str(OUT / "help.tmp.mp4")])
    (OUT / "help.tmp.mp4").rename(OUT / "help.mp4")
    print(json.dumps({"seconds": round(clock, 1), "cues": len(cues)}))


if __name__ == "__main__":
    main(preview_only="--preview" in sys.argv)
