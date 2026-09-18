"""Render actual Receiver UI and verify USB settings/dialog controls at 160x80."""
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
out = pathlib.Path(a.output).resolve()
out.mkdir(parents=True, exist_ok=True)
env = dict(os.environ)
env['PATH'] = str(pathlib.Path(a.compiler_bin).resolve()) + os.pathsep + env['PATH']
cmake = f'''cmake_minimum_required(VERSION 3.16)
project(usb_ui_qa LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(LV_BUILD_CONF_PATH "{repo.as_posix()}/test/main_dashboard/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("{repo.as_posix()}/managed_components/lvgl__lvgl" lvgl)
file(GLOB RES "{repo.as_posix()}/src/res/font/*.c" "{repo.as_posix()}/src/res/img/*.c")
add_executable(usb_ui "{here.as_posix()}/ui.cpp" "{repo.as_posix()}/src/ui/ui.cpp"
  "{repo.as_posix()}/src/ui/ui_main.cpp" "{repo.as_posix()}/src/ui/ui_bt.cpp"
  "{repo.as_posix()}/src/ui/ui_loading.cpp" ${{RES}})
target_include_directories(usb_ui PRIVATE "{here.as_posix()}/fakes" "{repo.as_posix()}/test/main_dashboard/fakes" "{repo.as_posix()}/include")
target_compile_options(usb_ui PRIVATE -Wall -Wextra)
target_link_libraries(usb_ui PRIVATE lvgl)
'''
(out / 'CMakeLists.txt').write_text(cmake)
subprocess.run(['cmake', '-S', str(out), '-B', str(out / 'build'), '-G', 'MinGW Makefiles'], env=env, check=True)
subprocess.run(['cmake', '--build', str(out / 'build'), '-j', '8'], env=env, check=True)
captures = out / 'captures'
captures.mkdir(exist_ok=True)
subprocess.run([str(out / 'build/usb_ui.exe')], cwd=captures, env=env, check=True)
print(f'Screenshots: {captures}')
