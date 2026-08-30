@echo off
setlocal
cd /d "%~dp0"
if not exist build mkdir build
pushd build

cl /nologo /std:c++20 /Zc:preprocessor /EHsc /O2 /GL /Brepro /MD /W4 ^
  /external:W0 /permissive- /LD /DNDEBUG /DWIN32 /D_WINDOWS ^
  /external:I..\third_party\reframework ^
  /Fo:plugin.obj ..\src\plugin.cpp ^
  /link /LTCG /OPT:REF /OPT:ICF /Brepro /NOIMPLIB ^
  /OUT:RE2MouseFix.dll /PDB:RE2MouseFix.pdb

set build_result=%errorlevel%
popd
exit /b %build_result%
