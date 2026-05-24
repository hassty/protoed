build-dir := "build"
exe := "protoed"
compiler := "clang" # or gcc

alias b := build
alias r := run
alias d := debug
alias c := clean

setup:
    C={{ compiler }} CXX={{ compiler }}++ meson setup {{ build-dir }}
    mkdir -p {{ build-dir }}/generated

build:
    meson compile -C {{ build-dir }}

debug:
    gdb {{ build-dir }}/{{ exe }}

[default]
run: build
    {{ build-dir }}/{{ exe }}

clean:
    rm -rf {{ build-dir }}
