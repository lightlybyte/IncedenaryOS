# IncedenaryOS

![IncedenaryOS Demo](TST.gif)

An OS that is made by the user lightlybyte (@lightlybyt on TikTok), it is meant to be a basic daily-driver operating system with essential drivers like PS/2, USB, HDMI, Analog Audio, and possibly other drivers in the future like DisplayPort. Made in C/C++ and NASM. Uses Batchfile and GNU Linker.

Whilst still being text mode GUI

---

## Features

- Custom 32-bit kernel with Multiboot support
- VGA text mode output (80x25)
- PS/2 keyboard driver (polling-based)
- Basic Executive shell with an executable format
- FAT12 filesystem detection (demo)
- Scrolling support
- Backspace and Enter key support in shell

---

## Requirements

Before building, ensure the following tools are installed and accessible in your PATH:

- **NASM** — Assembler for the bootloader stub
- **Clang / LLVM** — Compiler with support for `-target i386-unknown-elf`
- **LLD** — LLVM linker (part of the LLVM toolchain)
- **QEMU** — Emulator for testing the OS
- **mkisofs** or **genisoimage** — Tool for creating the bootable ISO image

All tools are available for Windows, Linux, and macOS. On Windows, it is recommended to use a MinGW or MSYS2 environment for the necessary command-line utilities.

---

## How to Build

You build by running the file:

```bash
buildkerneltoiso.bat
```
This will output an os.iso along with other files.

---

## How to Clear Build Directory

You use the same build file but run the command:

```bash
buildkerneltoiso.bat clear
```
This will remove all the generated files from build, purging the directory to how it was before.

---

## License

This project is open source. See the [LICENSE](LICENSE) file for details.

---

## Links

- GitHub Repository: https://github.com/lightlybyte/IncedenaryOS
- TikTok: https://www.tiktok.com/@lightlybyt