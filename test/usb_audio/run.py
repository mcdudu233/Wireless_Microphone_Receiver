"""Exercise production USB code with the actual TinyUSB device/class stack.
Uses a fake physical controller, scheduler and TF media.
"""
import argparse
import os
import pathlib
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--compiler-bin', required=True)
p.add_argument('--output', required=True)
a = p.parse_args()
here = pathlib.Path(__file__).resolve().parent
repo = here.parents[1]
tiny = repo / 'managed_components/espressif__tinyusb/src'
out = pathlib.Path(a.output).resolve()
out.mkdir(parents=True, exist_ok=True)
env = dict(os.environ)
env['PATH'] = str(pathlib.Path(a.compiler_bin).resolve()) + os.pathsep + env['PATH']
sources = [tiny / name for name in ['tusb.c', 'device/usbd.c', 'common/tusb_fifo.c',
           'class/audio/audio_device.c', 'class/msc/msc_device.c']]
sources += [repo / 'src/module/usb/usb_descriptors.cpp', repo / 'src/module/usb/usb_device_uac.cpp',
            repo / 'src/module/usb/usb_device_msc.cpp',
            repo / 'src/module/audio/buffer.cpp', here / 'usb_audio.cpp']
cmake = f'''cmake_minimum_required(VERSION 3.16)
project(usb_audio_qa LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
add_executable(usb_audio {' '.join('"' + s.as_posix() + '"' for s in sources)})
target_include_directories(usb_audio PRIVATE "{here.as_posix()}/fakes" "{repo.as_posix()}/include" "{tiny.as_posix()}")
target_compile_options(usb_audio PRIVATE -Wall -Wextra)
'''
(out / 'CMakeLists.txt').write_text(cmake)
subprocess.run(['cmake', '-S', str(out), '-B', str(out / 'build'), '-G', 'MinGW Makefiles'], env=env, check=True)
subprocess.run(['cmake', '--build', str(out / 'build'), '-j', '8'], env=env, check=True)
subprocess.run([str(out / 'build/usb_audio.exe')], env=env, check=True)
