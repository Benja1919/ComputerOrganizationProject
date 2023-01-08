MAIN:
	add $a0, $zero, $zero, 0					# set 1st sector adress
	add $a1, $zero, $imm, 1						# set 2nd sector adress
	jal $ra, $imm, $zero, Disk_test				# start disktest func
	beq $imm, $zero, $zero, RETURN				#end program from Main section
	
Disk_test:

	add $s0, $zero, $imm, 1						# $s0 = const 1
	add $t2, $zero, $imm, 1023
	out $t2, $imm, $zero, 16					# ioregister[16 = diskbuffer] = $s0 = 0 -> start with first memory cell
	
	add $sp, $sp, $imm, -4						# set place for 1 items
	sw $ra, $sp, $zero, 0						# save return adress in stack	
	jal $ra, $imm, $zero, Wait	
	out $a0, $imm, $zero, 15					# ioregister[15 = disksector] = $a0 -> select the first sector given
	out $s0, $imm, $zero, 14					# ioregister[14 = diskcmd] = 1 -> read

	add $t0, $zero, $imm, 7						# i = 7
First_loop:
	lw $t1, $t2, $t0, 0						# load the i'th word from memory (after DMA from sector)
	add $s1, $s1, $t1, 0						# sum $s1 = sector[a0][i]
	sub $t0, $t0, $imm, 1						# i--		
	bge $imm, $t0, $zero, First_loop			# while i >= 0
	sw $s1, $zero, $imm, 0x100					# save the sum in memory

	jal $ra, $imm, $zero, Wait
	out $a1, $imm, $zero, 15					# ioregister[15 = disksector] = $a1 -> select the first sector given
	out $s0, $imm, $zero, 14					# ioregister[14 = diskcmd] = 1 -> read

	add $t0, $zero, $imm, 7						#set i = 7
Sec_loop:
	lw $t1, $t2, $t0, 0						# load the i'th word from memory (after DMA from sector
	add $s2, $s2, $t1, 0						# sum $s2 = sector[a1][i]
	sub $t0, $t0, $imm, 1						# i--
	bge $imm, $t0, $zero, Sec_loop				# while i >= 0
	sw $s2, $zero, $imm, 0x101					# save the sum in memory
	
	bgt $imm, $s1, $s2, Save_first				#compare two sums
	sw $s2, $zero, $imm, 0x102
	beq $imm, $zero, $zero, end_loop			#end program

Save_first:
	sw $s1, $zero, $imm, 0x102
	beq $imm, $zero, $zero, end_loop			#end program

end_loop:										# back to main
	lw $ra, $sp, $zero, 0
	add $sp, $sp, $imm, 4
	beq $ra, $zero, $zero, 0					

RETURN:
	halt $zero, $zero, $zero, 0					# End

Wait:
	in $t0, $zero, $imm, 17						# check diskstatus
	beq $ra, $t0, $zero, 0						# disk is not busy
	beq $imm, $zero, $zero, Wait				# loop till disk is free to work	