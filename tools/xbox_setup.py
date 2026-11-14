"""One-click setup for the UWP app (Xbox Developer Mode): find the game, prepare it, deliver it.

1. Finds your PS4 dump of The Last of Us Remastered on this PC: the base game (CUSA00552) and its
   update 1.11, by reading each folder's sce_sys/param.sfo (any folder name, any drive).
2. Prepares it for the console: tools/probe_tlou.py --stage (the same preparation the PC version
   does at every start).
3. Delivers game, update and stage folder to the console, either
   - over Wi-Fi to the app (Receive files over Wi-Fi in its launcher: address and code on the TV), or
   - onto a USB drive, as <drive>:\\TLOU\\CUSA00552, CUSA00552-patch and stage.
   Both continue where an earlier, interrupted run stopped.

  python tools/xbox_setup.py                         asks what to do
  python tools/xbox_setup.py --wifi 192.168.1.20 --code 123456
  python tools/xbox_setup.py --usb E
  python tools/xbox_setup.py --game D:\\PS4\\CUSA00552 --update D:\\PS4\\CUSA00552-patch

Settings that patch the game are chosen at step 2, as environment variables (TLOU_RESOLUTION,
TLOU_AIM_ASSIST, TLOU_AMBIENT_SHADOWS, ...). Only standard-library Python is used."""
import argparse
import http.client
import json
import os
import shutil
import string
import struct
import subprocess
import sys
import time
import urllib.parse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TITLE = "CUSA00552"
UPDATE_VERSION = "01.11"
PORT = 8077
CHUNK = 32 * 1024 * 1024
SKIP_DIRS = {"windows", "program files", "program files (x86)", "programdata", "$recycle.bin",
             "system volume information", "appdata", "node_modules", ".git"}


# ---- finding the game ----

def read_sfo(path):
    """TITLE_ID, APP_VER, CATEGORY ... of a param.sfo, or {}."""
    try:
        data = path.read_bytes()
    except OSError:
        return {}
    if len(data) < 20 or data[:4] != b"\0PSF":
        return {}
    _, keys, values, count = struct.unpack_from("<IIII", data, 4)
    out = {}
    for i in range(count):
        if 20 + i * 16 + 16 > len(data):
            break
        key_off, fmt, length, _, value_off = struct.unpack_from("<HHIII", data, 20 + i * 16)
        key = data[keys + key_off:data.index(b"\0", keys + key_off)].decode("ascii", "replace")
        raw = data[values + value_off:values + value_off + length]
        out[key] = raw.split(b"\0")[0].decode("utf-8", "replace") if fmt == 0x0204 else raw
    return out


def describe(folder):
    sfo = read_sfo(folder / "sce_sys" / "param.sfo")
    if sfo.get("TITLE_ID") != TITLE or not (folder / "eboot.bin").is_file():
        return None
    kind = "update" if sfo.get("CATEGORY") == "gp" else "game"
    return kind, sfo.get("APP_VER", "?")


def search_roots():
    roots = [ROOT, ROOT.parent, ROOT.parent.parent, Path.home() / "Downloads", Path.home() / "Desktop"]
    if os.name == "nt":
        roots += [Path(f"{d}:\\") for d in string.ascii_uppercase if Path(f"{d}:\\").exists()]
    seen, unique = set(), []
    for r in roots:
        key = str(r).lower()
        if key not in seen and r.exists():
            seen.add(key)
            unique.append(r)
    return unique


def find_folders(depth=3):
    """{"game": [(path, version)], "update": [...]} from a bounded walk of the usual places."""
    found = {"game": [], "update": []}
    visited = set()

    def walk(folder, level):
        key = str(folder).lower()
        if key in visited:
            return
        visited.add(key)
        hit = describe(folder)
        if hit:
            found[hit[0]].append((folder, hit[1]))
            return
        if level >= depth:
            return
        try:
            children = [c for c in folder.iterdir() if c.is_dir() and c.name.lower() not in SKIP_DIRS
                        and not c.name.startswith(".")]
        except OSError:
            return
        for child in children[:400]:
            walk(child, level + 1)

    for root in search_roots():
        walk(root, 0)
    return found


def choose(label, options):
    if len(options) == 1:
        return options[0]
    print(f"\nMore than one {label} found:")
    for i, (path, version) in enumerate(options, 1):
        print(f"  {i}) {path}  (version {version})")
    while True:
        answer = input(f"Which {label}? [1-{len(options)}] ").strip()
        if answer.isdigit() and 1 <= int(answer) <= len(options):
            return options[int(answer) - 1]


def link_update(game, update):
    """probe_tlou.py finds the update next to the game as <game>-patch: a junction if needed."""
    expected = game.parent / (game.name + "-patch")
    if expected.exists():
        if expected.resolve() == update.resolve():
            return
        hit = describe(expected)
        if hit and hit[0] == "update" and hit[1] == UPDATE_VERSION:
            return
        sys.exit(f"{expected} exists but is not update {UPDATE_VERSION}; move it away and run again.")
    print(f"Linking the update next to the game: {expected} -> {update}")
    result = subprocess.run(["cmd", "/c", "mklink", "/J", str(expected), str(update)], capture_output=True, text=True)
    if result.returncode:
        sys.exit("Could not link the update next to the game:\n" + result.stdout + result.stderr +
                 f"\nRename or move the update folder to {expected} and run again.")


# ---- delivering ----

def files_of(game, update, stage):
    jobs = []
    for source, target in ((game, TITLE), (update, TITLE + "-patch"), (stage, "stage")):
        for path in sorted(p for p in source.rglob("*") if p.is_file()):
            jobs.append((path, target + "/" + path.relative_to(source).as_posix()))
    return jobs


def progress(done, total, started, name):
    rate = done / max(time.time() - started, 0.001)
    left = (total - done) / rate if rate > 0 else 0
    line = f"\r{done / 2**30:7.2f} / {total / 2**30:.2f} GiB  {rate / 2**20:6.1f} MiB/s  {left / 60:5.1f} min left  {name[-40:]:40}"
    print(line, end="", flush=True)


class Console:
    def __init__(self, address, code):
        address = address.strip()
        if "://" not in address:
            address = "http://" + address
        parsed = urllib.parse.urlparse(address)
        self.host, self.port, self.code = parsed.hostname, parsed.port or PORT, code.strip()

    def call(self, method, name, params=None, body=None):
        query = urllib.parse.urlencode({"code": self.code, **(params or {})})
        for attempt in range(4):
            try:
                conn = http.client.HTTPConnection(self.host, self.port, timeout=120)
                headers = {"Content-Length": str(len(body))} if body is not None else {}
                conn.request(method, f"/api/{name}?{query}", body=body, headers=headers)
                response = conn.getresponse()
                text = response.read().decode("utf-8", "replace")
                conn.close()
                if response.status == 403:
                    sys.exit("\nThe console refused the code: use the six digits shown on the TV.")
                if response.status != 200:
                    raise OSError(f"{response.status}: {text}")
                return json.loads(text) if text else {}
            except (OSError, http.client.HTTPException) as error:
                if attempt == 3:
                    raise
                print(f"\n  retrying ({error})", flush=True)
                time.sleep(2 * (attempt + 1))


def send_wifi(jobs, console):
    status = console.call("GET", "status")
    total = sum(p.stat().st_size for p, _ in jobs)
    print(f"Console reached. Free space there: {status.get('free', -1) / 2**30:.1f} GiB, to send: {total / 2**30:.1f} GiB")
    if 0 <= status.get("free", -1) < total:
        print("Warning: that may not fit.")
    done, started = 0, time.time()
    for path, name in jobs:
        size = path.stat().st_size
        have = console.call("GET", "info", {"path": name}).get("size", -1)
        if have == size:
            done += size
            continue
        offset = have if 0 < have < size else 0
        done += offset
        with path.open("rb") as f:
            f.seek(offset)
            while offset < size or (size == 0 and offset == 0):
                piece = f.read(CHUNK)
                console.call("PUT", "file", {"path": name, "offset": offset, "total": size}, piece)
                offset += len(piece)
                done += len(piece)
                progress(done, total, started, name)
                if size == 0:
                    break
    console.call("POST", "done")
    print("\nAll files are on the console.")


def removable_drives():
    if os.name != "nt":
        return []
    import ctypes
    drives = []
    system = os.environ.get("SystemDrive", "C:").upper()[0]
    for letter in string.ascii_uppercase:
        root = f"{letter}:\\"
        kind = ctypes.windll.kernel32.GetDriveTypeW(root)
        if kind in (2, 3) and letter != system and Path(root).exists():  # removable, or fixed (USB disks)
            try:
                free = shutil.disk_usage(root).free
            except OSError:
                continue
            drives.append((letter, "removable" if kind == 2 else "fixed", free))
    return drives


def copy_usb(jobs, drive):
    target = Path(f"{drive}:\\") / "TLOU"
    total = sum(p.stat().st_size for p, _ in jobs)
    free = shutil.disk_usage(f"{drive}:\\").free
    print(f"Copying to {target} ({total / 2**30:.1f} GiB, {free / 2**30:.1f} GiB free)")
    if free < total:
        print("Warning: that may not fit.")
    done, started = 0, time.time()
    for path, name in jobs:
        destination = target / Path(name)
        size = path.stat().st_size
        if destination.is_file() and destination.stat().st_size == size:
            done += size
            continue
        destination.parent.mkdir(parents=True, exist_ok=True)
        with path.open("rb") as src, destination.open("wb") as dst:
            while True:
                piece = src.read(CHUNK)
                if not piece:
                    break
                dst.write(piece)
                done += len(piece)
                progress(done, total, started, name)
    print(f"\nDone. Plug the drive into the Xbox; the app finds {target}.")


# ---- main ----

def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--game", type=Path, help="The base game folder (CUSA00552)")
    parser.add_argument("--update", type=Path, help="The update 1.11 folder")
    parser.add_argument("--wifi", metavar="ADDRESS", help="Send to the Xbox app at this address")
    parser.add_argument("--code", help="The code shown on the TV (with --wifi)")
    parser.add_argument("--usb", metavar="DRIVE", help="Copy to this drive letter")
    parser.add_argument("--prepare-only", action="store_true", help="Stop after the preparation")
    parser.add_argument("--skip-prepare", action="store_true", help="Use the existing uwp-stage folder")
    args = parser.parse_args()

    print("The Last of Us Remastered - setup for the Xbox app\n")
    if args.game and args.update:
        game, update = args.game.resolve(), args.update.resolve()
    else:
        print("Looking for your PS4 dump (CUSA00552 and update 1.11) ...")
        found = find_folders()
        if not found["game"]:
            sys.exit("No CUSA00552 game folder found (it needs sce_sys/param.sfo and eboot.bin).\n"
                     "Pass it with --game <folder>. The Steam game (The Last of Us Part I) cannot be used.")
        updates = [u for u in found["update"] if u[1] == UPDATE_VERSION]
        if not updates:
            others = ", ".join(f"{p} ({v})" for p, v in found["update"]) or "none"
            sys.exit(f"No update {UPDATE_VERSION} found (updates seen: {others}). Pass it with --update <folder>.")
        game = args.game.resolve() if args.game else choose("game folder", found["game"])[0]
        update = args.update.resolve() if args.update else choose("update", updates)[0]
    for folder, kind in ((game, "game"), (update, "update")):
        hit = describe(folder)
        if not hit or hit[0] != kind:
            sys.exit(f"{folder} is not the CUSA00552 {kind}.")
    if describe(update)[1] != UPDATE_VERSION:
        sys.exit(f"{update} is version {describe(update)[1]}; this build needs {UPDATE_VERSION}.")
    print(f"Game:   {game}  (version {describe(game)[1]})")
    print(f"Update: {update}  (version {UPDATE_VERSION})")

    stage = ROOT / "uwp-stage"
    if not args.skip_prepare:
        link_update(game, update)
        print("\nPreparing the game for the console (a few minutes the first time) ...")
        result = subprocess.run([sys.executable, str(ROOT / "tools" / "probe_tlou.py"), "--game", str(game),
                                 "--stage", str(stage)], cwd=ROOT)
        if result.returncode:
            sys.exit("The preparation failed; the messages above say why.")
    if not (stage / "boot.json").is_file():
        sys.exit(f"{stage}\\boot.json is missing: run without --skip-prepare.")
    if args.prepare_only:
        print(f"\nPrepared: {stage}")
        return

    jobs = files_of(game, update, stage)
    if not args.wifi and not args.usb:
        print("\nHow should the files get to the Xbox?")
        print("  1) Over Wi-Fi (in the app: Receive files over Wi-Fi)")
        print("  2) Copy to a USB drive")
        answer = input("[1/2] ").strip()
        if answer == "1":
            args.wifi = input("Address shown on the TV (for example 192.168.1.20): ").strip()
        else:
            drives = removable_drives()
            if not drives:
                sys.exit("No drive other than the system drive found. Plug in the USB drive and run again.")
            for letter, kind, free in drives:
                print(f"  {letter}:  {kind}, {free / 2**30:.1f} GiB free")
            args.usb = input("Drive letter: ").strip().rstrip(":\\").upper()
    if args.wifi:
        code = args.code or input("Code shown on the TV: ").strip()
        send_wifi(jobs, Console(args.wifi, code))
    else:
        copy_usb(jobs, args.usb.rstrip(":\\").upper())
    print("On the Xbox: start the app; the launcher checks the files. Then choose Play.")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nStopped. Run again to continue where it stopped.")
        sys.exit(1)
