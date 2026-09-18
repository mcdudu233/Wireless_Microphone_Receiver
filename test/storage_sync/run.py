"""Compile actual Preferences, TF sync, and logging against strict hardware fakes."""
import argparse, os, pathlib, subprocess
p=argparse.ArgumentParser()
p.add_argument('--compiler-bin',required=True)
p.add_argument('--output',required=True)
a=p.parse_args()
here=pathlib.Path(__file__).resolve().parent
repo=here.parents[1]
out=pathlib.Path(a.output).resolve();out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ);env['PATH']=a.compiler_bin+os.pathsep+env['PATH']
includes=[here/'fakes',here.parent/'recording/fakes',here.parent/'usb_audio/fakes',repo/'include']
subprocess.run([str(pathlib.Path(a.compiler_bin)/'g++.exe'),'-std=c++17','-Wall','-Wextra',
 *['-I'+str(i) for i in includes],str(here/'sync.cpp'),'-o',str(out/'sync.exe')],env=env,check=True)
subprocess.run([str(out/'sync.exe')],cwd=out,env=env,check=True)
