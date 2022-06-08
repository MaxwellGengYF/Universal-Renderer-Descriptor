@echo off
xmake project -k compile_commands
move compile_commands.json .vscode/compile_commands.json