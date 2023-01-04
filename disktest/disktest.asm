MAIN:
	add $t0, $zero, $imm, 1			 			# $t0 = num of sectors = 2 sectors (0,1) 
	add $s0, $zero, $imm, 0			 			# $s0 = 0
	add $s1, $zero, $imm, 1						# $s1 = const 1
	out $s0, $imm, $zero, 16					# ioregister[16 = diskbuffer] = $s0 = 0
	add $t1, $zero, $zero, 0					# $t1 = index = 0
	# Read data from sector
	out $t1, $imm, $zero, 15					# ioregister[15 = disksector] = $t1
	out $s1, $imm, $zero, 14					# ioregister[14 = diskcmd] = 1 for read
	in $t2, $imm, $zero, 16						# $t2 = ioregister[16 = diskbuffer]
	add $t1, $t1, $imm, 1						# $t1++
	halt $zero, $zero, $zero, 0					# End	