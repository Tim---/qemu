xtensa_ss = ss.source_set()
xtensa_ss.add(files(
  'mx_pic.c',
  'pic_cpu.c',
  'xtensa_memory.c',
))
xtensa_ss.add(when: 'CONFIG_XTENSA_SIM', if_true: files('sim.c'))
xtensa_ss.add(when: 'CONFIG_XTENSA_VIRT', if_true: files('virt.c'))
xtensa_ss.add(when: 'CONFIG_XTENSA_XTFPGA', if_true: files('xtfpga.c'))
xtensa_ss.add(when: 'CONFIG_XTENSA_SMU', if_true: files('xtensa_smu.c', 'xtensa_smu_smuio.c', 'xtensa_smu_regs.c'))

hw_arch += {'xtensa': xtensa_ss}
