"""Compile and exercise the production button state machine on the host."""

import argparse
import os
import pathlib
import subprocess

p = argparse.ArgumentParser()
p.add_argument("--compiler-bin", required=True)
p.add_argument("--output", required=True)
a = p.parse_args()

here = pathlib.Path(__file__).resolve().parent
repo = here.parents[1]
output = pathlib.Path(a.output).resolve()
output.mkdir(parents=True, exist_ok=True)
executable = output / "button_qa.exe"
compiler = pathlib.Path(a.compiler_bin).resolve() / "g++.exe"

env = dict(os.environ)
env["PATH"] = str(pathlib.Path(a.compiler_bin).resolve()) + os.pathsep + env["PATH"]
subprocess.run(
    [
        str(compiler),
        "-std=c++17",
        "-Wall",
        "-Wextra",
        "-I",
        str(here / "fakes"),
        "-I",
        str(repo / "include"),
        str(here / "button.cpp"),
        "-o",
        str(executable),
    ],
    env=env,
    check=True,
)
subprocess.run([str(executable)], env=env, check=True)
