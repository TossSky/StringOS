rm kernel.o kernel.bin bootsect.o bootsect.bin
echo "-------------- compile kernel --------------\n"
g++ -fno-pie -ffreestanding -m32 -o kernel.o -c kernel.cpp

echo "\n-------------- link kernel --------------\n"
ld --oformat binary -Ttext 0x10000 -o kernel.bin --entry=kmain -m elf_i386 kernel.o

echo "\n-------------- compile bootsect --------------\n"
as --32 -o bootsect.o bootsect.asm

echo "\n-------------- link bootsect --------------\n"
ld -Ttext 0x7c00 --oformat binary -m elf_i386 -o bootsect.bin bootsect.o

qemu -drive format=raw,file=bootsect.bin,index=0,if=floppy -drive format=raw,file=kernel.bin,index=1,if=floppy
