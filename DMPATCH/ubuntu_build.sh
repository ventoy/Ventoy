#!/usr/bin/env bash

# Configurable variables
FTPIP="192.168.44.1"
FTPUSR='a:a'
KERNEL_SOURCE_DIR="/home/panda/linux-source-5.15.0"
BUILD_DIR="/home/panda/build"
MODULE_NAME="dm_patch"
MODULE_NAME_IBT="${MODULE_NAME}_ibt"
MODULE_FILES=("dmpatch.c" "Makefile" "Makefile_IBT")

# Function to download files
download_file() {
    local file=$1
    curl -s -u "$FTPUSR" "ftp://$FTPIP/$file" -o "$file"
    if [ -f "$file" ]; then
        echo "Download $file OK ..."
    else
        echo "Download $file FAILED ..."
        exit 1
    fi
}

# Function to build module
build_module() {
    local module_name=$1
    local makefile=$2

    echo "Building $module_name.ko ..."

    # Create a clean build directory
    rm -rf "./$module_name"
    mkdir "./$module_name"
    cp *.c "$module_name/"
    cp "$makefile" "$module_name/Makefile"

    # Build the module
    (cd "$KERNEL_SOURCE_DIR" && make modules M="$BUILD_DIR/$module_name/")
    strip --strip-debug "$BUILD_DIR/$module_name/$module_name.ko"

    # Copy the built module
    cp "$module_name/$module_name.ko" ./
}

# Download required files
for file in "${MODULE_FILES[@]}"; do
    download_file "$file"
done

# Remove previous kernel modules
rm -f *.ko

# Build modules
build_module "$MODULE_NAME" "Makefile"
build_module "$MODULE_NAME_IBT" "Makefile_IBT"

# Remove build directories
rm -rf ./$MODULE_NAME
rm -rf ./$MODULE_NAME_IBT

# Upload built modules
curl -s -T "$MODULE_NAME.ko" -u "$FTPUSR" "ftp://$FTPIP/${MODULE_NAME}_64.ko" || exit 1
curl -s -T "$MODULE_NAME_IBT.ko" -u "$FTPUSR" "ftp://$FTPIP/${MODULE_NAME_IBT}_64.ko" || exit 1

# Check success
if [ -f "./$MODULE_NAME.ko" -a -f "./$MODULE_NAME_IBT.ko" ]; then
    echo -e "\n\n=============== SUCCESS =============\n\n"
else
    echo -e "\n\n=============== FAILED ==============\n\n"
fi
