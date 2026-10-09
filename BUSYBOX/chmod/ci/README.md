# Reproducible build for `vtchmod*`

`vtchmod.c` is compiled five times by `../build.sh`, once per CPU Ventoy
boots on, and the results are copied into the three `IMG/cpio_*` trees.
The scripts here run that same `build.sh`, unmodified, inside a container
that has the toolchains `DOC/BuildVentoyFromSource.txt` lists, then `cmp`
all ten output files against the ones in git. Every one is byte-identical.

| binary          | toolchain                                                        | smoke test in CI |
|-----------------|------------------------------------------------------------------|------------------|
| vtchmod32       | CentOS 7.5.1804, gcc 4.8.5-28, dietlibc 0.34 (fefe tarball)      | native           |
| vtchmod64       | same                                                             | native           |
| vtchmod64_musl  | same container, musl 1.2.1                                       | native           |
| vtchmodaa64     | Bootlin `aarch64--uclibc--stable-2020.08-1` (gcc 9.3.0)          | qemu-user-static |
| vtchmodm64e     | `ventoy/musl-cross-make` release `output.tar.bz2` (gcc 7.3.0)    | qemu-user-static |

## What is pinned, and why

- **CentOS 7.5.1804, gcc 4.8.5-28.** `DOC/BuildVentoyFromSource.txt` says
  CentOS 7.8, but the `.comment` section of the committed x86 binaries
  records `GCC: (GNU) 4.8.5 20150623 (Red Hat 4.8.5-28)`, which is the
  7.5 build. 7.8 ships 4.8.5-39 and produces different bytes. The image is
  pinned by digest and the Dockerfile checks the exact gcc package.
- **dietlibc 0.34, the fefe release tarball.** Debian/Ubuntu ship
  `0.34~cvs20160606`, a later CVS snapshot whose startup code differs; it
  does not reproduce these bytes. It is installed with the repo's own
  `DOC/installdietlibc.sh`.
- **musl 1.2.1** built with that gcc into `/usr/local/musl`.
- **The two cross toolchains** from the URLs in `DOC/BuildVentoyFromSource.txt`.
- All four tarballs by SHA-256 in `SHA256SUMS`. fefe.de no longer serves
  the dietlibc tarball; `fetch.sh` falls back, in order, to Software
  Heritage (which stores it under the same SHA-256), Fedora's source
  cache, and the Wayback Machine. The `ventoy/musl-cross-make` toolchain
  is a `latest` release tag. If any upstream changes or goes away the
  build fails loudly rather than producing different bytes.

## Run it

```sh
sh BUSYBOX/chmod/ci/run-all.sh
```

That is: `fetch.sh` (download + verify the tarballs into `ci/dl`),
`docker build` of `Dockerfile.centos75`, `build.sh` inside the container
(writes `ci/out/BUSYBOX/...` and `ci/out/IMG/...`), `verify.sh` (cmp all
ten files, exit 1 on any mismatch), `test.sh` (smoke tests; the two non-x86
binaries need `qemu-user-static` and are skipped without it).
`.github/workflows/build-vtchmod.yml` runs the same steps.
