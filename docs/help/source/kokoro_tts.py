"""Kokoro-82M (ONNX) text to speech, phonemized with espeak-ng: one sentence -> 24 kHz WAV.

  python kokoro_tts.py <voice> <speed> <out.wav> "text"
Voices: af_heart, af_bella, am_michael, ... (voices-v1.0.bin)."""
import json
import os
import re
import subprocess
import sys
import wave
from pathlib import Path

import numpy as np
import onnxruntime as ort

HERE = Path(__file__).resolve().parent
VOCAB = json.load(open(HERE / "kokoro-onnx/src/kokoro_onnx/config.json"))["vocab"]
PUNCT = re.compile(r"([,.!?;:—…]+)")
_session = None
_voices = None


def espeak(text):
    env = dict(os.environ, LD_LIBRARY_PATH=str(HERE / "piper"))
    out = subprocess.run([str(HERE / "piper/espeak-ng"), f"--path={HERE / 'piper'}", "-q", "--ipa", "-v", "en-us", text],
                         capture_output=True, text=True, env=env, check=True).stdout
    return " ".join(out.split())


def phonemize(text):
    """espeak-ng IPA with stress, punctuation kept where it was (as phonemizer does for Kokoro)."""
    parts = PUNCT.split(text.strip())
    out = []
    for i, part in enumerate(parts):
        if i % 2:  # punctuation
            out.append(part.replace("—", ",").replace("…", "...") + " ")
        elif part.strip():
            out.append(espeak(part.strip()))
    phonemes = "".join(out).strip()
    return "".join(p for p in phonemes if p in VOCAB)


def synthesize(text, voice="af_heart", speed=1.0):
    global _session, _voices
    if _session is None:
        options = ort.SessionOptions()
        options.intra_op_num_threads = os.cpu_count() or 2
        _session = ort.InferenceSession(str(HERE / "kokoro-v1.0.onnx"), options, providers=["CPUExecutionProvider"])
        _voices = np.load(HERE / "voices-v1.0.bin")
    phonemes = phonemize(text)
    tokens = [VOCAB[p] for p in phonemes][:510]
    style = _voices[voice][min(len(tokens), 510) - 1]
    names = {i.name: i.type for i in _session.get_inputs()}
    key = "input_ids" if "input_ids" in names else "tokens"
    speed_type = np.int32 if "int" in names.get("speed", "float") else np.float32
    audio = _session.run(None, {key: np.array([[0, *tokens, 0]], dtype=np.int64),
                                "style": np.asarray(style, dtype=np.float32),
                                "speed": np.array([speed], dtype=speed_type)})[0]
    return np.asarray(audio, dtype=np.float32).reshape(-1)


def write_wav(path, audio, rate=24000):
    pcm = np.clip(audio, -1, 1)
    pcm = (pcm * 32767).astype(np.int16)
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(pcm.tobytes())


if __name__ == "__main__":
    voice, speed, out, text = sys.argv[1], float(sys.argv[2]), sys.argv[3], sys.argv[4]
    write_wav(out, synthesize(text, voice, speed))
