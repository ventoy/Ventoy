#!/bin/sh
# Compatibility wrapper. All architectures go through build/build-user.sh.
exec "$(CDPATH= cd -- "$(dirname "$0")" && pwd)/build-user.sh" loongarch64
