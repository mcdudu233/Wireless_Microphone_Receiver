"""Compile production recording/storage against strict hardware fakes and inspect WAVs."""
import argparse
import os
from pathlib import Path
import struct
import subprocess
import wave

p = argparse.ArgumentParser()
p.add_argument('--compiler-bin', required=True)
p.add_argument('--output', required=True)
a = p.parse_args()
here = Path(__file__).resolve().parent
repo = here.parents[1]
out = Path(a.output).resolve()
out.mkdir(parents=True, exist_ok=True)
env = dict(os.environ)
env['PATH'] = str(Path(a.compiler_bin).resolve()) + os.pathsep + env['PATH']
subprocess.run([str(Path(a.compiler_bin) / 'g++.exe'), '-std=c++17', '-Wall', '-Wextra',
                '-I'+str(here / 'fakes'), '-I'+str(here.parent / 'usb_audio/fakes'),
                '-I'+str(repo / 'include'), str(here / 'recording.cpp'), '-o', str(out / 'recording.exe')], env=env, check=True)
subprocess.run([str(out / 'recording.exe')], cwd=out, env=env, check=True)
for rate in (48000, 96000, 192000):
    for bit in (16, 24, 32):
        for channels in (1, 2):
            path = out / f'{rate}_{bit}_{channels}.wav'
            with wave.open(str(path), 'rb') as wav:
                assert (wav.getframerate(), wav.getsampwidth(), wav.getnchannels(), wav.getnframes()) == (rate, bit//8, channels, rate*36//1000)
            raw = path.read_bytes()
            assert struct.unpack_from('<I', raw, 4)[0] == len(raw)-8
            cursor = 12
            tags = {}
            while cursor < len(raw):
                tag, size = struct.unpack_from('<4sI', raw, cursor)
                if tag == b'LIST':
                    assert raw[cursor+8:cursor+12] == b'INFO'
                    item = cursor+12
                    while item < cursor+8+size:
                        key, length = struct.unpack_from('<4sI', raw, item)
                        tags[key] = raw[item+8:item+8+length].rstrip(b'\0')
                        item += 8+length+(length & 1)
                cursor += 8+size+(size & 1)
            assert tags[b'INAM'] == b'Wireless Microphone'
            assert tags[b'IART'] == b'dudu233, tiosa'
            assert tags[b'ISFT'] == b'Wireless Microphone Receiver'
print('PASS: Python wave independently decoded all 18 formats; RIFF sizes and LIST/INFO metadata verified')
