# Plos

**Plos** is a custom, preemptive multitasking operating system kernel built for the `x86_64` architecture. It is designed from scratch as an educational journey into low-level systems engineering, memory management, and hardware abstraction.

Plos boots via the [Limine](https://limine-bootloader.org/) bootloader and fully supports modern UEFI environments.

## Key Features

* **Boot Protocol:** Limine boot protocol compliance with UEFI support (OVMF).
* **Memory Management:**
    * Physical Memory Manager (PMM) with a buddy allocator.
    * Virtual Memory Manager (VMM) supporting paging, Higher-Half Direct Mapping (HHDM), and Lazy Allocation with page-fault resolution.
    * Custom Kernel Heap allocator (kmalloc / kfree).
* **Hardware Abstraction:**
    * Global Descriptor Table (GDT) and Interrupt Descriptor Table (IDT).
    * ACPI parsing and Local APIC (LAPIC) initialization.
    * Hardware Timer configuration.
* **Multithreading & Scheduling:**
    * 2-Level Hierarchical Scheduler: Separates resource ownership (Task/Process) from execution units (Thread).
    * Preemptive Round-Robin: Driven by LAPIC timer interrupts.
    * Voluntary Preemption: Support for software interrupt-driven yielding (int $50).
    * Proper Thread/Task lifecycle management (Zombie state and Idle-task reaping).
* **Debugging:** Integrated serial output logging for deep kernel introspection.

## Prerequisites

To build and run Plos, you will need a Unix-like environment (Linux or macOS) with the following tools installed:

* **Toolchain:** A standard C compiler (cc, gcc, or clang) and make.
* **ISO Generation:** xorriso (required to pack the bootable ISO).
* **Emulation:** qemu-system-x86_64 (for running the OS).
* **Documentation:** doxygen (optional, for generating code docs).
* **Utilities:** git, curl, tar, gunzip.

*(Note: The Limine bootloader and EDK2 OVMF firmware are automatically downloaded by the Makefile).*

## Quick Start

### 1. Build the OS
Clone the repository and run the default make target. This will fetch the necessary dependencies, compile the kernel, and generate the bootable ISO (plos-x86_64.iso).

`make`

### 2. Run in QEMU (UEFI)
To spin up the OS in QEMU using the Q35 machine type and OVMF firmware:

`make run`

*(By default, this allocates 2GB of RAM and redirects the serial port to your terminal's standard output).*

## Debugging

Plos includes a dedicated make target for kernel debugging using GDB. This target starts QEMU in a paused state (-S) and opens a GDB stub on port 1234 (-s).

1. Start the debug target:
   `make debug-x86_64`

2. In a separate terminal, launch your cross-compiled GDB and attach it to QEMU:
   `x86_64-elf-gdb kernel/bin/kernel`
   `(gdb) target remote localhost:1234`
   `(gdb) break kmain`
   `(gdb) continue`

## Documentation

The project source code is documented. You can generate the HTML/LaTeX documentation using Doxygen via the Makefile:

`make docs`

The generated documentation will be placed in the configured Doxygen output directory.

## Cleaning Up

To remove the generated ISO, QEMU hard drive images, and the iso_root directory:

`make clean`

To perform a deep clean (removes downloaded Limine binaries, OVMF firmware, and kernel object files):

`make distclean`

---

Built with passion and lots of Triple Faults.
