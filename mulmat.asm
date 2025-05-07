	# Initialize loop variables
	add $a0, $zero, $zero, $zero, 0, 0 	# $a0 := i = 0
Outer_Loop: 
	add $a1, $zero, $zero, $zero, 0, 0 	# $a1 := j = 0
Inner_Loop: 
	add $t2, $zero, $zero, $zero, 0, 0 	# $t2 := sum = 0
	add $s2, $zero, $zero, $zero, 0, 0 	# $s2 := k = 0
Multiply_Loop: 
	mac $t0, $a0, $imm1, $s2, 4, 0 # $t0 = i * 4 + k
	add $t0, $t0, $imm1, $zero, 0x100, 0 # $t0 = &matrix1[i * 4 + k]
	mac $t1, $s2, $imm1, $a1, 4, 0	# k * 4 + j
	add $t1, $t1, $imm1, $zero, 0x110 # $t1 = &matrix2[k * 4 + j]
	lw $t0, $t0, $zero, $zero, 0, 0 # $t0 = MEM[&matrix1[i * 4 + k]] = matrix1[i * 4 + k]
	lw $t1, $t1, $zero, $zero , 0, 0	#$t1 = matrix2[k * 4 + j]
	mac $t2, $t0, $t1, $t2, 0, 0 # sum += matrix1[i * 4 + k] * matrix2[k * 4 + j] 
	add $s2, $s2, $imm1, $zero, 1, 0 # k++ 
	blt $zero, $s2, $imm1, $imm2, 4, Multiply_Loop # if k < 4 jump to Multiply_Loop
	mac $t0, $a0, $imm1, $a1, 4, 0 # $t0 = i * 4 + j
	sw $zero, $t0, $imm1, $t2, 0x120, 0 # MEM[i * 4 + j + 0x120] = sum = matrix_result[i * 4 + j] = sum; 
	add $a1, $imm1, $a1, $zero, 1, 0 # j++
	blt $zero, $a1, $imm1, $imm2, 4, Inner_Loop # if (j < 4) jump to Inner_Loop
	add $a0, $imm1, $a0, $zero, 1, 0 	# i++
	blt $zero, $a0, $imm1, $imm2, 4, Outer_Loop # if (i < 4) jump to Outer_Loop
	halt $zero, $zero, $zero, $zero, 0, 0 # exit the program
	
	# Matrix 1 Data
	.word 0x100 1 
	.word 0x101 2 
	.word 0x102 3 
	.word 0x103 4 
	.word 0x104 5 
	.word 0x105 6
	.word 0x106 7 
	.word 0x107 8 
	.word 0x108 1
	.word 0x109 2 
	.word 0x10A 3 
	.word 0x10B 4 
	.word 0x10C 5 
	.word 0x10D 6
	.word 0x10E 7 
	.word 0x10F 8 
	
	# Matrix 2 Data
	.word 0x110 9
	.word 0x111 2
	.word 0x112 4
	.word 0x113 6
	.word 0x114 4
	.word 0x115 5
	.word 0x116 42
	.word 0x117 3
	.word 0x118 9
	.word 0x119 6
	.word 0x11A 4
	.word 0x11B 3
	.word 0x11C 4
	.word 0x11D 7
	.word 0x11E 42
	.word 0x11F 3 
