    .text
    .align 8
    .global check_args_are_correct
    .type   check_args_are_correct, @function
check_args_are_correct:
    andd $r16 = $sp, 7
    ;;
    cb.dnez $r16? 1f
    ;;
    make $r32 = 0x0000000000000000
    ;;
    compd.eq $r15 = $r0, $r32
    ;;
    cb.deqz $r15? 1f
    ;;
    make $r32 = 0x0000000000000001
    ;;
    compd.eq $r15 = $r1, $r32
    ;;
    cb.deqz $r15? 1f
    ;;
    make $r32 = 0x0000000000000002
    ;;
    compd.eq $r15 = $r2, $r32
    ;;
    cb.deqz $r15? 1f
    ;;
    make $r32 = 0x0000000000000003
    ;;
    compd.eq $r15 = $r3, $r32
    ;;
    cb.deqz $r15? 1f
    ;;
    make $r32 = 0x0000000000000004
    ;;
    compd.eq $r15 = $r4, $r32
    ;;
    cb.deqz $r15? 1f
    ;;
    make $r32 = 0x0000000000000005
    ;;
    compd.eq $r15 = $r5, $r32
    ;;
    cb.deqz $r15? 1f
    ;;
    make $r32 = 0x0000000000000006
    ;;
    compd.eq $r15 = $r6, $r32
    ;;
    cb.deqz $r15? 1f
    ;;
    make $r32 = 0x0000000000000007
    ;;
    compd.eq $r15 = $r7, $r32
    ;;
    cb.deqz $r15? 1f
    ;;
    make $r32 = 0x0000000000000008
    ;;
    compd.eq $r15 = $r8, $r32
    ;;
    cb.deqz $r15? 1f
    ;;
    make $r32 = 0x0000000000000009
    ;;
    compd.eq $r15 = $r9, $r32
    ;;
    cb.deqz $r15? 1f
    ;;
    make $r32 = 0x000000000000000A
    ;;
    compd.eq $r15 = $r10, $r32
    ;;
    cb.deqz $r15? 1f
    ;;
    make $r32 = 0x000000000000000B
    ;;
    compd.eq $r15 = $r11, $r32
    ;;
    cb.deqz $r15? 1f
    ;;
    ld $r16 = 0[$sp]
    ;;
    compd.eq $r16 = $r16, 123456
    ;;
    cb.deqz $r16? 1f
    ;;
    ld $r16 = 8[$sp]
    ;;
    compd.eq $r16 = $r16, 0xdead
    ;;
    cb.deqz $r16? 1f
    ;;
    ld $r16 = 16[$sp]
    ;;
    compd.eq $r16 = $r16, 0xbeef
    ;;
    cb.deqz $r16? 1f
    ;;
    ld $r16 = 24[$sp]
    ;;
    compd.eq $r16 = $r16, 0xcafe
    ;;
    cb.deqz $r16? 1f
    ;;
    ld $r16 = 32[$sp]
    ;;
    compd.eq $r16 = $r16, 0xbebe
    ;;
    cb.deqz $r16? 1f
    ;;
    make $r0 = 0x0000000000000000
    ;;
    make $r1 = 0x0000000000000001
    ;;
    make $r2 = 0x0000000000000002
    ;;
    make $r3 = 0x0000000000000003
    ;;
    make $r4 = 0x0000000000000004
    ;;
    make $r5 = 0x0000000000000005
    ;;
    make $r6 = 0x0000000000000006
    ;;
    make $r7 = 0x0000000000000007
    ;;
    make $r8 = 0x0000000000000008
    ;;
    make $r9 = 0x0000000000000009
    ;;
    make $r10 = 0x000000000000000A
    ;;
    make $r11 = 0x000000000000000B
    ;;
    goto check_args_are_correct_user /* tail call */
    ;;
1:
    make $r0 = 0
    ret
    ;;
