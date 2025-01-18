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

// needed for running the model and/or initializing inference setup
#include "sww_model.h"
#include "sww_model_data.h"
#include "model_test_inputs.h"


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
	else if(strcmp(cmd_args[0], "run_model") == 0) {
		run_model(cmd_args);
	}
	else {
		printf("Unrecognized command %s, with arguments %s\r\n", cmd_args[0], full_command);
	}
}


/* Global handle to reference the instantiated C-model */
static ai_handle sww_model = AI_HANDLE_NULL;

/* Global c-array to handle the activations buffer */
AI_ALIGNED(32)
static ai_i8 activations[AI_SWW_MODEL_DATA_ACTIVATIONS_SIZE];

/* Array to store the data of the input tensor */
AI_ALIGNED(32)
static ai_i8 in_data[AI_SWW_MODEL_IN_1_SIZE];
/* or static ai_i8 in_data[AI_SWW_MODEL_DATA_IN_1_SIZE_BYTES]; */

/* c-array to store the data of the output tensor */
AI_ALIGNED(32)
static ai_i8 out_data[AI_SWW_MODEL_OUT_1_SIZE];
/* static ai_i8 out_data[AI_SWW_MODEL_DATA_OUT_1_SIZE_BYTES]; */

/* Array of pointer to manage the model's input/output tensors */
static ai_buffer *ai_input;
static ai_buffer *ai_output;


/*
 * Bootstrap inference framework
 */
int aiInit(void) {
  ai_error err;

  /* Create and initialize the c-model */
  const ai_handle acts[] = { activations };
  err = ai_sww_model_create_and_init(&sww_model, acts, NULL);

  if (err.type != AI_ERROR_NONE) {
	  ;
  };

  /* Reteive pointers to the model's input/output tensors */
  ai_input = ai_sww_model_inputs_get(sww_model, NULL);
  ai_output = ai_sww_model_outputs_get(sww_model, NULL);

  return 0;
}



/*
 * Run inference
 */
int aiRun(const void *in_data, void *out_data) {
  ai_i32 n_batch;
  ai_error err;

  /* 1 - Update IO handlers with the data payload */
  ai_input[0].data = AI_HANDLE_PTR(in_data);
  ai_output[0].data = AI_HANDLE_PTR(out_data);

  /* 2 - Perform the inference */
  n_batch = ai_sww_model_run(sww_model, &ai_input[0], &ai_output[0]);
  if (n_batch != 1) {
      err = ai_sww_model_get_error(sww_model);

  };

  return 0;
}

void run_model(char *cmd_args[]) {
//	acquire_and_process_data(in_data);
	const int8_t *input_source=NULL;

	printf("In run_model. about to run model\r\n");
	if (strcmp(cmd_args[1], "class0") == 0) {
		input_source = test_input_class0;
	}
	else if (strcmp(cmd_args[1], "class1") == 0) {
		input_source = test_input_class1;
	}
	else if (strcmp(cmd_args[1], "class2") == 0) {
		input_source = test_input_class2;
	}
	else {
		printf("Unknown input tensor name, defaulting to test_input_class0\r\n");
		input_source = test_input_class0;
	}


	for(int i=0;i<AI_SWW_MODEL_IN_1_SIZE;i++){
		in_data[i] = (ai_i8)input_source[i];
	}
	/*  Call inference engine */
	aiRun(in_data, out_data);
	printf("Output = [");
	for(int i=0;i<AI_SWW_MODEL_OUT_1_SIZE;i++){
		printf("%02d, ", out_data[i]);
	}
	printf("]\r\n");
}
