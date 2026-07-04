    .text
    .align 8
    .global check_args_are_correct
    .type   check_args_are_correct, @function
check_args_are_correct:
    andd $r16 = $sp, 7
    ;;
    cb.dnez $r16? 1f
    ;;
    compd.eq $r15 = $r1, 1
    ;;
    cb.deqz $r15? 1f
    ;;
    compd.eq $r15 = $r2, 2
    ;;
    cb.deqz $r15? 1f
    ;;
    ld $r16 = 0[$r0]
    ;;
    make $r32 = 0x0000000000000000
    ;;
    compd.eq $r16 = $r16, $r32
    ;;
    cb.deqz $r16? 1f
    ;;
    ld $r16 = 8[$r0]
    ;;
    make $r32 = 0x0000000100000001
    ;;
    compd.eq $r16 = $r16, $r32
    ;;
    cb.deqz $r16? 1f
    ;;
    ld $r16 = 16[$r0]
    ;;
    compd.eq $r16 = $r16, 2
    ;;
    cb.deqz $r16? 1f
    ;;
    ld $r16 = 24[$r0]
    ;;
    compd.eq $r16 = $r16, 3
    ;;
    cb.deqz $r16? 1f
    ;;
    ld $r16 = 32[$r0]
    ;;
    make $r32 = 0xcdefaa9955555555
    ;;
    compd.eq $r16 = $r16, $r32
    ;;
    cb.deqz $r16? 1f
    ;;
    ld $r16 = 40[$r0]
    ;;
    make $r32 = 0xdeadbeef12345678
    ;;
    compd.eq $r16 = $r16, $r32
    ;;
    cb.deqz $r16? 1f
    ;;
    ld $r16 = 48[$r0]
    ;;
    make $r32 = 0xbcdefbb888888888
    ;;
    compd.eq $r16 = $r16, $r32
    ;;
    cb.deqz $r16? 1f
    ;;
    ld $r16 = 56[$r0]
    ;;
    make $r32 = 0xdeaddead12345678
    ;;
    compd.eq $r16 = $r16, $r32
    ;;
    cb.deqz $r16? 1f
    ;;
    ld $r16 = 64[$r0]
    ;;
    make $r32 = 0xbcdefcc777777777
    ;;
    compd.eq $r16 = $r16, $r32
    ;;
    cb.deqz $r16? 1f
    ;;
    ld $r16 = 72[$r0]
    ;;
    make $r32 = 0xbeefbeef12345678
    ;;
    compd.eq $r16 = $r16, $r32
    ;;
    cb.deqz $r16? 1f
    ;;
    make $r1 = 1
    ;;
    make $r2 = 2
    ;;
    goto check_args_are_correct_user /* tail call */
    ;;
1:
    make $r0 = 0
    ret
    ;;
