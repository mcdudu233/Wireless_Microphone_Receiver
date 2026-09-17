"""Build the actual dashboard against desktop LVGL and run geometry/state QA.
Usage: python run.py --compiler-bin <MinGW bin> --output <scratch directory>
Requires CMake and the Receiver managed LVGL sources from a PlatformIO build.
"""
import argparse, os, pathlib, subprocess
p=argparse.ArgumentParser()
p.add_argument('--compiler-bin', required=True)
p.add_argument('--output', required=True)
a=p.parse_args()
here=pathlib.Path(__file__).resolve().parent
repo=here.parents[1]
out=pathlib.Path(a.output).resolve();out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ);env['PATH']=str(pathlib.Path(a.compiler_bin).resolve())+os.pathsep+env['PATH']
cmake=f'''cmake_minimum_required(VERSION 3.16)
project(main_dashboard_qa LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(LV_BUILD_CONF_PATH "{here.as_posix()}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("{repo.as_posix()}/managed_components/lvgl__lvgl" lvgl)
file(GLOB RES "{repo.as_posix()}/src/res/font/*.c" "{repo.as_posix()}/src/res/img/*.c")
add_executable(dashboard "{here.as_posix()}/dashboard.cpp" "{repo.as_posix()}/src/ui/ui.cpp" ${{RES}})
target_include_directories(dashboard PRIVATE "{here.as_posix()}/fakes" "{repo.as_posix()}/include")
target_compile_options(dashboard PRIVATE -Wall -Wextra)
target_link_libraries(dashboard PRIVATE lvgl)
'''
(out/'CMakeLists.txt').write_text(cmake)
subprocess.run(['cmake','-S',str(out),'-B',str(out/'build'),'-G','MinGW Makefiles'],env=env,check=True)
subprocess.run(['cmake','--build',str(out/'build'),'-j','8'],env=env,check=True)
subprocess.run([str(out/'build/dashboard.exe')],cwd=out,env=env,check=True)
