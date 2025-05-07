	# Enable disk interrupt (IRQ1)
	out $zero, $zero, $imm1, $imm2, 1, 1		# Enable IRQ1 for disk operations

	# Set stack pointer to 2048 (1 << 11)
	sll $sp, $imm1, $imm2, $zero, 1, 11		# Initialize stack pointer

	# Assign IRQ handler address
	out $zero, $imm1, $zero, $imm2, 6, irq_service		# Set interrupt handler to irq_service

	# Initialize disk registers
	out $zero, $imm1, $zero, $imm2, 15, 7		# Set disk sector number to 7
	out $zero, $imm1, $zero, $imm2, 16, 0x0		# Set disk buffer address to 0x0

	# Set initial operation mode and sector counter
	add $t2, $imm1, $zero, $zero, 1, 0			# $t2 = 1 (READ mode)
	add $s2, $imm1, $zero, $zero, 7, 0			# $s2 = 7 (sector count)

process_disk:
	# Read disk status
	in $t0, $imm1, $zero, $zero, 17, 0			# $t0 = diskstatus

	# If disk is busy (diskstatus == 1), skip issuing command
	beq $zero, $t0, $imm1, $imm2, 1, skip_command

	# Issue disk operation (READ or WRITE)
	out $zero, $imm1, $zero, $t2, 14, 0			# diskcmd = $t2

skip_command:
	# If remaining sectors > 0, continue processing
	bne $zero, $s2, $imm1, $imm2, 0xffffffff, process_disk	

	# End execution
	halt $zero, $zero, $zero, $zero, 0, 0		

irq_service:
	# Check if last operation was WRITE
	beq $zero, $t2, $imm1, $imm2, 2, switch_to_write	

	# Increment sector number
	add $t1, $imm1, $s2, $zero, 1, 0			

	# Update disk sector register
	out $zero, $imm1, $zero, $t1, 15, 0			

	# Change mode to WRITE
	add $t2, $imm1, $zero, $zero, 2, 0			

	# Return from interrupt
	beq $zero, $zero, $zero, $imm2, 0, irq_exit	

switch_to_write:
	# Change mode back to READ
	add $t2, $imm1, $zero, $zero, 1, 0			

	# Decrease sector count
	sub $s2, $s2, $imm1, $zero, 1, 0			

	# Update disk sector register
	out $zero, $imm1, $zero, $s2, 15, 0			

irq_exit:
	# Clear IRQ1 status and return from interrupt
	out $zero, $zero, $imm2, $zero, 0, 4		
	reti $zero, $zero, $zero, $zero, 0, 0		
