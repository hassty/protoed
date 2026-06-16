set windows-shell := ["cmd.exe", "/C"]

t := "d"
build-type := if t == "d" { "debug" } else if t == "r" { "release" } else { t }
build-dir := "build_" + build-type
exe := "protoed"
copy := if os_family() == "windows" { "copy" } else { "cp" }

alias s := setup
alias b := build
alias r := run
alias d := debug
alias c := clean

default:
    @just --list

setup:
    {{ if os_family() == "windows" { \
        "if not exist " + build-dir + " (" \
    } else { \
        "if [ ! -d " + build-dir + " ]; then " \
    } }} \
    meson setup {{ build-dir }} \
        {{ if build-type == "release" { '-Doptimization=s -Ddebug=false -Db_ndebug=true -Db_lto=true -Db_lundef=true -Dstrip=true -Dcpp_args="-ffunction-sections -fdata-sections -fno-rtti" -Dcpp_link_args="-Wl,--gc-sections -Wl,--strip-all -s"' } else { "--buildtype=" + build-type } }} \
        --force-fallback-for=abseil-cpp,protobuf \
        -Ddefault_library=static \
    {{ if os_family() == "windows" { ")" } else { "; fi" } }}

build: setup && compile_commands
    meson compile -C {{ build-dir }}

compile_commands:
    {{ copy }} {{ build-dir }}{{ PATH_SEP }}compile_commands.json .

[unix]
debug:
    gdb {{ build-dir }}/{{ exe }}

[windows]
debug:
    msg %USERNAME% "Babe! It's 4pm, time for your Visual Studio launching"

run *args: build
    {{ build-dir }}{{ PATH_SEP }}{{ exe }} {{ args }}


[unix]
clean:
    rm -fr {{ build-dir }}

[windows]
clean:
    if exist {{ build-dir }} rmdir /s /q {{ build-dir }}
