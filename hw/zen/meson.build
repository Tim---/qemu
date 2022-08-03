specific_ss.add(when: 'CONFIG_FCH', if_true: files(
    'fch.c', 'fch-lpc-bridge.c', 'fch-spi.c', 'fch-rtc.c',
))
specific_ss.add(when: 'CONFIG_ZEN', if_true: files(
    'zen-rom.c', 'zen-cpuid.c', 'zen-utils.c', 'zen-mobo.c', 'zen-pci-root.c',
    'smu.c',
))
specific_ss.add(when: 'CONFIG_PSP', if_true: files(
    'psp-regs.c', 'psp-ocbl.c', 'psp-smn-bridge.c', 'psp-dirty.c', 'psp-timer.c',
    'psp-ht-bridge.c', 'psp-fuses.c', 'psp-soc.c',
))

specific_ss.add(when: 'CONFIG_CCP', if_true: files('ccp.c', 'ccp-ops.c'))
specific_ss.add(when: 'CONFIG_CCP', if_true: cc.find_library('nettle'))
specific_ss.add(when: 'CONFIG_CCP', if_true: cc.find_library('crypto'))
