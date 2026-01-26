all: iso




kernel_main.o: kernel/kernel.c
	gcc -m32 -ffreestanding -fno-pie -fno-pic -c kernel/kernel.c -o kernel_main.o

boot.o: boot.asm
	nasm -f elf32 boot.asm -o boot.o

gdt.o: kernel/gdt/gdt.c kernel/gdt/gdt.h
	gcc -m32 -ffreestanding  -c kernel/gdt/gdt.c -o gdt.o

tss.o: kernel/gdt/tss.c kernel/gdt/tss.h
	gcc -m32 -ffreestanding  -c kernel/gdt/tss.c -o tss.o

loader.o: kernel/gdt/loader.asm
	nasm -f elf32 kernel/gdt/loader.asm -o loader.o

idt.o: kernel/idt/idt.c kernel/idt/idt.h
	gcc -m32 -ffreestanding  -c kernel/idt/idt.c -o idt.o

idt_loader.o: kernel/idt/idt_loader.asm
	nasm -f elf32 kernel/idt/idt_loader.asm -o idt_loader.o

isr.o: kernel/isr/isr.c kernel/isr/isr.h
	gcc -m32 -ffreestanding  -c kernel/isr/isr.c -o isr.o

isr_stub.o: kernel/isr/isr_stub.asm
	nasm -f elf32 kernel/isr/isr_stub.asm -o isr_stub.o

isr_handler.o: kernel/isr/isr_handler.c
	gcc -m32 -ffreestanding  -c kernel/isr/isr_handler.c -o isr_handler.o

irq_handler.o: kernel/irq/irq_handler.c
	gcc -m32 -ffreestanding  -c kernel/irq/irq_handler.c -o irq_handler.o

irq.o: kernel/irq/irq.c kernel/irq/irq.h
	gcc -m32 -ffreestanding  -c kernel/irq/irq.c -o irq.o

irq_stub.o: kernel/irq/irq_stub.asm
	nasm -f elf32 kernel/irq/irq_stub.asm -o irq_stub.o

pic.o: kernel/irq/pic.c kernel/irq/pic.h
	gcc -m32 -ffreestanding  -c kernel/irq/pic.c -o pic.o

io.o: kernel/io/io.c kernel/io/io.h
	gcc -m32 -ffreestanding  -c kernel/io/io.c -o io.o

serial.o: kernel/drivers/serial.c kernel/drivers/serial.h
	gcc -m32 -ffreestanding  -c kernel/drivers/serial.c -o serial.o

memmap.o: kernel/multiboot/memorymap.c kernel/multiboot/memorymap.h
	gcc -m32 -ffreestanding  -c kernel/multiboot/memorymap.c -o memmap.o

libs.o: kernel/libs/memhelp.c kernel/libs/memhelp.h
	gcc -m32 -ffreestanding  -c kernel/libs/memhelp.c -o libs.o

pmm.o: kernel/mem/pmm.c kernel/mem/pmm.h
	gcc -m32 -ffreestanding  -c kernel/mem/pmm.c -o pmm.o

paging.o: kernel/mem/paging.c kernel/mem/paging.h
	gcc -m32 -ffreestanding  -c kernel/mem/paging.c -o paging.o

slab.o: kernel/mem/slab.c kernel/mem/slab.h
	gcc -m32 -ffreestanding  -c kernel/mem/slab.c -o slab.o

kernel_heap.o: kernel/mem/kernel_heap.c kernel/mem/kernel_heap.h
	gcc -m32 -ffreestanding  -c kernel/mem/kernel_heap.c -o kernel_heap.o

pit.o: kernel/drivers/pit_timer.c kernel/drivers/pit_timer.h
	gcc -m32 -ffreestanding  -c kernel/drivers/pit_timer.c -o pit.o

process.o: kernel/multitasking/proccess.c kernel/multitasking/proccess.h
	gcc -m32 -ffreestanding  -c kernel/multitasking/proccess.c -o process.o

scheduler.o: kernel/multitasking/scheduler.c kernel/multitasking/scheduler.h
	gcc -m32 -ffreestanding  -c kernel/multitasking/scheduler.c -o scheduler.o

ide_io.o: kernel/drivers/ide/ide_io.c kernel/drivers/ide/ide_io.h
	gcc -m32 -ffreestanding -c kernel/drivers/ide/ide_io.c -o ide_io.o

ide.o: kernel/drivers/ide/ide.c kernel/drivers/ide/ide.h
	gcc -m32 -ffreestanding -c kernel/drivers/ide/ide.c -o ide.o

vga.o: kernel/drivers/vga.c kernel/drivers/vga.h
	gcc -m32 -ffreestanding -c kernel/drivers/vga.c -o vga.o

shell.bin: kernel/shell/shell.asm
	nasm -f bin kernel/shell/shell.asm -o shell.bin

shell.o: shell.bin
	objcopy -I binary -O elf32-i386 -B i386 shell.bin shell.o --rename-section .data=.userprog



kernel.elf: boot.o kernel_main.o gdt.o tss.o loader.o idt.o idt_loader.o isr.o isr_stub.o isr_handler.o irq.o irq_stub.o irq_handler.o pic.o io.o serial.o memmap.o libs.o pmm.o paging.o slab.o kernel_heap.o pit.o shell.o process.o scheduler.o ide_io.o ide.o vga.o
	ld -m elf_i386 -T linker.ld -o kernel.elf boot.o kernel_main.o gdt.o tss.o loader.o idt.o idt_loader.o isr.o isr_stub.o isr_handler.o irq.o irq_stub.o irq_handler.o pic.o io.o serial.o memmap.o libs.o pmm.o paging.o slab.o kernel_heap.o pit.o  shell.o process.o scheduler.o ide_io.o ide.o vga.o

iso: kernel.elf
	mkdir -p isodir/boot/grub
	cp kernel.elf isodir/boot/kernel.elf
	cp grub/grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o ytOS.iso isodir


run: iso 
	qemu-system-i386 -cdrom ytOS.iso -m 4G -serial stdio -drive file=disk.img,if=ide,index=0

clean:
	rm -rf isodir
	rm -f *.o
	rm -f kernel.elf
	rm -f ytOS.iso

.PHONY: all iso run clean





