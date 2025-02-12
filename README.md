# TheBootloader

TheBootloader is a simple 16-bit x86 bootloader written in assembly, designed to understand the fundamentals of low-level programming and system bootstrapping.

---

## Current Status

---

## Target
1. **Short-Term Goals:**
   - Enhance the bootloader to support basic input handling (e.g., reading keystrokes).
   - Implement functionality to display strings dynamically.

2. **Long-Term Goals:**
   - Develop a multi-stage bootloader.
   - Load and execute a small kernel or program from disk.
   - Explore file system support.

---

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
