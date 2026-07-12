mkdir -p bin
gcc -c syscall.S -o ./bin/syscall.o
objcopy -O binary --only-section=.text ./bin/syscall.o ./bin/syscall.bin
