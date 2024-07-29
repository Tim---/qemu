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

