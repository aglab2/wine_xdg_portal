CC=/opt/llvm-mingw/bin/i686-w64-mingw32-gcc
FOLDER=/run/media/admin/SSD/Project64Luna

SOURCES="procfs.c wine_xdg_portal.c log.c recon.c portal.c heavens.c errno_conv.c string_conv.c pattern_parser.c"
CFLAGS="-mssse3 -fvisibility=hidden -g -gcodeview"
BINARY=wine_xdg_portal

mkdir -p bin
$CC $SOURCES $CFLAGS -shared -o bin/$BINARY.dll --for-linker --pdb=bin/$BINARY.pdb -DWP_DLL -Ldbus -ldbus-static -Idbus
cp ./bin/$BINARY.dll ./bin/$BINARY.pdb $FOLDER
