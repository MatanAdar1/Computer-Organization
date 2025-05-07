// Disable secure warnings on Windows (allows use of functions like 'strcpy' without warnings).
#define _CRT_SECURE_NO_WARNINGS

// Standard C library includes.
#include <stdio.h>      // For input/output functions (fopen, fprintf, fscanf, etc.)
#include <stdlib.h>     // For general utilities (exit, malloc, atoi, etc.)
#include <string.h>     // For string manipulation functions (strcpy, strcmp, strchr, etc.)
#include <stdint.h>     // For fixed-width integer types (uint32_t, uint64_t, etc.)
#include <ctype.h>      // For character classification (isspace, isdigit, etc.)
#include <stdbool.h>    // For boolean type (_Bool in C)

// Define constants for maximum line length, maximum number of instruction lines, and instruction width.
#define MAX_LINE_LEN 256
#define MAX_INSTRUCTION_LINES 4100
#define INSTRUCTION_WIDTH 48

// Structure to hold label information: the label string and its corresponding address.
typedef struct {
	char label[MAX_LINE_LEN]; // Label name
	int address;              // Address (PC value) where the label is located
}Label;

// Declare a global array of labels and its current size.
Label label_list[MAX_INSTRUCTION_LINES];
int label_list_size = 0;

// Declare an array to hold data memory. Each index corresponds to a memory address.
uint32_t data_list[MAX_INSTRUCTION_LINES] = { 0 };

/*
 * get_line:
 * -----------
 *  Takes a string line (assembly code), removes leading/trailing spaces, handles comments (removes part after '#'),
 *  replaces commas with spaces, and reduces multiple spaces to a single space.
 *  Returns a pointer to the modified string.
 */
char* get_line(char* str) {
	// Remove leading spaces
	while (isspace((unsigned char)*str)) str++;
	if (*str == 0) return str; // If the string is empty after trimming

	// Remove comments (everything after '#')
	char* hash_pos = strchr(str, '#');
	if (hash_pos) {
		*hash_pos = '\0'; // Null-terminate the string at the '#' character
	}

	// Remove trailing spaces
	int end_idx = strlen(str); // End of string index
	while (end_idx > 1 && isspace(str[end_idx - 1]))
		end_idx--;
	str[end_idx] = '\0'; // Null terminate the string

	// Removing ',' characters by replacing them with spaces
	for (int i = 0; str[i] != '\0'; i++)
	{
		if (str[i] == ',')
			str[i] = ' ';
	}

	// Reduce multiple spaces in between parameters to a single space
	int read_index = 0;
	int write_index = 0;
	bool space_flag = false; // Flag to track if we're in a sequence of spaces
	while (str[read_index] != '\0') {
		if (isspace(str[read_index])) {
			if (!space_flag) {
				str[write_index] = ' '; // Write a single space
				space_flag = true;      // Set the flag indicating we're in a space sequence
				write_index++;
			}
		}
		else {
			str[write_index] = str[read_index]; // Write the non-space character
			write_index++;                      // Advance write index
			space_flag = false;                 // Reset the space flag
		}
		read_index++; // Move to the next character
	}
	str[write_index] = '\0'; // Null-terminate the final string
	return str;
}

/*
 * addLabel:
 * ----------
 *  Adds a label and its address to the global label_list.
 *  label: name of the label.
 *  address: the PC address corresponding to this label.
 */
void addLabel(char* label, int address)
{
	strcpy(label_list[label_list_size].label, label);
	label_list[label_list_size].address = address;
	label_list_size++;
}

/*
 * get_opcode:
 * -------------
 *  Takes an opcode string and returns the corresponding numeric code (instruction_code).
 *  If the opcode is invalid, it prints an error message and exits.
 */
int get_opcode(char* opcode)
{
	int instruction_code;
	if (strcmp(opcode, "add") == 0) instruction_code = 0;
	else if (strcmp(opcode, "sub") == 0) instruction_code = 1;
	else if (strcmp(opcode, "mac") == 0) instruction_code = 2;
	else if (strcmp(opcode, "and") == 0) instruction_code = 3;
	else if (strcmp(opcode, "or") == 0) instruction_code = 4;
	else if (strcmp(opcode, "xor") == 0) instruction_code = 5;
	else if (strcmp(opcode, "sll") == 0) instruction_code = 6;
	else if (strcmp(opcode, "sra") == 0) instruction_code = 7;
	else if (strcmp(opcode, "srl") == 0) instruction_code = 8;
	else if (strcmp(opcode, "beq") == 0) instruction_code = 9;
	else if (strcmp(opcode, "bne") == 0) instruction_code = 10;
	else if (strcmp(opcode, "blt") == 0) instruction_code = 11;
	else if (strcmp(opcode, "bgt") == 0) instruction_code = 12;
	else if (strcmp(opcode, "ble") == 0) instruction_code = 13;
	else if (strcmp(opcode, "bge") == 0) instruction_code = 14;
	else if (strcmp(opcode, "jal") == 0) instruction_code = 15;
	else if (strcmp(opcode, "lw") == 0) instruction_code = 16;
	else if (strcmp(opcode, "sw") == 0) instruction_code = 17;
	else if (strcmp(opcode, "reti") == 0) instruction_code = 18;
	else if (strcmp(opcode, "in") == 0) instruction_code = 19;
	else if (strcmp(opcode, "out") == 0) instruction_code = 20;
	else if (strcmp(opcode, "halt") == 0) instruction_code = 21;
	else {
		fprintf(stderr, "Error: Invalid opcode '%s'\n", opcode);
		exit(EXIT_FAILURE);
	}
	return instruction_code;
}

/*
 * get_label_address:
 * -------------------
 *  Looks up a label in the global label_list and returns the address if found.
 *  If the label is not defined, it prints an error and exits.
 */
int get_label_address(const char* label_target)
{
	for (int i = 0; i < label_list_size; i++)
	{
		if (strcmp(label_list[i].label, label_target) == 0)
			return label_list[i].address;
	}
	fprintf(stderr, "Error: Undefined label '%s'\n", label_target);
	exit(EXIT_FAILURE);
}

/*
 * isNumber:
 * ----------
 *  Checks if a given string represents a valid integer (including negative values).
 *  Returns true if it is a number, false otherwise.
 */
bool isNumber(const char* str)
{
	if (str[0] == '\0') // Empty string is not a number.
		return false;
	for (int i = 0; str[i] != '\0'; i++)
	{
		if (i == 0 && str[0] == '-')
			continue; // Allow negative sign at the beginning
		else if (!isdigit(str[i]))
			return false; // If any character is not a digit, return false.
	}
	return true; // All characters are digits (or an optional leading '-')
}

/*
 * get_immidiate_value:
 * ----------------------
 *  Converts a string into an integer value. The string can represent:
 *    - A label (to be resolved to its address).
 *    - A decimal number.
 *    - A hexadecimal number (prefixed with "0x" or "0X").
 *  Returns the integer value.
 */
int get_immidiate_value(const char *str) {
	if (!isNumber(str) && !(str[0] == '0' && (str[1] == 'X' || str[1] == 'x'))) {
		// If it's not strictly numeric or hex, treat it as a label
		return get_label_address(str);
	}
	else if (str[0] == '0' && (str[1] == 'X' || str[1] == 'x')) {
		// Hexadecimal conversion
		return strtol(str, NULL, 16);
	}
	else {
		// Decimal conversion
		return atoi(str);
	}
}

/*
 * get_reg_code:
 * --------------
 *  Converts a register string (e.g. "$zero", "$a0", "$ra") to its corresponding register code (0-15).
 *  If the register is invalid, it prints an error message and exits.
 */
int get_reg_code(const char* reg_str)
{
	int register_code = -1;

	// Check for special registers
	if (strcmp(reg_str, "$zero") == 0) register_code = 0;
	else if (strcmp(reg_str, "$imm1") == 0) register_code = 1;
	else if (strcmp(reg_str, "$imm2") == 0) register_code = 2;
	else if (strcmp(reg_str, "$v0") == 0) register_code = 3;

	// Check for general registers
	else if (reg_str[0] == '$')
	{
		if (reg_str[1] == 'a')
		{
			// $a0, $a1, $a2
			if (strcmp(&reg_str[2], "0") == 0 || strcmp(&reg_str[2], "1") == 0 || strcmp(&reg_str[2], "2") == 0)
				register_code = 4 + atoi(&reg_str[2]);
		}
		else if (reg_str[1] == 't')
		{
			// $t0, $t1, $t2
			if (strcmp(&reg_str[2], "0") == 0 || strcmp(&reg_str[2], "1") == 0 || strcmp(&reg_str[2], "2") == 0)
				register_code = 7 + atoi(&reg_str[2]);
		}
		else if (reg_str[1] == 's')
		{
			// $s0, $s1, $s2
			if (strcmp(&reg_str[2], "0") == 0 || strcmp(&reg_str[2], "1") == 0 || strcmp(&reg_str[2], "2") == 0)
				register_code = 10 + atoi(&reg_str[2]);
		}
	}

	// Check for more special registers
	if (strcmp(reg_str, "$gp") == 0) register_code = 13;
	else if (strcmp(reg_str, "$sp") == 0) register_code = 14;
	else if (strcmp(reg_str, "$ra") == 0) register_code = 15;

	// If invalid register, print error and exit
	if (register_code != -1)
		return register_code;
	else {
		fprintf(stderr, "Error: Invalid register '%s'\n", reg_str);
		exit(EXIT_FAILURE);
	}
}

/*
 * encodeInstruction:
 * -------------------
 *  Encodes an instruction given its components (opcode, registers, and immediates)
 *  into a 48-bit format (actually packed in a 64-bit variable).
 *
 *  The format is (from MSB to LSB):
 *     [ 8 bits opcode | 4 bits rd | 4 bits rs | 4 bits rt | 4 bits rm | 12 bits imm1 | 12 bits imm2 ]
 *  Returns the 64-bit encoded instruction.
 */
uint64_t encodeInstruction(int opcode, int rd, int rs, int rt, int rm, int imm1, int imm2)
{
	uint64_t instruction_bits_rep = 0;

	// Start from opcode (8 bits)
	instruction_bits_rep = (opcode & 0XFF);
	instruction_bits_rep = instruction_bits_rep << 4;          // Shift to make room for rd (4 bits)
	instruction_bits_rep = instruction_bits_rep | (rd & 0XF);
	instruction_bits_rep = instruction_bits_rep << 4;          // Shift to make room for rs
	instruction_bits_rep = instruction_bits_rep | (rs & 0XF);
	instruction_bits_rep = instruction_bits_rep << 4;          // Shift to make room for rt
	instruction_bits_rep = instruction_bits_rep | (rt & 0XF);
	instruction_bits_rep = instruction_bits_rep << 4;          // Shift to make room for rm
	instruction_bits_rep = instruction_bits_rep | (rm & 0XF);
	instruction_bits_rep = instruction_bits_rep << (4 * 3);    // Shift to make room for imm1 (12 bits)
	instruction_bits_rep = instruction_bits_rep | (imm1 & 0XFFF);
	instruction_bits_rep = instruction_bits_rep << (4 * 3);    // Shift to make room for imm2 (12 bits)
	instruction_bits_rep = instruction_bits_rep | (imm2 & 0XFFF);

	return instruction_bits_rep;
}

/*
 * assemble:
 * ----------
 *  Main function that processes the assembly file to produce two output files:
 *    1) instructionFile: contains the machine code for instructions.
 *    2) dataFile: contains the data memory initialization.
 *
 *  It performs two passes:
 *    - First Pass: read each line, extract labels, and store them with their addresses.
 *    - Second Pass: encode instructions and handle '.word' directives to fill data memory.
 *
 *  inputFile: the input assembly file name
 *  instructionFile: name of the file to write encoded instructions
 *  dataFile: name of the file to write data memory contents
 */
void assemble(const char* inputFile, const char* instructionFile, const char* dataFile)
{
	// Open input assembly file for reading
	FILE* PtrInstruction_In = fopen(inputFile, "r");
	if (PtrInstruction_In == NULL)
	{
		perror("Error opening instruction file");
		exit(EXIT_FAILURE);
	}

	// Open file for writing instructions
	FILE* PtrInstruction_Out = fopen(instructionFile, "w");
	if (PtrInstruction_Out == NULL)
	{
		perror("Error opening instrcuion file");
		fclose(PtrInstruction_In);
		exit(EXIT_FAILURE);
	}

	// Open file for writing data
	FILE* PtrDataOut = fopen(dataFile, "w");
	if (PtrDataOut == NULL)
	{
		perror("Error opening data file");
		fclose(PtrInstruction_In);
		fclose(PtrInstruction_Out);
		exit(EXIT_FAILURE);
	}

	// Buffer to read lines from inputFile
	char line[MAX_INSTRUCTION_LINES];
	int pc = 0;  // Program Counter to track instruction addresses

	// --------------------------
	// First Pass: Label parsing
	// --------------------------
	while (fgets(line, sizeof(line), PtrInstruction_In)) {
		// Process line to standard form (remove comments, spaces, etc.)
		char *standard_instruction_line = get_line(line);

		// Skip empty lines or lines that are now comments
		if (standard_instruction_line[0] == '#' || strlen(standard_instruction_line) == 0) 
			continue;

		// Check if this line has a label (indicated by ':')
		char* label_end_position = strchr(standard_instruction_line, ':');
		if (label_end_position != NULL)
		{
			// Terminate string at the colon to isolate label name
			*label_end_position = '\0';
			// Add the label to the global list along with current pc
			addLabel(standard_instruction_line, pc);
		}
		else if (strncmp(standard_instruction_line, ".word", 5) != 0)
			// If it's not a '.word' directive, increment pc (instruction line)
			pc++;
	}

	// Reset to beginning of file for second pass
	rewind(PtrInstruction_In);
	pc = 0; // Reset program counter
	int max_memory_address = 0; // Track highest data memory address used by '.word'

	// -----------------------------
	// Second Pass: Encode Instructions
	// -----------------------------
	while (fgets(line, sizeof(line), PtrInstruction_In)) {
		// Standardize the line again
		char* standard_instruction_line = get_line(line);

		// Skip empty or comment lines
		if (standard_instruction_line[0] == '#' || strlen(standard_instruction_line) == 0) 
			continue;

		// Skip lines that only contain a label definition (already handled in pass 1)
		char* label_end_ptr = strchr(standard_instruction_line, ':');
		if (label_end_ptr != NULL) 
			continue;
		if (strlen(standard_instruction_line) == 0) 
			continue;

		// Check if line is a data directive '.word'
		if (strncmp(standard_instruction_line, ".word", 5) == 0) {
			int wordAddress;
			uint32_t wordData;

			// Parse the address and the data value from the line
			if (sscanf(standard_instruction_line + 6, "%i %i", &wordAddress, &wordData) != 2) {
				fprintf(stderr, "Error: Invalid .word directive '%s'\n", standard_instruction_line);
				exit(EXIT_FAILURE);
			}

			// Check if memory address is already used
			if (data_list[wordAddress] != 0) {
				fprintf(stderr, "Error: Memory address %d already defined\n", wordAddress);
				exit(EXIT_FAILURE);
			}

			// Store data in data_list at the specified address
			data_list[wordAddress] = wordData;

			// Update the maximum used memory address
			if (wordAddress > max_memory_address)
				max_memory_address = wordAddress;
		}
		else {
			// It's an instruction line. We'll parse and encode it.
			char opcode[20], rd[20], rs[20], rt[20], rm[20], imm1[20], imm2[20];

			// Read up to 7 tokens (opcode rd rs rt rm imm1 imm2)
			if (sscanf(standard_instruction_line, "%s %s %s %s %s %s %s", 
				opcode, rd, rs, rt, rm, imm1, imm2) < 0)
			{
				fprintf(stderr, "Error: Invalid instruciton format'%s'\n", standard_instruction_line);
				exit(EXIT_FAILURE);
			}

			// Convert imm1 and imm2 strings to integer values (they may be labels, decimal, or hex)
			int imm1_int = get_immidiate_value(imm1), imm2_int = get_immidiate_value(imm2);

			// Encode the instruction into a 48-bit format
			uint64_t instruction = encodeInstruction(
				get_opcode(opcode),
				get_reg_code(rd),
				get_reg_code(rs),
				get_reg_code(rt),
				get_reg_code(rm),
				imm1_int,
				imm2_int
			);

			// Write the encoded instruction as a 12-hex-digit string to the instruction file
			fprintf(PtrInstruction_Out, "%012llX\n", instruction);

			// Increment program counter for the next instruction
			pc++;
		}
	}

	// After processing all lines, output the data memory contents to dataFile
	for (int i = 0; i <= max_memory_address; i++)
	{
		fprintf(PtrDataOut, "%08X\n", data_list[i]);
	}

	// Close all opened files
	fclose(PtrInstruction_In);
	fclose(PtrInstruction_Out);
	fclose(PtrDataOut);
}

/*
 * main:
 * ------
 *  Expects three arguments:
 *    - input file: assembly source code
 *    - instruction memory file: output file for encoded instructions
 *    - data memory file: output file for initialized data
 *
 *  The main function simply calls 'assemble' with these parameters.
 */
int main(int argc, char* argv[])
{
	if (argc != 4) {
		fprintf(stderr, "Usage: %s <input file> <instruction memory file> <data memory file>\n", argv[0]);
		return EXIT_FAILURE;
	}
	assemble(argv[1], argv[2], argv[3]);
	return EXIT_SUCCESS;
}
