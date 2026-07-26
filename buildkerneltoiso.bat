nasm -f elf32 loader.s
ld.lld -T link.ld -melf_i386 loader.o -o kernel.elf
mkdir iso\boot\grub
copy stage2_eltorito iso\boot\grub
copy kernel.elf iso\boot\grub\kernel.elf
echo default=0 > iso\boot\grub\menu.lst
echo timeout=0 >> iso\boot\grub\menu.lst
echo title IncedenaryOS >> iso\boot\grub\menu.lst
echo kernel /boot/kernel.elf >> iso\boot\grub\menu.lst
mkisofs -R -b boot/grub/stage2_eltorito -no-emul-boot -boot-load-size 4 -A os -quiet -boot-info-table -o os.iso iso
qemu-system-x86_64 -cdrom os.iso