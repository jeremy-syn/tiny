/*
 * sww_util.c
 *
 *  Created on: Jan 16, 2025
 *      Author: jeremy
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include "sww_util.h"

#define  MAX_CMD_TOKENS 8 // maximum number of tokens in a command, including the command and arguments
// Command buffer (incoming commands from host)
char g_cmd_buf[EE_CMD_SIZE + 1];
size_t g_cmd_pos = 0u;


void print_vals_int16(int16_t *buffer, uint32_t num_vals)
{
	const int vals_per_line = 16;
	printf("[");
	for(uint32_t i=0;i<num_vals;i+= vals_per_line)
	{
		for(int j=0;j<vals_per_line;j++)
		{
			if(i+j >= num_vals)
			{
				break;
			}
			printf("%d, ", buffer[i+j]);
		}
		printf("\r\n");
	}
	printf("]\r\n==== Done ====\r\n");
}

void print_bytes(uint8_t *buffer, uint32_t num_bytes)
{
	const int vals_per_line = 16;
	printf("[");
	for(uint32_t i=0;i<num_bytes;i+= vals_per_line)
	{
		for(int j=0;j<vals_per_line;j++)
		{
			if(i+j >= num_bytes)
			{
				break;
			}
			printf("0x%X, ", buffer[i+j]);
		}
		printf("\r\n");
	}
	printf("]\r\n==== Done ====\r\n");
}


void print_vals_float(float *buffer, uint32_t num_vals)
{
	const int vals_per_line = 8;
	printf("[");
	for(uint32_t i=0;i<num_vals;i+= vals_per_line)
	{
		for(int j=0;j<vals_per_line;j++)
		{
			if(i+j >= num_vals)
			{
				break;
			}
			printf("%3.4f, ", buffer[i+j]);
		}
		printf("\r\n");
	}
	printf("]\r\n==== Done ====\r\n");
}
void log_printf(LogBuffer *log, const char *format, ...) {
    va_list args;
    char temp_buffer[LOG_BUFFER_SIZE];
    int written;

    // Initialize the variable argument list
    va_start(args, format);

    // Write formatted output to a temporary buffer
    written = vsnprintf(temp_buffer, sizeof(temp_buffer), format, args);

    // End the variable argument list
    va_end(args);

    // Check if the formatted string fits in the remaining buffer
    if (log->current_pos + written >= LOG_BUFFER_SIZE) {
        // Buffer overflow: Zero out and reset to the beginning
        memset(log->buffer, 0, LOG_BUFFER_SIZE);
        log->current_pos = 0;
    }

    // Copy the formatted string to the log buffer
    if (written > 0) {
        size_t bytes_to_copy = (written < LOG_BUFFER_SIZE) ? written : LOG_BUFFER_SIZE - 1;
        strncpy(&log->buffer[log->current_pos], temp_buffer, bytes_to_copy);
        log->current_pos += bytes_to_copy;
    }
}


/**
 * This function assembles a command string from the UART. It should be called
 * from the UART ISR for each new character received. When the parser sees the
 * termination character, the user-defined th_command_ready() command is called.
 * It is up to the application to then dispatch this command outside the ISR
 * as soon as possible by calling ee_serial_command_parser_callback(), below.
 */
void ee_serial_callback(char c) {
  if (c == EE_CMD_TERMINATOR) {
    g_cmd_buf[g_cmd_pos] = (char)0;
    process_command(g_cmd_buf);
    g_cmd_pos = 0;
  } else {
    g_cmd_buf[g_cmd_pos] = c;
    g_cmd_pos = g_cmd_pos >= EE_CMD_SIZE ? EE_CMD_SIZE : g_cmd_pos + 1;
  }
}


void process_command(char *full_command) {

	char *cmd_args[MAX_CMD_TOKENS] = {NULL};

    printf("Full command: %s\r\n", full_command);

    char* token = strtok(full_command, " ");
    cmd_args[0] = token;

    for(int i=1;i<MAX_CMD_TOKENS;i++) {
        cmd_args[i] = strtok(NULL, " ");
        if(cmd_args[i] == NULL)
            break;
    }
    for(int i=0;i<MAX_CMD_TOKENS && cmd_args[i] != NULL;i++) {
        printf("[%d]: %p=>%s\r\n", i, (void *)cmd_args[i], cmd_args[i]);
    }

	// full_command should be "<command> <arg1> <arg2>" (command and args delimited by spaces)
	// put the command and arguments into the array cmd_arg[]
	if (strcmp(cmd_args[0], "name") == 0) {
		printf("streaming wakeword test platform\r\n");
	}
	// else if() {}
	else {
		printf("Unrecognized command %s, with arguments %s\r\n", cmd_args[0], full_command);
	}
}

//void run_model(char *cmd_args[]) {
////	acquire_and_process_data(in_data);
//	printf("In while loop. about to run model\r\n");
//	for(int i=0;i<AI_SWW_MODEL_IN_1_SIZE;i++){
//		in_data[i] = (ai_i8)test_input_class2[i];
//	}
//	/* 2 - Call inference engine */
//	aiRun(in_data, out_data);
//	printf("Output = [");
//	for(int i=0;i<AI_SWW_MODEL_OUT_1_SIZE;i++){
//		printf("%02d, ", out_data[i]);
//	}
//	printf("]\r\n");
//}
