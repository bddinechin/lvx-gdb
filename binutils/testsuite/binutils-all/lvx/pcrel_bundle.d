#name: pcrel_bundle
#source: pcrel_bundle.s
#PROG: objcopy
#as:
#objdump: -dr
#...

Disassembly of section .text:

0000000000000000 <foo>:
   0:	00 0e 00 e1 00 00 00 80 00 00 00 00             	pcrel \$r0 = 56 \(0x38\);;

   c:	0d 00 00 98                                     	call 40 <bar>
  10:	00 0a 00 e1 00 00 00 80 00 00 00 00             	pcrel \$r0 = 40 \(0x28\);;

  1c:	09 00 00 98                                     	call 40 <bar>
  20:	00 06 00 e1 00 00 00 b8 00 00 00 80             	pcrel \$r0 = 24 \(0x18\)
  2c:	00 00 00 00                                     	ld \$r0 = 0 \(0x0\)\[\$r0\];;

  30:	c1 ff 07 7f                                     	nop;;

  34:	c1 ff 07 7f                                     	nop;;


0000000000000038 <.table>:
  38:	c1 ff 07 7f                                     	nop;;

  3c:	c1 ff 07 7f                                     	nop;;


0000000000000040 <bar>:
  40:	c1 ff 07 7f                                     	nop;;

