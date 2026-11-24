AS = nasm
CC = gcc
LD = ld

CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector
LDFLAGS = -m elf_i386 -T linker.ld

all: sarp-os.iso

boot.o: boot.asm
	$(AS) -f elf32 boot.asm -o boot.o

kernel.o: kernel.c
	$(CC) $(CFLAGS) -c kernel.c -o kernel.o

sarp-os.bin: boot.o kernel.o linker.ld
	$(LD) $(LDFLAGS) -o sarp-os.bin boot.o kernel.o

sarp-os.iso: sarp-os.bin
	mkdir -p isodir/boot/grub
	cp sarp-os.bin isodir/boot/sarp-os.bin
	printf 'menuentry "Sarp OS v0.1" {\n\tmultiboot /boot/sarp-os.bin\n}\n' > isodir/boot/grub/grub.cfg
	grub-mkrescue -o sarp-os.iso isodir

run: sarp-os.iso
	qemu-system-x86_64 -cdrom sarp-os.iso

clean:
	rm -rf *.o *.bin *.iso isodir
