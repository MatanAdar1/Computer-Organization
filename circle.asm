    lw $t0, $imm2, $zero, $zero, 7, 0x100     # Load r from memory address 0x100 into $t0
    blt $zero, $t0, $imm1, $imm2, 1, halt     # If r < 1, halt the program
    out $zero, $imm1, $zero, $imm2, 21, 255   # Set monitor pixel color to white (255)
    mac $s0, $t0, $t0, $zero, 0, 0            # Compute r^2 and store it in $s0
    jal $ra, $zero, $zero, $imm2, 0, main     # Jump to main, storing return address in $ra

halt: 
    halt $zero, $zero, $zero, $zero, 0, 0		# Stop execution

main: 
    add $s1, $zero, $zero, $zero, 0, 0        # Initialize y = 0

Loop1:
    add $s2, $zero, $zero, $zero, 0, 0        # Initialize x = 0

Loop2: 
    sub $t1, $s2, $imm1, $zero, 127, 0        # Compute x - 127
    sub $t2, $s1, $imm1, $zero, 127, 0        # Compute y - 127
    mac $t1, $t1, $t1, $zero, 0, 0            # Square (x - 127) and store in $t1
    mac $t2, $t2, $t2, $zero, 0, 0            # Square (y - 127) and store in $t2
    add $t1, $t1, $t2, $zero, 0, 0            # Compute (x - 127)^2 + (y - 127)^2
    bgt $zero, $t1, $s0, $imm2, 0, L1         # If distance > r^2, skip drawing pixel
    mac $t2, $s1, $imm1, $s2, 256, 0          # Compute pixel address: (y * 256) + x
    out $zero, $imm1, $zero, $t2, 20, 0       # Set monitor address to pixel location
    out $zero, $imm1, $zero, $imm2, 22, 1     # Write white color to pixel (monitorcmd = 1)

L1:
    add $s2, $s2, $imm1, $zero, 1, 0          # Increment x
    blt $zero, $s2, $imm1, $imm2, 256, Loop2  # If x < 256, continue inner loop
    add $s1, $s1, $imm1, $zero, 1, 0          # Increment y
    blt $zero, $s1, $imm1, $imm2, 256, Loop1  # If y < 256, restart outer loop
    beq $zero, $zero, $zero, $ra, 0, 0        # Return to caller

    .word 0x100 0x35                            # Data section: radius value at address 0x35