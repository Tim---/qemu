===========
QEMU README
===========

Modified Qemu to run AMD Zen-related processors.

Building
========

.. code-block:: shell

  mkdir build
  cd build
  ../configure \
      --target-list=xtensa-softmmu,arm-softmmu,x86_64-softmmu \
      --with-devices-xtensa=smu \
      --with-devices-arm=psp \
      --with-devices-x86_64=pc_zen \
      --without-default-features \
      --without-default-devices
  make


Running
=======

Given a BIOS image `bios.bin`, you can emulate the PSP:

.. code-block:: shell

  ./qemu-system-arm -M psp-summit-ridge -drive if=mtd,format=raw,file=bios.bin -d guest_errors,unimp

Using the same image, you can also emulate the x86 processor:

.. code-block:: shell

  ./qemu-system-x86_64 -M pc-zen -cpu summit-ridge -drive if=mtd,format=raw,file=bios.bin

You can also extract the firmware of the SMU to `/tmp/smu.bin`, and emulate the SMU:

.. code-block:: shell

  ./qemu-system-xtensa -M smu-v9 -d guest_errors,unimp


Note: for the PSP and X86 machines, you can choose between summit-ridge / pinnacle-ridge / raven-ridge / picasso / matisse / vermeer / lucienne / renoir / cezanne.

Note: for the SMU, you can choose between smu-v9 / smu-v10 / smu-v11.
