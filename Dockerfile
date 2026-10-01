# Modern Ventoy builder - AlmaLinux 9 (replaces dead CentOS 7)
FROM almalinux:9 AS builder

RUN dnf update -y && \
    dnf install -y epel-release && \
    dnf install -y \
        gcc gcc-c++ make git tar bzip2 patch perl findutils \
        glibc-devel glibc-devel.i686 \
        gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu \
        gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu \
        clang lld llvm llvm-devel llvm-static \
        mingw64-gcc mingw64-binutils mingw64-cpp \
        mingw64-llvm mingw64-llvm-static mingw64-llvm-tools \   # <-- This is what you wanted
        python3 python3-pip python3-devel \
        qemu virt-* libvirt* fuse-devel dejavu-sans-fonts \
        gnu-efi nasm acpica-tools xz bzip2-devel \
        && dnf clean all

# cmake-converter (for any old VS project conversions)
# cross compilation via cmake n clang 
RUN pip3 install cmake-converter

# Full llvm-mingw (pure Clang-based MinGW cross, including static libs)
# Latest as of early April 2026 — update the version when a newer release drops
ARG LLVM_MINGW_VERSION=20250401
RUN curl -L -o /tmp/llvm-mingw.tar.xz \
    https://github.com/mstorsjo/llvm-mingw/releases/download/\( {LLVM_MINGW_VERSION}/llvm-mingw- \){LLVM_MINGW_VERSION}-ucrt-ubuntu-20.04-x86_64.tar.xz && \
    mkdir -p /opt/llvm-mingw && \
    tar -xf /tmp/llvm-mingw.tar.xz -C /opt/llvm-mingw --strip-components=1 && \
    rm -f /tmp/llvm-mingw.tar.xz

ENV PATH="/opt/llvm-mingw/bin:${PATH}"

WORKDIR /ventoy
COPY . /ventoy

# Default: run iPXE build (override with docker compose or docker run)
CMD ["bash", "-c", "cd IPXE && ./buildipxe.sh"]
