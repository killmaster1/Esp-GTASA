#!/usr/bin/env bash
set -eu
mod_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
ndk_dir="${1:-${ANDROID_NDK_HOME:-}}"
output_dir="${2:-$mod_dir/build-local}"
if [ -z "$ndk_dir" ]; then
    echo 'Uso: bash sampesp/build_local.sh /caminho/android-ndk-r27c [pasta-saida]' >&2
    exit 1
fi
compiler="$ndk_dir/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android23-clang++"
if [ ! -x "$compiler" ]; then
    echo "Compilador Android ARM64 nao encontrado: $compiler" >&2
    exit 1
fi
mkdir -p "$output_dir"
"$compiler" -std=c++17 -O3 -DNDEBUG -fPIC -fno-omit-frame-pointer \
    -fstack-protector-strong -ffunction-sections -fdata-sections \
    -Wall -Wextra -Wno-unused-parameter -shared -static-libstdc++ \
    -I"$mod_dir/.." "$mod_dir/main.cpp" \
    -Wl,-soname,libsampesp.so -Wl,--build-id=sha1 -Wl,--gc-sections -Wl,--no-undefined \
    -llog -ldl -o "$output_dir/libsampesp.so"
"$ndk_dir/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip" --strip-debug "$output_dir/libsampesp.so"
echo "Biblioteca gerada: $output_dir/libsampesp.so"
