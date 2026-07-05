#name: kvx-farcall-goto-gsym
#source: farcall-goto-gsym.s
#as:
#ld: -Ttext 0x1000
#error: .*\(.text\+0x0\): relocation truncated to fit: R_LVX_S27S2_PCREL against symbol `bar_gsym'.*
