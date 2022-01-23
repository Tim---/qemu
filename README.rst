==================
QEMU PSP emulation
==================

This is an implementation of the
`AMD PSP <https://en.wikipedia.org/wiki/AMD_Platform_Security_Processor>`_
for QEMU, currently a work in progress.

Building
========

.. code-block:: shell

  mkdir build
  cd build
  ../configure --target-list=arm-softmmu
  make

Running
=======

Only the serial port:

.. code-block:: shell

  ./qemu-system-arm -M psp -nographic -monitor none -bios $rom_file

Debugging information:

.. code-block:: shell

  ./qemu-system-arm -M psp -d "unimp,guest_errors,trace:psp*" -nographic -monitor none -bios $rom_file

Basic gdbinit:

.. code-block:: shell

  set pagination off
  set confirm off
  set arch arm

  define hook-quit
  monitor quit
  end

  target remote | ./qemu-system-arm -M psp -d "unimp,guest_errors,trace:psp*" -nographic -monitor none -S -gdb stdio -serial none -bios $rom_file
