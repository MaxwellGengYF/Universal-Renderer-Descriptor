@echo off
xmake f -p windows -a x64 -m debug --cc=clang-cl --cxx=clang-cl -c
xmake -w
pause