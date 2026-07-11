gcc -c syscall.S -o syscall.o
objcopy -O binary --only-section=.text syscall.o syscall.bin
