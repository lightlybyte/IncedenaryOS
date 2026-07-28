@echo off
setlocal enabledelayedexpansion

rem ============================================
rem  IncedenaryOS Build Script
rem  Boot: El Torito CD-ROM (os.iso)
rem  Storage: FAT12 Floppy (hdd.img)
rem ============================================

set CC=clang
set CFLAGS=-m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nodefaultlibs -Wall -Wextra -Werror -c -target i386-unknown-elf
set AS=nasm
set ASFLAGS=-f elf32
set LD=ld.lld
set LDFLAGS=-m elf_i386 --script link.ld

set OBJECTS=loader.o kmain.o
set ISO_NAME=os.iso
set HDD_NAME=hdd.img

if "%1"=="clean" goto clean
if "%1"=="-c" goto clean

echo.
echo ==========================================
echo   Building IncedenaryOS (POSIX Shell)
echo ==========================================
echo.

echo [NASM] Assembling loader.s...
nasm -f elf32 loader.s -o loader.o
if errorlevel 1 (
    echo ERROR: NASM assembly failed!
    exit /b 1
)

echo [CLANG] Compiling kmain.c...
clang -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nodefaultlibs -Wall -Wextra -Werror -c kmain.c -o kmain.o -target i386-unknown-elf
if errorlevel 1 (
    echo ERROR: kmain.c compilation failed!
    exit /b 1
)

echo [LD] Linking kernel.elf...
ld.lld -m elf_i386 loader.o kmain.o -o kernel.elf --script link.ld
if errorlevel 1 (
    echo ERROR: Linking failed!
    exit /b 1
)

rem === Prepare ISO structure ===
if not exist iso\boot\grub mkdir iso\boot\grub

rem === Copy bootloader and kernel ===
copy /y stage2_eltorito iso\boot\grub > nul
copy /y kernel.elf iso\boot\kernel.elf > nul

rem === Create GRUB menu ===
(
    echo default=0
    echo timeout=0
    echo title IncedenaryOS
    echo kernel /boot/kernel.elf
) > iso\boot\grub\menu.lst

rem === Build OS ISO ===
echo [MKISOFS] Building %ISO_NAME%...
mkisofs -R -b boot/grub/stage2_eltorito -no-emul-boot -boot-load-size 4 -A "IncedenaryOS" -quiet -boot-info-table -o %ISO_NAME% iso
if errorlevel 1 (
    echo ERROR: mkisofs failed. Try using genisoimage instead.
    exit /b 1
)

rem === Create FAT12 Storage Floppy (1.44 MB) ===
echo [HDD] Creating FAT12 storage floppy %HDD_NAME%...
if not exist %HDD_NAME% (
    fsutil file createnew %HDD_NAME% 1474560 > nul
    echo [HDD] FAT12 storage floppy created: %HDD_NAME%
) else (
    echo [HDD] FAT12 storage floppy already exists.
)

rem === Run in QEMU ===
echo [QEMU] Booting IncedenaryOS...
echo.
qemu-system-x86_64 -vga std -cdrom %ISO_NAME% -fda %HDD_NAME% -serial stdio

echo.
echo ==========================================
echo Build complete.
echo ==========================================
echo.
goto :eof

rem ============================================
:clean
echo Cleaning build artifacts...
del /q *.o 2>nul
del /q kernel.elf 2>nul
del /q %ISO_NAME% 2>nul
del /q %HDD_NAME% 2>nul
if exist iso rmdir /s /q iso
echo Cleaned.
echo.
goto :eof