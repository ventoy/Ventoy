#!/bin/sh
# The release is no longer built in one container.
# Debian trixie builds GRUB, EDK2, and the loongarch64 tools.
# CentOS 7 builds the other installers and packs the release.
echo "Run the two CI containers in order:" >&2
echo "  docker compose run -T --rm --build trixie" >&2
echo "  docker compose run -T --rm --build centos" >&2
exit 1
