MAIN:
	add $t0, $imm1, $zero, $zero, 7, 0 			# 7 sectors  
	add $s0, $zero, $zero, $zero, 0, 0 			# $s0 = 0
	out $zero, $imm1, $zero, $s0, 16, 0			# ioregister[16 = diskbuffer] = $s0 = 0
	
FOR:	
	blt $zero, $t0, $imm1, $imm2, 0, RETURN 	# break if $t0 (started at 7) is less than 0
	add $t2, $t0, $imm1, $zero, 1, 0 			# $t2 = $t0 + 1 (the next sector)

  	jal $ra, $zero, $zero, $imm2, 0, WAIT		# Wait till disk is ready
	# Read data from sector
	out $zero, $imm1, $zero, $t0, 15, 0			# ioregister[15 = disksector] = $t0
	out $zero, $imm1, $zero, $imm2, 14, 1		# ioregister[14 = diskcmd] = 1 for read
	 
	jal $ra, $zero, $zero, $imm2, 0, WAIT		# Wait till disk is ready
	# Write data to sector
	out $zero, $imm1, $zero, $t2, 15, 0			# ioregister[15 = disksector] = $t2 = $t0 + 1
	out $zero, $imm1, $zero, $imm2, 14, 2		# ioregister[14 = diskcmd] = 2 for write

	add $t0, $t0, $imm1, $zero, -1, 0 			# $t0--
	beq $zero, $zero, $zero, $imm2, 0, FOR		# Jump to next sector

WAIT:
	in $t1, $imm1, $zero, $zero, 17, 0			# $t1 = ioregister[17] meaning disk status
	beq $zero, $t1, $imm1, $imm2, 1, WAIT		# While disk is busy
	beq $zero, $zero, $zero, $ra, 0, 0			# Return to loop
	
	
RETURN:
	halt $zero, $zero, $zero, $zero, 0, 0		# End	