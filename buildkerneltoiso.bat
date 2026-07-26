@echo off
setlocal enabledelayedexpansion

rem ============================================
rem  IncedenaryOS Build Script
rem  Builds kernel, creates ISO, boots in QEMU
rem ============================================

set CC=clang
set CFLAGS=-m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nodefaultlibs -Wall -Wextra -Werror -c -target i386-unknown-elf
set AS=nasm
set ASFLAGS=-f elf32
set LD=ld.lld
set LDFLAGS=-m elf_i386 --script link.ld

set OBJECTS=loader.o kmain.o
set ISO_NAME=os.iso

rem === Check for clean argument ===
if "%1"=="clean" goto clean
if "%1"=="-c" goto clean

echo.
echo ==========================================
echo        Building IncedenaryOS
echo ==========================================
echo.

rem === Build each object file ===
echo [NASM] Assembling loader.s...
%AS% %ASFLAGS% loader.s -o loader.o
if errorlevel 1 (
    echo ERROR: NASM assembly failed!
    exit /b 1
)

echo [CLANG] Compiling kmain.c...
%CC% %CFLAGS% kmain.c -o kmain.o
if errorlevel 1 (
    echo ERROR: kmain.c compilation failed!
    exit /b 1
)

rem === Link kernel ===
echo [LD] Linking kernel.elf...
%LD% %LDFLAGS% %OBJECTS% -o kernel.elf
if errorlevel 1 (
    echo ERROR: Linking failed!
    exit /b 1
)

rem === Prepare ISO structure ===
if not exist iso\boot\grub mkdir iso\boot\grub

rem === Copy bootloader and kernel ===
copy /y stage2_eltorito iso\boot\grub > nul
if errorlevel 1 (
    echo WARNING: stage2_eltorito not found. Make sure it exists.
)
copy /y kernel.elf iso\boot\kernel.elf > nul
if errorlevel 1 (
    echo ERROR: kernel.elf not found!
    exit /b 1
)

rem === Create GRUB menu ===
(
    echo default=0
    echo timeout=0
    echo title IncedenaryOS
    echo kernel /boot/kernel.elf
) > iso\boot\grub\menu.lst

rem === Build ISO ===
echo [MKISOFS] Building %ISO_NAME%...
mkisofs -R -b boot/grub/stage2_eltorito -no-emul-boot -boot-load-size 4 -A "IncedenaryOS" -quiet -boot-info-table -o %ISO_NAME% iso
if errorlevel 1 (
    echo ERROR: mkisofs failed. Try using genisoimage instead.
    exit /b 1
)

rem === Run in QEMU ===
echo [QEMU] Booting IncedenaryOS...
echo.
qemu-system-x86_64 -cdrom %ISO_NAME%

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
if exist iso rmdir /s /q iso
echo Cleaned.
echo.
goto :eof