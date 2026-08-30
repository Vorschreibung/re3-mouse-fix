#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$project_dir"
mkdir -p build
cd build

lmsvc --2022 --arch x64 cl \
  /nologo \
  /std:c++20 \
  /Zc:preprocessor \
  /EHsc \
  /O2 \
  /GL \
  /Brepro \
  /MD \
  /W4 \
  /external:W0 \
  /permissive- \
  /LD \
  /DNDEBUG \
  /DWIN32 \
  /D_WINDOWS \
  /external:I../third_party/reframework \
  /Fo:plugin.obj \
  ../src/plugin.cpp \
  /link \
  /LTCG \
  /OPT:REF \
  /OPT:ICF \
  /Brepro \
  /NOIMPLIB \
  /OUT:RE2MouseFix.dll \
  /PDB:RE2MouseFix.pdb
