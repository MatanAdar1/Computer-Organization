#define _CRT_SECURE_NO_WARNINGS

// Includes
#include <stdio.h>      // For file I/O and standard I/O operations
#include <stdlib.h>     // For general utilities (exit, atoi, etc.)
#include <stdint.h>     // For fixed-width integer types
#include <stdbool.h>    // For boolean type in C
#include <string.h>     // For string operations (strcmp, strcpy, etc.)

// Constants
#define MEM_SIZE 4096              // Instruction and data memory size
#define DISK_SIZE (128 * 128)      // Disk size in words (128 sectors * 128 words/sector)
#define MONITOR_SIZE (256 * 256)   // Monitor resolution (256x256)
#define NUM_CPU_REGS 16            // Number of CPU registers
#define NUM_IO_REGS 23             // Number of I/O registers
#define PC_START 0                 // Initial value of the Program Counter

// I/O Register Indexes
#define IRQ0_ENABLE 0
#define IRQ1_ENABLE 1
#define IRQ2_ENABLE 2
#define IRQ0_STATUS 3
#define IRQ1_STATUS 4
#define IRQ2_STATUS 5
#define IRQ_HANDLER 6
#define IRQ_RETURN 7
#define CLOCK_CYCLE 8
#define LEDS 9
#define DISPLAY_7SEG 10
#define TIMER_ENABLE 11
#define TIMER_CURRENT 12
#define TIMER_MAX 13
#define DISK_CMD 14
#define DISK_SECTOR 15
#define DISK_BUFFER 16
#define DISK_STATUS 17
// Index 18 and 19 are 'reserved' (not explicitly used in code)
#define MONITOR_ADDR 20
#define MONITOR_DATA 21
#define MONITOR_CMD 22

// Opcodes
#define RETI_OP 18   // Return-from-interrupt opcode
#define HALT_OP 21   // Halt opcode

// Globals for CPU and Memory
uint64_t instruction_memory[MEM_SIZE] = { 0 };  // Instruction memory array
uint32_t data_memory[MEM_SIZE] = { 0 };         // Data memory array
uint32_t disk_memory[DISK_SIZE] = { 0 };        // Disk memory array
uint32_t cpu_registers[NUM_CPU_REGS] = { 0 };   // Array for CPU registers
uint32_t io_registers[NUM_IO_REGS] = { 0 };     // Array for I/O registers

// Globals for Monitor and Special Hardware
uint32_t monitor_buffer[MONITOR_SIZE] = { 0 };  // Buffer representing monitor pixels
uint32_t led_status = 0;                        // LED register content (32 bits)
uint32_t seven_segment_display = 0;             // 7-segment display register content

// Globals for Simulation State
uint32_t program_counter = PC_START;            // Current PC (program counter)
int halt_flag = 0;                              // Flag to indicate HALT instruction encountered
int isr_active_flag = 0;                        // Flag to indicate CPU is currently in ISR
int irq2_next_cycle = 0;                        // Stores next clock cycle for an IRQ2 event
int disk_cycle_counter = 0;                     // Tracks the timing for disk operations
int disk_index = 0;                             // Tracks how many words have been transferred in a disk op

// I/O Register Names (for debug/logging)
char *io_register_names[NUM_IO_REGS] = {
    "irq0enable", "irq1enable", "irq2enable", "irq0status", "irq1status", "irq2status",
    "irqhandler", "irqreturn", "clks", "leds", "display7seg", "timerenable",
    "timercurrent", "timermax", "diskcmd", "disksector", "diskbuffer", "diskstatus",
    "reserved", "reserved", "monitoraddr", "monitordata", "monitorcmd"
};

// File Pointers for Input
FILE *irq2_file = NULL;  // IRQ2 events file pointer

// File Pointers for Output
FILE *trace_file = NULL;                // Instruction trace output file
FILE *hw_register_trace_file = NULL;    // HW register trace output file
FILE *cycle_count_file = NULL;          // Cycle count output file
FILE *led_output_file = NULL;           // LED output file
FILE *seven_segment_output_file = NULL; // 7-segment output file
FILE *disk_output_file = NULL;          // Disk output file
FILE *monitor_output_file = NULL;       // Monitor text output file
FILE *monitor_yuv_file = NULL;          // Monitor YUV output file (binary)
FILE *register_output_file = NULL;      // Final register values output file


/*
 * log_instruction_trace:
 * -----------------------
 * Logs the current instruction in hexadecimal, along with the 16 registers in hex,
 * to the trace_file. This is meant to track each instruction execution.
 */
void log_instruction_trace(uint32_t pc, uint64_t instruction, uint32_t *registers) {
    // If we've halted the CPU but the disk is still busy, avoid logging additional instructions
    if (halt_flag == 1 && io_registers[DISK_STATUS] == 1)
        return;

    // Print PC in 3-digit hex, instruction in 12-digit hex, then register values
    fprintf(trace_file, "%03X %012llX ", pc, instruction);
    for (int i = 0; i < NUM_CPU_REGS; i++) {
        fprintf(trace_file, "%08x", registers[i]);
        if (i != NUM_CPU_REGS - 1)
            fprintf(trace_file, " ");
    }
    fprintf(trace_file, "\n");
}

/*
 * increment_clock_cycle:
 * -----------------------
 * Increments the system clock cycle register (CLOCK_CYCLE).
 * If it overflows (0xFFFFFFFF), wraps back to 0.
 */
void increment_clock_cycle(uint32_t *io_registers) {
    io_registers[CLOCK_CYCLE] = (io_registers[CLOCK_CYCLE] == 0xFFFFFFFF) ? 0 : io_registers[CLOCK_CYCLE] + 1;
}

/*
 * sign_extend:
 * -------------
 * Extends a 'value' (bits wide) to a 32-bit signed value.
 * bits: how many bits (of 'value') are used to determine the sign bit.
 */
int32_t sign_extend(int32_t value, int bits) {
    int32_t mask = 1 << (bits - 1);  // Calculate the position of the sign bit
    return (value ^ mask) - mask;    // XOR and subtract to preserve sign
}

/*
 * decode_instruction:
 * --------------------
 * Decodes a 48-bit instruction into register indices and a 24-bit immediate.
 * registerUsed: an array to store the 4 register fields (4 bits each).
 * Returns the 24-bit immediate portion of the instruction.
 */
uint32_t decode_instruction(uint64_t instruction, int *registersUsed) {
    uint64_t mask = 0x00F000000000; // Mask to isolate the register fields
    int shift = 36;                 // Bit shift amount (starting for first reg)

    // Extract 4 registers, each 4 bits
    for (int i = 0; i < 4; i++) {
        registersUsed[i] = (instruction & mask) >> shift;
        mask >>= 4;  // Move mask to next 4 bits
        shift -= 4;  // Update shift accordingly
    }

    // Return the 24 least significant bits as immediate
    return instruction & 0xFFFFFF;
}

/*
 * read_next_irq:
 * ---------------
 * Reads the next line of IRQ2 file (already fetched as 'line') to get the
 * clock cycle where the next IRQ2 event occurs.
 * Returns -1 if the line pointer is NULL (indicating no more IRQ events).
 */
int read_next_irq(char *line) {
    if (!line) {
        return -1; // No more IRQ events
    }
    return atoi(line); // Convert line content to an integer
}

/*
 * write_monitor_data:
 * --------------------
 * Writes the entire monitor buffer to two output files:
 *   1. yuv_file: in raw binary (each byte is a pixel)
 *   2. text_file: in hex form, but only up to the highest non-zero pixel index.
 */
void write_monitor_data(FILE *text_file, FILE *yuv_file) {
    int max = 0;

    // Find the highest monitor_buffer index that is not zero
    for (int i = 0; i < MONITOR_SIZE; i++) {
        uint8_t pixel_value = monitor_buffer[i];
        if (monitor_buffer[i] != 0x0)
            max = i;

        // Write raw pixel data to YUV file
        fwrite(&pixel_value, sizeof(uint8_t), 1, yuv_file);
    }

    // Write hex pixel values to text file up to 'max' index
    if (max != 0) {
        for (int i = 0; i < max + 1; i++) {
            uint8_t pixel_value = monitor_buffer[i];
            fprintf(text_file, "%02X\n", pixel_value);
        }
    }
}

/*
 * load_memory:
 * -------------
 * Loads 64-bit instruction words from a file into 'memory' (size mem_size).
 * Reads each line as a hex string of width 'hex_width' bits.
 */
void load_memory(const char *filename, uint64_t *memory, size_t mem_size, int hex_width) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening memory input file");
        exit(EXIT_FAILURE);
    }

    char line[256];
    size_t addr = 0;

    // Read lines until either memory is full or we reach EOF
    while (fgets(line, sizeof(line), file) && addr < mem_size) {
        uint64_t value;
        sscanf(line, "%llx", &value); // Convert hex to 64-bit value
        memory[addr++] = value;
    }

    fclose(file);
}

/*
 * load_memory32:
 * ---------------
 * Similar to 'load_memory' but for 32-bit data values.
 */
void load_memory32(const char *filename, uint32_t *memory, size_t mem_size) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening memory input file");
        exit(EXIT_FAILURE);
    }

    char line[256];
    size_t addr = 0;

    while (fgets(line, sizeof(line), file) && addr < mem_size) {
        uint32_t value;
        sscanf(line, "%x", &value); // Convert hex to 32-bit value
        memory[addr++] = value;
    }

    fclose(file);
}

/*
 * save_memory:
 * -------------
 * Saves 32-bit memory array ('memory') to a file.
 * It first finds the highest nonzero index to optimize output.
 */
void save_memory(const char *filename, uint32_t *memory, size_t mem_size) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        perror("Error opening memory output file");
        exit(EXIT_FAILURE);
    }
    int max = 0;

    // Find the highest nonzero word
    for (int i = 0; i < mem_size; i++) {
        if (memory[i] != 0) {
            max = i;
        }
    }

    // Write values up to that index
    if (max != 0) {
        for (int i = 0; i < max + 1; i++) {
            fprintf(file, "%08X\n", memory[i]);
        }
    }

    fclose(file);
}

/*
 * update_timer:
 * --------------
 * Increments the timer if TIMER_ENABLE is set and compares TIMER_CURRENT with TIMER_MAX.
 * If they match, it resets TIMER_CURRENT and triggers IRQ0_STATUS.
 */
void update_timer(uint32_t *io_registers) {
    if (io_registers[TIMER_ENABLE]) {
        if (io_registers[TIMER_CURRENT] == io_registers[TIMER_MAX]) {
            io_registers[TIMER_CURRENT] = 0;
            io_registers[IRQ0_STATUS] = 1; // Trigger timer interrupt
        }
        else {
            io_registers[TIMER_CURRENT]++;
        }
    }
}

/*
 * handle_disk_operations:
 * ------------------------
 * Manages disk read/write operations (based on DISK_CMD).
 * Disk operations proceed in cycles, each 8 clock cycles transferring one word.
 * After 1024 cycles (128 words transferred), the operation is complete.
 */
void handle_disk_operations() {
    int buffer_address = io_registers[DISK_BUFFER];  // Memory address for transfer
    int sector_number = io_registers[DISK_SECTOR];   // Sector index on the disk

    // If there's a disk command and we're starting a new operation (disk_cycle_counter == 0)
    if (io_registers[DISK_CMD] != 0 && disk_cycle_counter == 0) {
        io_registers[DISK_STATUS] = 1; // Disk is busy
        disk_index = 0;
    }

    // READ operation (DISK_CMD == 1), every 8 cycles transfer 1 word
    if (io_registers[DISK_CMD] == 1 && (disk_cycle_counter % 8 == 0)) {
        data_memory[buffer_address + disk_index] = disk_memory[sector_number * 128 + disk_index];
        disk_index++;
    }
    // WRITE operation (DISK_CMD == 2), every 8 cycles transfer 1 word
    else if (io_registers[DISK_CMD] == 2 && (disk_cycle_counter % 8 == 0)) {
        disk_memory[sector_number * 128 + disk_index] = data_memory[buffer_address + disk_index];
        disk_index++;
    }

    // After 1024 cycles (128 words * 8 cycles/word), finish operation
    if (disk_cycle_counter == 1024) {
        disk_cycle_counter = 0;
        disk_index = 0;
        io_registers[DISK_CMD] = 0;     // Reset the disk command
        io_registers[DISK_STATUS] = 0;  // Disk is now free
        io_registers[IRQ1_STATUS] = 1;  // Trigger IRQ1 (disk operation complete)
    }

    // If a disk command is ongoing, increment the cycle counter
    if (io_registers[DISK_CMD] != 0) {
        disk_cycle_counter++;
    }
}

/*
 * handle_timer_operations:
 * -------------------------
 * Similar to update_timer, increments TIMER_CURRENT if enabled
 * and triggers IRQ0_STATUS if we hit TIMER_MAX.
 * (This appears to be a duplicate approach; see update_timer above.)
 */
void handle_timer_operations() {
    if (io_registers[TIMER_ENABLE] == 1) {
        if (io_registers[TIMER_CURRENT] == io_registers[TIMER_MAX]) {
            io_registers[TIMER_CURRENT] = 0;
            io_registers[IRQ0_STATUS] = 1;
        }
        else {
            io_registers[TIMER_CURRENT]++;
        }
    }
}

/*
 * handle_led_and_display_operations:
 * -----------------------------------
 * For OUT instruction (opcode == 20), checks if the target I/O register is LEDs or DISPLAY_7SEG
 * and logs it into respective output files.
 */
void handle_led_and_display_operations(int opcode, int *registers_used) {
    if (opcode == 20) { // OUT instruction
        int io_register_index = cpu_registers[registers_used[1]] + cpu_registers[registers_used[2]];
        if (io_register_index == LEDS) {
            // Log LED change
            fprintf(led_output_file, "%d %08x\n", io_registers[CLOCK_CYCLE], io_registers[LEDS]);
        }
        else if (io_register_index == DISPLAY_7SEG) {
            // Log 7-segment display change
            fprintf(seven_segment_output_file, "%d %08X\n", io_registers[CLOCK_CYCLE], io_registers[DISPLAY_7SEG]);
        }
    }
}

/*
 * handle_monitor_operations:
 * ---------------------------
 * If monitorcmd is set to 1, writes a pixel to 'monitor_buffer' at address = monitoraddr
 * with value = monitordata.
 */
void handle_monitor_operations() {
    if (io_registers[MONITOR_CMD] == 1) {
        monitor_buffer[io_registers[MONITOR_ADDR]] = io_registers[MONITOR_DATA];
    }
}

/*
 * log_hw_register_operations:
 * ----------------------------
 * Logs every IN or OUT instruction to the hw_register_trace_file,
 * showing clock cycle, read/write type, register name, and the data.
 */
void log_hw_register_operations(int opcode, int *registers_used) {
    if (opcode == 19) { // IN instruction
        int io_register_index = cpu_registers[registers_used[1]] + cpu_registers[registers_used[2]];
        fprintf(hw_register_trace_file, "%d READ %s %08x\n", io_registers[CLOCK_CYCLE],
                io_register_names[io_register_index], io_registers[io_register_index]);
    }
    else if (opcode == 20) { // OUT instruction
        int io_register_index = cpu_registers[registers_used[1]] + cpu_registers[registers_used[2]];
        fprintf(hw_register_trace_file, "%d WRITE %s %08x\n", io_registers[CLOCK_CYCLE],
                io_register_names[io_register_index], io_registers[io_register_index]);
    }
}

/*
 * check_interrupts:
 * ------------------
 * Returns 1 if any of IRQ0, IRQ1, or IRQ2 is enabled and pending.
 * Otherwise, returns 0.
 */
int check_interrupts() {
    int irq_pending = 0;

    // Check Timer Interrupt
    if (io_registers[IRQ0_ENABLE] && io_registers[IRQ0_STATUS]) {
        irq_pending = 1;
    }
    // Check Disk Interrupt
    if (io_registers[IRQ1_ENABLE] && io_registers[IRQ1_STATUS]) {
        irq_pending = 1;
    }
    // Check External Interrupt
    if (io_registers[IRQ2_ENABLE] && io_registers[IRQ2_STATUS]) {
        irq_pending = 1;
    }

    return irq_pending;
}

/*
 * handle_peripherals:
 * --------------------
 * Wrapper function that updates all peripheral-related logic each cycle:
 *   - Disk, timer, LEDs/7-seg display, monitor, and HW register traces.
 */
void handle_peripherals(int opcode, int *registers_used) {
    handle_disk_operations();
    handle_timer_operations();
    handle_led_and_display_operations(opcode, registers_used);
    handle_monitor_operations();
    log_hw_register_operations(opcode, registers_used);
}

/*
 * check_and_handle_interrupts:
 * -----------------------------
 * If an interrupt is pending and we are not in an ISR, jump to the IRQ_HANDLER
 * and set isr_active_flag.
 */
void check_and_handle_interrupts(uint32_t *pc) {
    int interrupt_pending = (io_registers[IRQ0_ENABLE] && io_registers[IRQ0_STATUS]) ||
                            (io_registers[IRQ1_ENABLE] && io_registers[IRQ1_STATUS]) ||
                            (io_registers[IRQ2_ENABLE] && io_registers[IRQ2_STATUS]);

    // If there's a pending interrupt and we're not already servicing one
    if (interrupt_pending && !isr_active_flag) {
        io_registers[IRQ_RETURN] = *pc;  // Save current PC
        *pc = io_registers[IRQ_HANDLER]; // Jump to ISR
        isr_active_flag = 1;             // Set ISR flag
    }
}

/*
 * handle_reti:
 * -------------
 * RETI instruction logic: restore PC from IRQ_RETURN and clear ISR flag.
 */
void handle_reti(uint32_t *pc) {
    *pc = io_registers[IRQ_RETURN];
    isr_active_flag = 0;
}

/*
 * clear_serviced_interrupts:
 * ---------------------------
 * Clears the status bits of IRQ0, IRQ1, and IRQ2 if they're enabled and active.
 */
void clear_serviced_interrupts() {
    if (io_registers[IRQ0_STATUS] && io_registers[IRQ0_ENABLE]) {
        io_registers[IRQ0_STATUS] = 0;
    }
    if (io_registers[IRQ1_STATUS] && io_registers[IRQ1_ENABLE]) {
        io_registers[IRQ1_STATUS] = 0;
    }
    if (io_registers[IRQ2_STATUS] && io_registers[IRQ2_ENABLE]) {
        io_registers[IRQ2_STATUS] = 0;
    }
}

/*
 * process_instruction:
 * ---------------------
 * Main ALU and control logic for each opcode. Modifies cpu_registers or PC as required.
 * Returns jump_flag=1 if the PC is changed by instruction itself, else 0.
 */
int process_instruction(int opcode, int *registersUsed, uint32_t *pc, int32_t imm1, int32_t imm2) {
    int jump_flag = 0; // 1 if we do a branch/jump

    switch (opcode) {
    case 0: // ADD
        cpu_registers[registersUsed[0]] = cpu_registers[registersUsed[1]]
                                          + cpu_registers[registersUsed[2]]
                                          + cpu_registers[registersUsed[3]];
        break;

    case 1: // SUB
        cpu_registers[registersUsed[0]] = cpu_registers[registersUsed[1]]
                                          - cpu_registers[registersUsed[2]]
                                          - cpu_registers[registersUsed[3]];
        break;

    case 2: // MAC
        cpu_registers[registersUsed[0]] = (cpu_registers[registersUsed[1]]
                                           * cpu_registers[registersUsed[2]])
                                          + cpu_registers[registersUsed[3]];
        break;

    case 3: // AND
        cpu_registers[registersUsed[0]] = cpu_registers[registersUsed[1]]
                                          & cpu_registers[registersUsed[2]]
                                          & cpu_registers[registersUsed[3]];
        break;

    case 4: // OR
        cpu_registers[registersUsed[0]] = cpu_registers[registersUsed[1]]
                                          | cpu_registers[registersUsed[2]]
                                          | cpu_registers[registersUsed[3]];
        break;

    case 5: // XOR
        cpu_registers[registersUsed[0]] = cpu_registers[registersUsed[1]]
                                          ^ cpu_registers[registersUsed[2]]
                                          ^ cpu_registers[registersUsed[3]];
        break;

    case 6: // SLL
        cpu_registers[registersUsed[0]] = cpu_registers[registersUsed[1]]
                                          << cpu_registers[registersUsed[2]];
        break;

    case 7: // SRA
        cpu_registers[registersUsed[0]] = (int32_t)cpu_registers[registersUsed[1]]
                                          >> cpu_registers[registersUsed[2]];
        break;

    case 8: // SRL
        cpu_registers[registersUsed[0]] = (uint32_t)cpu_registers[registersUsed[1]]
                                          >> cpu_registers[registersUsed[2]];
        break;

    case 9: // BEQ
        if (cpu_registers[registersUsed[1]] == cpu_registers[registersUsed[2]]) {
            *pc = cpu_registers[registersUsed[3]] & 0xFFF;
            jump_flag = 1;
        }
        break;

    case 10: // BNE
        if (cpu_registers[registersUsed[1]] != cpu_registers[registersUsed[2]]) {
            *pc = cpu_registers[registersUsed[3]] & 0xFFF;
            jump_flag = 1;
        }
        break;

    case 11: // BLT
        if ((int)cpu_registers[registersUsed[1]] < (int)cpu_registers[registersUsed[2]]) {
            *pc = cpu_registers[registersUsed[3]] & 0xFFF;
            jump_flag = 1;
        }
        break;

    case 12: // BGT
        if ((int)cpu_registers[registersUsed[1]] > (int)cpu_registers[registersUsed[2]]) {
            *pc = cpu_registers[registersUsed[3]] & 0xFFF;
            jump_flag = 1;
        }
        break;

    case 13: // BLE
        if ((int)cpu_registers[registersUsed[1]] <= (int)cpu_registers[registersUsed[2]]) {
            *pc = cpu_registers[registersUsed[3]] & 0xFFF;
            jump_flag = 1;
        }
        break;

    case 14: // BGE
        if ((int)cpu_registers[registersUsed[1]] >= (int)cpu_registers[registersUsed[2]]) {
            *pc = cpu_registers[registersUsed[3]] & 0xFFF;
            jump_flag = 1;
        }
        break;

    case 15: // JAL
        cpu_registers[registersUsed[0]] = *pc + 1; // Store return address
        *pc = cpu_registers[registersUsed[3]] & 0xFFF;
        jump_flag = 1;
        break;

    case 16: // LW
        cpu_registers[registersUsed[0]] = data_memory[cpu_registers[registersUsed[1]]
                                          + cpu_registers[registersUsed[2]]]
                                          + cpu_registers[registersUsed[3]];
        break;

    case 17: // SW
        data_memory[cpu_registers[registersUsed[1]] + cpu_registers[registersUsed[2]]] =
            cpu_registers[registersUsed[3]] + cpu_registers[registersUsed[0]];
        break;

    case 18: // RETI
        *pc = io_registers[IRQ_RETURN]; // Return from ISR
        break;

    case 19: // IN
        if (cpu_registers[registersUsed[1]] + cpu_registers[registersUsed[2]] == MONITOR_CMD) {
            // Reading from MONITOR_CMD always returns 0
            cpu_registers[registersUsed[0]] = 0;
        }
        else {
            cpu_registers[registersUsed[0]] =
                io_registers[cpu_registers[registersUsed[1]] + cpu_registers[registersUsed[2]]];
        }
        break;

    case 20: // OUT
        io_registers[cpu_registers[registersUsed[1]] + cpu_registers[registersUsed[2]]] =
            cpu_registers[registersUsed[3]];
        break;

    case 21: // HALT
        halt_flag = 1;
        break;

    default:
        fprintf(stderr, "Error: Unknown opcode %d\n", opcode);
        exit(EXIT_FAILURE);
    }

    // Make sure register $zero (index 0) is always 0
    cpu_registers[0] = 0;
    return jump_flag;
}

/*
 * load_input_files:
 * ------------------
 * Loads instruction memory, data memory, disk memory, and opens irq2_file for reading.
 * argv[]: command-line arguments with file names.
 * Returns true if successful, false otherwise.
 */
bool load_input_files(char *argv[]) {
    irq2_file = fopen(argv[4], "r");
    if (!irq2_file) {
        perror("Error opening irq2in file");
        return false;
    }

    // Load instruction, data, and disk from specified input files
    load_memory(argv[1], instruction_memory, MEM_SIZE, 12);
    load_memory32(argv[2], data_memory, MEM_SIZE);
    load_memory32(argv[3], disk_memory, DISK_SIZE);

    return true;
}

/*
 * open_output_files:
 * -------------------
 * Opens (for writing) all the required output files specified in argv[].
 * Returns true if all files could be opened, false otherwise.
 */
bool open_output_files(char *argv[]) {
    register_output_file = fopen(argv[6], "w");
    trace_file = fopen(argv[7], "w");
    hw_register_trace_file = fopen(argv[8], "w");
    cycle_count_file = fopen(argv[9], "w");
    led_output_file = fopen(argv[10], "w");
    seven_segment_output_file = fopen(argv[11], "w");
    disk_output_file = fopen(argv[12], "w");
    monitor_output_file = fopen(argv[13], "w");
    monitor_yuv_file = fopen(argv[14], "wb");

    // Return whether all are non-NULL
    return trace_file && hw_register_trace_file && cycle_count_file && led_output_file &&
           seven_segment_output_file && disk_output_file && monitor_output_file && monitor_yuv_file;
}

/*
 * write_output_files:
 * --------------------
 * Writes final states (data memory, disk memory, CPU registers, cycle count, monitor data).
 * argv[]: command-line arguments for file names.
 * Returns true if writing is successful.
 */
bool write_output_files(char *argv[]) {
    // Save data memory
    save_memory(argv[5], data_memory, MEM_SIZE);
    // Save disk memory
    save_memory(argv[12], disk_memory, DISK_SIZE);

    // Write CPU register values (indices 3..15)
    for (int i = 3; i < NUM_CPU_REGS; i++) {
        fprintf(register_output_file, "%08X\n", cpu_registers[i]);
    }

    // Write cycle count to file
    fprintf(cycle_count_file, "%d\n", io_registers[CLOCK_CYCLE]);

    // Write monitor buffer in both text and YUV formats
    write_monitor_data(monitor_output_file, monitor_yuv_file);

    return true;
}

/*
 * cleanup_files:
 * ---------------
 * Closes all file pointers used during the simulation.
 */
void cleanup_files() {
    fclose(trace_file);
    fclose(hw_register_trace_file);
    fclose(cycle_count_file);
    fclose(led_output_file);
    fclose(seven_segment_output_file);
    fclose(disk_output_file);
    fclose(monitor_output_file);
    fclose(monitor_yuv_file);
    fclose(irq2_file);
}

/*
 * get_instruction:
 * -----------------
 * Fetches instruction from instruction_memory at 'program_counter'.
 * If the disk is busy (DISK_STATUS == 1) and CPU is halted (halt_flag == 1),
 * it tries to re-fetch the same instruction. Otherwise, it just returns
 * the instruction at 'program_counter'.
 */
uint64_t get_instruction()
{
    if(io_registers[DISK_STATUS] == 1 && halt_flag == 1) {
        // Return an instruction but effectively stall by decrementing PC
        uint64_t inst = instruction_memory[program_counter];
        program_counter -= 1;
        return instruction_memory[program_counter];
    }
    else {
        return instruction_memory[program_counter];
    }
}

/*
 * execute_simulation_loop:
 * -------------------------
 * Main CPU execution loop. Continues until we see HALT + disk free.
 *  - Reads IRQ2 events at the right clock cycles
 *  - Fetches and decodes instructions
 *  - Logs trace
 *  - Executes instruction
 *  - Handles peripherals
 *  - Manages interrupts
 *  - Increments clock cycle, updates timer
 */
void execute_simulation_loop() {
    io_registers[TIMER_MAX] = 0xFFFFFFFF; // Initialize timer max
    int interrupt_flag = 0;              // Tracks if an interrupt is triggered
    char input_line[255];

    // Load next IRQ2 event into irq2_next_cycle
    irq2_next_cycle = read_next_irq(fgets(input_line, sizeof(input_line), irq2_file));

    // Continue running until CPU is halted AND disk is idle
    while (!(halt_flag == 1 && io_registers[DISK_STATUS] == 0)) {
        // Check if it's time for an IRQ2 event
        if (irq2_next_cycle == io_registers[CLOCK_CYCLE]) {
            irq2_next_cycle = read_next_irq(fgets(input_line, sizeof(input_line), irq2_file));
            io_registers[IRQ2_STATUS] = 1; // Trigger IRQ2 status
        }

        // Fetch instruction from memory
        uint64_t current_instruction = get_instruction();

        // Extract opcode (top 8 bits)
        int opcode = current_instruction >> 40;

        // Decode the rest of the 48-bit instruction
        int operand_registers[4] = { 0 };
        uint32_t immediate_value = decode_instruction(current_instruction, operand_registers);
        // Split 24-bit immediate into two 12-bit parts
        int32_t immediate1 = sign_extend((immediate_value & 0xFFF000) >> 12, 12);
        int32_t immediate2 = sign_extend(immediate_value & 0xFFF, 12);

        // Write immediate1 and immediate2 to $imm1 and $imm2 registers (indexes 1 and 2)
        cpu_registers[1] = immediate1;
        cpu_registers[2] = immediate2;

        // Log instruction trace to file
        log_instruction_trace(program_counter, current_instruction, cpu_registers);

        // Execute the current instruction, possibly modifying program_counter
        if (!process_instruction(opcode, operand_registers, &program_counter, immediate1, immediate2)) {
            program_counter++; // If no jump/branch occurred, move to next
        }

        // Update peripherals (disk, timer, displays, etc.)
        handle_peripherals(opcode, operand_registers);

        // Check for RETI (ends ISR) or pending interrupts
        if (opcode == RETI_OP) {
            isr_active_flag = 0;
        }
        if (!isr_active_flag) {
            interrupt_flag = check_interrupts();
            if (interrupt_flag) {
                // Save return address as PC-1
                io_registers[IRQ_RETURN] = program_counter - 1;
                // Jump to ISR
                program_counter = io_registers[IRQ_HANDLER];
                isr_active_flag = 1;
            }
        }

        // Increment clock
        increment_clock_cycle(io_registers);
        // Update timer
        update_timer(io_registers);

        // Reset monitor command after use
        if (io_registers[MONITOR_CMD] == 1)
            io_registers[MONITOR_CMD] = 0;
    }
}

/*
 * main:
 * ------
 * Entry point. Expects 15 arguments for file input/output.
 *  1) imemin.txt
 *  2) dmemin.txt
 *  3) diskin.txt
 *  4) irq2in.txt
 *  5) dmemout.txt
 *  6) regout.txt
 *  7) trace.txt
 *  8) hwregtrace.txt
 *  9) cycles.txt
 * 10) leds.txt
 * 11) display7seg.txt
 * 12) diskout.txt
 * 13) monitor.txt
 * 14) monitor.yuv
 */
int main(int argc, char *argv[]) {
    // Check argument count
    if (argc != 15) {
        fprintf(stderr, "Usage: %s <imemin.txt> <dmemin.txt> <diskin.txt> <irq2in.txt> <dmemout.txt> "
                        "<regout.txt> <trace.txt> <hwregtrace.txt> <cycles.txt> <leds.txt> "
                        "<display7seg.txt> <diskout.txt> <monitor.txt> <monitor.yuv>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Load input files (instruction, data, disk, irq2)
    if (!load_input_files(argv)) {
        fprintf(stderr, "Error loading input files. Exiting.\n");
        return EXIT_FAILURE;
    }

    // Open output files for writing
    if (!open_output_files(argv)) {
        fprintf(stderr, "Error opening output files. Exiting.\n");
        return EXIT_FAILURE;
    }

    // Run the main simulation
    execute_simulation_loop();

    // Write all final data to the respective output files
    if (!write_output_files(argv)) {
        fprintf(stderr, "Error writing output files. Exiting.\n");
        return EXIT_FAILURE;
    }

    // Close all file pointers
    cleanup_files();
    return EXIT_SUCCESS;
}
