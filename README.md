# Vector Forth Kernel

A bare-metal x86-64 operating system kernel implementing a Forth virtual machine with AVX-512 SIMD-optimized stack operations.

## Architecture

- **Boot**: Two-stage bootloader (MBR → Protected Mode → Long Mode)
- **Kernel**: 64-bit kernel with full memory management, interrupts, and drivers
- **Forth VM**: Threaded code interpreter with SIMD-accelerated data stack
- **Network**: Full TCP/IP stack implemented as Forth words

## Build Requirements

- NASM assembler
- x86_64-elf-gcc cross-compiler (or native gcc on x86_64 Linux)
- GNU LD
- QEMU for testing

### Installing Cross-Compiler (Ubuntu)

```bash
sudo apt install nasm qemu-system-x86
# For cross-compiler, build from source or use:
sudo apt install gcc-x86-64-linux-gnu