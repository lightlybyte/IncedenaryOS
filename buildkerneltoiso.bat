@echo off
setlocal enabledelayedexpansion

rem ============================================
rem  IncedenaryOS Build Script
rem  GRUB Legacy (stage2_eltorito) + Text GUI
rem ============================================

set CC=clang
set CFLAGS=-m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nodefaultlibs -Wall -Wextra -Werror -c -target i386-unknown-elf
set AS=nasm
set ASFLAGS=-f elf32
set LD=ld.lld
set LDFLAGS=-m elf_i386 --script link.ld

set OBJECTS=loader.o kmain.o font.o gui.o font.psf.o
set ISO_NAME=os.iso
set FONT_FILE=font.psf

if "%1"=="clean" goto clean
if "%1"=="-c" goto clean

echo.
echo ==========================================
echo   Building IncedenaryOS (Text GUI)
echo ==========================================
echo.

if not exist %FONT_FILE% (
    echo WARNING: %FONT_FILE% not found. Creating placeholder...
    echo Placeholder font > %FONT_FILE%
)

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

echo [CLANG] Compiling font.c...
%CC% %CFLAGS% font.c -o font.o
if errorlevel 1 (
    echo ERROR: font.c compilation failed!
    exit /b 1
)

echo [CLANG] Compiling gui.c...
%CC% %CFLAGS% gui.c -o gui.o
if errorlevel 1 (
    echo ERROR: gui.c compilation failed!
    exit /b 1
)

rem === Convert font PSF to ELF object ===
echo [OBJCOPY] Embedding font...
if exist %FONT_FILE% (
    llvm-objcopy -I binary -O elf32-i386 -B i386 %FONT_FILE% font.psf.o
    if errorlevel 1 (
        objcopy -I binary -O elf32-i386 -B i386 %FONT_FILE% font.psf.o
    )
) else (
    echo Placeholder > %FONT_FILE%
    llvm-objcopy -I binary -O elf32-i386 -B i386 %FONT_FILE% font.psf.o
)

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
copy /y kernel.elf iso\boot\kernel.elf > nul

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
qemu-system-x86_64 -vga std -cdrom %ISO_NAME%

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