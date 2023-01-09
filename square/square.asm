.word 0x100 8000
.word 0x101 80
MAIN:
	lw $s0, $zero, $imm, 0x100	            # $s0 = left top corner from address 0x100
	lw $s1, $zero, $imm, 0x101	            # $s1 = size of rec from address 0x100
	add $s2, $zero, $imm, 255				# $s2 = 255
	add $t0, $zero, $s0, 0					# $t0 = index_1 = $s0
	add $t1, $zero, $zero, 0	 			# $t1 = index_2 = 0
	mul $t2, $s2, $s1, 0					# $t2 = 255*size of rec
	add $t2, $t2, $s0, 0					# $t2 = 255*size of rec + initial loc
	add $t2, $t2, $s1, 0					# $t2 = 255*size of rec + initial loc + size of rec
	add $s0, $zero, $imm, 1					# $s0 = 1
LOOP:
	bge $imm, $t0, $t2, END					# if index_1 >= $t2 jump to END
	bge $imm, $t1, $s1, INC					# if index_2 >= size of rec jump to INC
	out $t0, $zero, $imm, 20				# read pixel address 	
	out $s2, $zero, $imm, 21				# set pixel color to white
	out $s0, $zero, $imm, 22				# draw pixel
	add $t1, $t1, $imm, 1					# index_2++
	add $t0, $t0, $imm, 1					# index_1++	
	beq $imm, $zero, $zero, LOOP			# jump to LOOP

INC:
	add $t0, $t0, $imm, 256					# index_1+=256
	mul $s1, $s1, $imm, -1					# $s1 = -$s1
	add $t0, $t0, $s1, 0					# index_1-=$s1
	mul $s1, $s1, $imm, -1					# $s1 = -(-$s1) = $s1
	add $t1, $zero, $zero, 0				# index_2 = 0
			
	beq $imm, $zero, $zero, LOOP			# jump to LOOP
END:
	halt $zero, $zero, $zero, 0				# halt