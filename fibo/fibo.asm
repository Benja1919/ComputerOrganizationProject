MAIN:
	add $s0, $zero, $imm, 0x8000	# minus
	add $s1, $zero, $imm, 0x100		# starting location
	add $t0, $zero, $zero, 0		# first fib element
	sw $t0, $zero, $s1, 0	        # load first element
	add $t1, $zero, $imm, 1			# second fib element
	add $s1, $s1, $imm, 1			# location++
	sw $t1, $zero, $s1, 0	        # load second element
LOOP:
	add $t2, $t0, $t1, 0			# $t2 = $t0 + $t1
	bgt $imm, $t2, $s0, RETURN		# if we reached the maximum value
	add $t0, $zero, $t1, 0			# $t0 becoms $t1
	add $t1, $zero, $t2, 0			# $t1 becoms $t2
	add $s1, $s1, $imm, 1			# location++
	sw $t2, $zero, $s1, 0			# store new fib number at next location
	beq $imm, $zero, $zero, LOOP	# jump back to main

RETURN:
	halt $zero, $zero, $zero, 0		# End	