Import("env")

import os, glob, re

libdeps = env.get("PROJECT_LIBDEPS_DIR")
pioenv = env.get("PIOENV")
meshcore_dir = os.path.join(libdeps, pioenv, "MeshCore")

variant = None
for flag in env.get("BUILD_FLAGS", []):
    m = re.match(r'-D\s*MC_VARIANT=(\S+)', flag)
    if m:
        variant = m.group(1)
        break

if not variant:
    print("***** MC_VARIANT not found in BUILD_FLAGS *****")

paths = [
    os.path.join(meshcore_dir, "src"),
    os.path.join(meshcore_dir, "lib", "ed25519"),
    os.path.join(meshcore_dir, "examples", "simple_repeater"),
]

if variant:
    paths.insert(0, os.path.join(meshcore_dir, "variants", variant))

for p in paths:
    env.Append(CPPPATH=[p])

ed25519_src = os.path.join(meshcore_dir, "lib", "ed25519")
for src in glob.glob(os.path.join(ed25519_src, "*.c")):
    env.AppendUnique(PIOBUILDFILES=[env.Object(src)])
