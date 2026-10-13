#!/bin/sh
# Download toolchain/libc tarballs into ci/dl and verify SHA-256.
# Usage: fetch.sh [name...]   (default: all four). Skips anything already
# present with the right hash.
set -eu
CI=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DL=$CI/dl
mkdir -p "$DL"
[ $# -gt 0 ] || set -- dietlibc-0.34.tar.xz musl-1.2.1.tar.gz \
    aarch64--uclibc--stable-2020.08-1.tar.bz2 mips64el-linux-musl-gcc730.tar.bz2

wanted() { case " $WANT " in *" $1 "*) return 0 ;; esac; return 1; }
WANT=$*
SEEN=""

get() {
    name=$1; shift
    wanted "$name" || return 0
    SEEN="$SEEN $name"
    if [ -f "$DL/$name" ] && (cd "$DL" && grep " $name\$" "$CI/SHA256SUMS" | sha256sum -c --quiet - 2>/dev/null); then
        echo "fetch: $name ok (cached)"
        return 0
    fi
    for url in "$@"; do
        echo "fetch: $name <- $url"
        if curl -fL --retry 3 --connect-timeout 20 --speed-limit 10240 --speed-time 60 --max-time 1800 -o "$DL/$name.part" "$url"; then
            mv "$DL/$name.part" "$DL/$name"
            if (cd "$DL" && grep " $name\$" "$CI/SHA256SUMS" | sha256sum -c --quiet -); then
                return 0
            fi
            echo "fetch: $name hash mismatch from $url" >&2
            rm -f "$DL/$name"
        fi
        rm -f "$DL/$name.part"
    done
    echo "fetch: could not get $name" >&2
    return 1
}

# fefe.de no longer serves the 0.34 release. Software Heritage keeps it
# addressed by the same SHA-256 pinned in SHA256SUMS; Fedora's source cache
# keeps it by SHA-512; the Wayback Machine has the original download.
get dietlibc-0.34.tar.xz \
    'https://www.fefe.de/dietlibc/dietlibc-0.34.tar.xz' \
    'https://archive.softwareheritage.org/api/1/content/sha256:7994ad5a63d00446da2e95da1f3f03355b272f096d7eb9830417ab14393b3ace/raw/' \
    'https://src.fedoraproject.org/repo/pkgs/dietlibc/dietlibc-0.34.tar.xz/sha512/2b38528c0ccf50e426f587b6448fed997fab1147eecc9e1af2f3fb3efe3d8f3997656d8e66e7cf1045ceb1f602cef43456c62ba83ff494f9c9816721bdb4d6c6/dietlibc-0.34.tar.xz' \
    'https://web.archive.org/web/20160314123456id_/https://www.fefe.de/dietlibc/dietlibc-0.34.tar.xz'
get musl-1.2.1.tar.gz \
    'https://musl.libc.org/releases/musl-1.2.1.tar.gz'
get aarch64--uclibc--stable-2020.08-1.tar.bz2 \
    'https://toolchains.bootlin.com/downloads/releases/toolchains/aarch64/tarballs/aarch64--uclibc--stable-2020.08-1.tar.bz2'
get mips64el-linux-musl-gcc730.tar.bz2 \
    'https://github.com/ventoy/musl-cross-make/releases/download/latest/output.tar.bz2'

for w in $WANT; do
    case " $SEEN " in *" $w "*) ;; *) echo "fetch: unknown tarball name: $w" >&2; exit 1 ;; esac
done
echo "fetch: verified: $WANT"
