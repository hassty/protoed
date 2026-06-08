t := "d"
build-type := if t == "d" { "debug" } else if t == "r" { "release" } else { t }
build-dir := "build_" + build-type
exe := "protoed"

alias s := setup
alias b := build
alias r := run
alias d := debug
alias c := clean

setup:
    C=clang CXX=clang++ meson setup {{ build-dir }} \
        --buildtype={{ build-type }} \
        --force-fallback-for=abseil-cpp,protobuf \
        -Ddefault_library=static
    mkdir -p {{ build-dir }}/generated

build:
    meson compile -C {{ build-dir }}
    cp {{ build-dir }}/compile_commands.json .

debug:
    gdb {{ build-dir }}/{{ exe }}

run: build
    {{ build-dir }}/{{ exe }}

clean:
    rm -rf {{ build-dir }}
