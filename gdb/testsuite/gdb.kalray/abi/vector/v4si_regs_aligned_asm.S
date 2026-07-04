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
    compd.eq $r15 = $r4, 4
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
    make $r32 = 0x0000000800000008
    ;;
    compd.eq $r15 = $r8, $r32
    ;;
    cb.deqz $r15? 1f
    ;;
    make $r32 = 0x0000000900000009
    ;;
    compd.eq $r15 = $r9, $r32
    ;;
    cb.deqz $r15? 1f
    ;;
    make $r32 = 0x0000000a0000000a
    ;;
    compd.eq $r15 = $r10, $r32
    ;;
    cb.deqz $r15? 1f
    ;;
    make $r32 = 0x0000000b0000000b
    ;;
    compd.eq $r15 = $r11, $r32
    ;;
    cb.deqz $r15? 1f
    ;;
    ld $r16 = 0[$sp]
    ;;
    compd.eq $r16 = $r16, 12
    ;;
    cb.deqz $r16? 1f
    ;;
    ld $r16 = 8[$sp]
    ;;
    compd.eq $r16 = $r16, 13
    ;;
    cb.deqz $r16? 1f
    ;;
    make $r0 = 0x0000000000000000
    ;;
    make $r1 = 0x0000000100000001
    ;;
    make $r2 = 0x0000000200000002
    ;;
    make $r3 = 0x0000000300000003
    ;;
    make $r4 = 4
    ;;
    make $r5 = 5
    ;;
    make $r6 = 0x0000000600000006
    ;;
    make $r7 = 0x0000000700000007
    ;;
    make $r8 = 0x0000000800000008
    ;;
    make $r9 = 0x0000000900000009
    ;;
    make $r10 = 0x0000000a0000000a
    ;;
    make $r11 = 0x0000000b0000000b
    ;;
    goto check_args_are_correct_user /* tail call */
    ;;
1:
    make $r0 = 0
    ret
    ;;
