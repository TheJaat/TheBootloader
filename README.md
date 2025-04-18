# TheBootloader

TheBootloader is the multi stage bootloader for the TheTaaJ OS.


## Current Status

- [x] **Multi-Stage Bootloader:** Two-stages implemented.
- [x] **File System Support:** ISO9660 FileSystem - Read, Load the Stage2 and Kernel from it.


## How to Build and Run
### Requirements:
- **NASM**: An assembler for x86 programs.
- **QEMU**: An emulator to test the bootloader.

### Build Steps:
1. Clone the repository:
   ```bash
   git clone https://github.com/TheJaat/TheBootloader
   cd TheBootloader
   ```
3. Build the binary using the Makefile:
   ```bash
   make
   ```
3. Run the bootloader in QEMU:
   ```bash
   make run
   ```
4. Clean Up:
   ```bash
   make clean
   ```

## License
This project is open-source and available under the MIT License.
