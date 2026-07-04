    .text
    .align 8
    .global check_args_are_correct
    .type   check_args_are_correct, @function
check_args_are_correct:
    andd $r16 = $sp, 7
    ;;
    cb.dnez $r16? 1f
    ;;
    make $r32 = 0x3f8000003f800000
    ;;
    compd.eq $r15 = $r0, $r32
    ;;
    cb.deqz $r15? 1f
    ;;
    make $r32 = 0x00000000bf800000
    ;;
    compd.eq $r15 = $r1, $r32
    ;;
    cb.deqz $r15? 1f
    ;;
    make $r32 = 0xbf80000000000000
    ;;
    compd.eq $r15 = $r2, $r32
    ;;
    cb.deqz $r15? 1f
    ;;
    make $r0 = 0x3f8000003f800000
    ;;
    make $r1 = 0x00000000bf800000
    ;;
    make $r2 = 0xbf80000000000000
    ;;
    goto check_args_are_correct_user /* tail call */
    ;;
1:
    make $r0 = 0
    ret
    ;;
