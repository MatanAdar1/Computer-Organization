Start:    
    lw $a0, $imm1, $zero, $zero, 0x100, 0 #load n-value to a0
    lw $a1, $imm1, $zero, $zero, 0x101, 0 #load k-value to a1
    add $ra, $imm2, $zero, $zero, 0, End #set last return address
    add $s2, $zero, $zero, $zero, 0, 0 #initial add value
Init:
    sw $s2, $sp, $zero, $zero, 0, 0 #push added value to stack
    sw $a1, $sp, $imm1, $zero, 1, 0 #push k-value to stack
    sw $a0, $sp, $imm1, $zero, 2, 0 #push n-value to stack
    sw $ra, $sp, $imm1, $zero, 3, 0 #push return address to stack
    add $sp, $sp, $imm1, $zero, 4, 0 #move stack head
    xor $t1, $a0, $a1, $zero, 0, 0 #set t1 to 0 if n==k
    xor $t2, $a1, $zero, $zero, 0, 0 #set t2 to 0 if k==0
    beq $zero, $t1, $zero, $imm2, 0, Stop #stop if first condition was satisfied
    beq $zero, $t2, $zero, $imm2, 0, Stop #stop if second condition was satisfied
Main:
    add $s2, $zero, $zero, $zero, $zero, 0, 0 #set result to 0
    sub $a0, $a0, $imm1, $zero, 1, 0 # n = n-1
    sub $a1, $a1, $imm1, $zero, 1, 0 # k = k-1
    jal $ra, $zero, $zero, $imm2, 0, Init #Binom(n-1,k-1)
    add $s2, $s2, $v0, $zero, 0, 0 #add return
    add $a1, $a1, $imm1, $zero, 1, 0 # k = k+1
    jal $ra, $zero, $zero, $imm2, 0, Init #Binom(n-1,k-1)
    add $s2, $s2, $v0, $zero, 0, 0 
    beq $zero, $zero, $zero, $imm1, Rollback, 0 #prepare for return
Stop:
    add $v0, $imm1, $zero, $zero, 1, 0 #Set v0=1
    sub $sp, $sp, $imm1, $zero, 4, 0 #roll back the stack
    lw $s2, $sp, $zero, $zero, 0, 0 #pop added value from stack
    lw $a1, $sp, $imm1, $zero, 1, 0 #pop k-value from stack
    lw $a0, $sp, $imm1, $zero, 2, 0 #pop n-value from stack
    lw $ra, $sp, $imm1, $zero, 3, 0 #pop return address from stack
    beq $zero, $zero, $zero, $ra, 0, 0 #return
Rollback:
    add $v0, $s2, $zero, $zero, 0, 0 #set $v0=Binom(n-1,k)+Binom(n-1,k-1)
    sub $sp, $sp, $imm1, $zero, 4, 0 #roll back the stack
    lw $s2, $sp, $zero, $zero, 0, 0 #pop added value from stack
    lw $a1, $sp, $imm1, $zero, 1, 0 #pop k-value from stack
    lw $a0, $sp, $imm1, $zero, 2, 0 #pop n-value from stack
    lw $ra, $sp, $imm1, $zero, 3, 0 #pop return address from stack
    beq $zero, $zero, $zero, $ra, 0, 0 #return
End:
    sw $v0, $imm2, $zero, $zero, 0, 0x102 #Store the result
    halt $zero, $zero, $zero, $zero, 0, 0 # halt
    .word 0x100 12 #the value of n
    .word 0x101 5 #the value of k