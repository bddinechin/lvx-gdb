    .text
    .align 8
    .global check_args_are_correct
    .type   check_args_are_correct, @function
check_args_are_correct:
    andd $r16 = $sp, 7
    ;;
    cb.dnez $r16? 1f
    ;;
    compd.eq $r15 = $r0, 0
    ;;
    cb.deqz $r15? 1f
    ;;
    make $r32 = 0x0000000100000001
    ;;
    compd.eq $r15 = $r1, $r32
    ;;
    cb.deqz $r15? 1f
    ;;
    make $r32 = 0x0000000200000002
    ;;
    compd.eq $r15 = $r2, $r32
    ;;
    cb.deqz $r15? 1f
    ;;
    make $r32 = 0x0000000300000003
    ;;
    compd.eq $r15 = $r3, $r32
    ;;
    cb.deqz $r15? 1f
    ;;
    make $r32 = 0x0000000400000004
    ;;
    compd.eq $r15 = $r4, $r32
    ;;
    cb.deqz $r15? 1f
    ;;
    compd.eq $r15 = $r5, 5
    ;;
    cb.deqz $r15? 1f
    ;;
    make $r32 = 0x0000000600000006
    ;;
    compd.eq $r15 = $r6, $r32
    ;;
    cb.deqz $r15? 1f
    ;;
    make $r32 = 0x0000000700000007
    ;;
    compd.eq $r15 = $r7, $r32
    ;;
    cb.deqz $r15? 1f
    ;;
    compd.eq $r15 = $r8, 8
    ;;
    cb.deqz $r15? 1f
    ;;
    compd.eq $r15 = $r9, 9
    ;;
    cb.deqz $r15? 1f
    ;;
    compd.eq $r15 = $r10, 10
    ;;
    cb.deqz $r15? 1f
    ;;
    compd.eq $r15 = $r11, 11
    ;;
    cb.deqz $r15? 1f
    ;;
    ld $r16 = 0[$sp]
    ;;
    make $r32 = 0x0000000c0000000c
    ;;
    compd.eq $r16 = $r16, $r32
    ;;
    cb.deqz $r16? 1f
    ;;
    ld $r16 = 8[$sp]
    ;;
    make $r32 = 0x0000000d0000000d
    ;;
    compd.eq $r16 = $r16, $r32
    ;;
    cb.deqz $r16? 1f
    ;;
    ld $r16 = 16[$sp]
    ;;
    compd.eq $r16 = $r16, 14
    ;;
    cb.deqz $r16? 1f
    ;;
    ld $r16 = 24[$sp]
    ;;
    compd.eq $r16 = $r16, 15
    ;;
    cb.deqz $r16? 1f
    ;;
    make $r0 = 0
    ;;
    make $r1 = 0x0000000100000001
    ;;
    make $r2 = 0x0000000200000002
    ;;
    make $r3 = 0x0000000300000003
    ;;
    make $r4 = 0x0000000400000004
    ;;
    make $r5 = 5
    ;;
    make $r6 = 0x0000000600000006
    ;;
    make $r7 = 0x0000000700000007
    ;;
    make $r8 = 8
    ;;
    make $r9 = 9
    ;;
    make $r10 = 10
    ;;
    make $r11 = 11
    ;;
    goto check_args_are_correct_user /* tail call */
    ;;
1:
    make $r0 = 0
    ret
    ;;
