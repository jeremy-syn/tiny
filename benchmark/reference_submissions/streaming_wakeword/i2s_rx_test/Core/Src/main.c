/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

#define LOG_BUFFER_SIZE 4096
typedef struct {
    char buffer[LOG_BUFFER_SIZE];
    size_t current_pos;
} LogBuffer;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef hlpuart1;
UART_HandleTypeDef huart3;

SAI_HandleTypeDef hsai_BlockA1;
DMA_HandleTypeDef hdma_sai1_a;

PCD_HandleTypeDef hpcd_USB_OTG_FS;

/* USER CODE BEGIN PV */

uint32_t g_int16s_read = 0;
uint32_t g_i2s_chunk_size_bytes = 1024;
uint32_t g_i2s_status = HAL_OK;
// two ping-pong byte buffers for DMA transfers from I2S port.
uint8_t *g_i2s_buffer0 = NULL;
uint8_t *g_i2s_buffer1 = NULL;
uint8_t *g_i2s_current_buff = NULL; // will be either g_i2s_buffer0 or g_i2s_buffer1
int g_i2s_buff_sel = 0;  // 0 for buffer0, 1 for buffer1
int16_t *g_wav_record = NULL;  // buffer to store complete waveform
uint32_t g_i2s_wav_len = 32*512; // length in (16b) samples
int g_i2s_rx_in_progess = 0;
LogBuffer g_log = { .buffer = {0}, .current_pos = 0 };

int num_calls=0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_LPUART1_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USB_OTG_FS_PCD_Init(void);
static void MX_SAI1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// These defines and this function are to get printf() working
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE
{
  HAL_UART_Transmit(&hlpuart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}

void print_vals_int16(const int16_t *buffer, uint32_t num_vals)
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

void print_bytes(const uint8_t *buffer, uint32_t num_bytes)
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

void ErrorHandler(HAL_StatusTypeDef returned_status) {
	printf("Error occured: %d\r\n", returned_status);
}


void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai) {

	log_printf(&g_log, "<beg>w0=%d\r\n", g_wav_record[0]);
	int reading_complete=0;

	num_calls += 1;

	g_int16s_read += g_i2s_chunk_size_bytes/2;

	// idle_buffer is the one that will be idle after we switch
	uint8_t* idle_buffer = g_i2s_buff_sel ? g_i2s_buffer1 : g_i2s_buffer0;
	g_i2s_buff_sel = g_i2s_buff_sel ^ 1; // toggle between 0/1 => g_i2s_buffer0/1
    g_i2s_current_buff = g_i2s_buff_sel ? g_i2s_buffer1 : g_i2s_buffer0;

	if(g_int16s_read + g_i2s_chunk_size_bytes/2 <= g_i2s_wav_len){
		// there is space left for a full chunk
		g_i2s_status = HAL_SAI_Receive_DMA(hsai, g_i2s_current_buff, g_i2s_chunk_size_bytes/2);
	}
	else {
		// if there is only space for a partial read
		// i.e. (g_int16s_read < g_i2s_wav_len < g_int16s_read + g_i2s_chunk_size_bytes/2)
		// don't start the read, b/c you'll overflow the allocated buffer
		// that means you'll read less than requested, but avoid a seg-fault.
		reading_complete = 1;
	}

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);

    // for 1024 bytes, this memcpy takes about 50 us.
    //
	memcpy((uint8_t*)(g_wav_record+g_int16s_read-g_i2s_chunk_size_bytes/2), idle_buffer, g_i2s_chunk_size_bytes);

	// This block just for debug.
	//	uint8_t *p_bytes=NULL;

	//    p_bytes = (uint8_t*)(g_wav_record+g_int16s_read);
	log_printf(&g_log, "%d calls to RxCb\r\n", num_calls);
    log_printf(&g_log, "cb:%lu,b%d,rs=%d,st=%d.\r\n", g_int16s_read, g_i2s_buff_sel, g_i2s_status, hsai->State);
    //  log_printf(&g_log, "\t[%8X] [0x%02X, 0x%02X, 0x%02X, 0x%02X]\r\n",p_bytes, p_bytes[0], p_bytes[1], p_bytes[2], p_bytes[3]);

	int16_t *p_int16s=(int16_t*)(g_wav_record);
    log_printf(&g_log, "W0:\t[%8X] <= [%d, %d, %d, %d, %d, %d, %d, %d]\r\n",p_int16s,
        		p_int16s[0], p_int16s[1], p_int16s[2], p_int16s[3], p_int16s[4], p_int16s[5], p_int16s[6], p_int16s[7]);

    p_int16s=(int16_t*)(g_wav_record+g_int16s_read - g_i2s_chunk_size_bytes/2);
    log_printf(&g_log, "WV:\t[%8X] <= [%d, %d, %d, %d, %d, %d, %d, %d]\r\n",p_int16s,
    		p_int16s[0], p_int16s[1], p_int16s[2], p_int16s[3], p_int16s[4], p_int16s[5], p_int16s[6], p_int16s[7]);

    p_int16s=(int16_t*)g_i2s_buffer0;
    log_printf(&g_log, "B0\t[%8X] <= [%d, %d, %d, %d, %d, %d, %d, %d]\r\n",p_int16s,
    		p_int16s[0], p_int16s[1], p_int16s[2], p_int16s[3], p_int16s[4], p_int16s[5], p_int16s[6], p_int16s[7]);

    p_int16s=(int16_t*)g_i2s_buffer1;
    log_printf(&g_log, "B1\t[%8X] <= [%d, %d, %d, %d, %d, %d, %d, %d]\r\n",p_int16s,
    		p_int16s[0], p_int16s[1], p_int16s[2], p_int16s[3], p_int16s[4], p_int16s[5], p_int16s[6], p_int16s[7]);

    // end debug block

    if( reading_complete ){
    	printf("DMA Receive completed %lu int16s read out of %lu requested\r\n", g_int16s_read, g_i2s_wav_len);
    	print_vals_int16(g_wav_record, g_int16s_read);
    	g_i2s_rx_in_progess = 0;
    }
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
    log_printf(&g_log, "<end>w0=%d\r\n", g_wav_record[0]);
}

uint32_t receive_i2s_blocking(uint8_t *buff_ptr, uint32_t max_chars)
{
	uint32_t i2s_status = HAL_OK;
	uint32_t i2s_timeout_ms = 1000;
	uint32_t bytes_read = 0;
	uint32_t i2s_chunk_size = 64;

	// Do a first read with a longer timeout value (5s)
	i2s_status = HAL_SAI_Receive(&hsai_BlockA1, buff_ptr, i2s_chunk_size, 5000);
	if(i2s_status == HAL_OK)
	{
		bytes_read += i2s_chunk_size;
	}

	while(i2s_status == HAL_OK && bytes_read < max_chars)
	{
		i2s_status = HAL_SAI_Receive(&hsai_BlockA1,
				buff_ptr+bytes_read,
				MIN(i2s_chunk_size, max_chars-bytes_read),
				i2s_timeout_ms);
		if(i2s_status == HAL_OK)
		{
			bytes_read += i2s_chunk_size;
		}
	}

	return bytes_read;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  // set up variables for I2S receiving
  g_i2s_buffer0 = malloc(g_i2s_chunk_size_bytes);
  g_i2s_buffer1 = malloc(g_i2s_chunk_size_bytes);
  g_i2s_current_buff = g_i2s_buffer0;
  g_wav_record = (int16_t *)malloc(g_i2s_wav_len * sizeof(int16_t));

  // And for UART (over USB) connection to host
  uint8_t *uart_buff;
  uint32_t uart_timeout_ms = 200;
  uint32_t uart_status;


  // These memset()s are probably not needed, since we do it before starting to record
  memset(g_i2s_buffer0, 0xFF, g_i2s_chunk_size_bytes);
  memset(g_i2s_buffer1, 0xFF, g_i2s_chunk_size_bytes);
  memset(g_wav_record, 0xFF, g_i2s_wav_len*2);

  uart_buff = malloc(64);
  memset(uart_buff, 0x00, 64);
  printf("SWW Test beginning.  Just allocated buffers\r\n");

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_LPUART1_UART_Init();
  MX_USART3_UART_Init();
  MX_USB_OTG_FS_PCD_Init();
  MX_SAI1_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */


  while (1)
  {
	  uart_status = HAL_UART_Receive(&hlpuart1, uart_buff, 1, uart_timeout_ms);
	  if(uart_status == HAL_OK) {  // otherwise timeout => no key input
		 if( uart_buff[0] == 'r') { // read from I2S
			 if(0 && g_i2s_rx_in_progess ) {
				 printf("I2S Rx currently in progress. Ignoring request\r\n");
			 }
			 else {
				 g_i2s_rx_in_progess = 1;
				 g_int16s_read = 0;
				 printf("Listening for I2S data ... \r\n");
				 memset(g_wav_record, 0xFF, g_i2s_wav_len*2); // *2 b/c wav_len is int16s
				 memset(g_i2s_buffer0, 0x55, g_i2s_chunk_size_bytes);
				 memset(g_i2s_buffer1, 0x55, g_i2s_chunk_size_bytes);

				 g_i2s_status = HAL_SAI_Receive_DMA(&hsai_BlockA1, g_i2s_current_buff, g_i2s_chunk_size_bytes/2);
				 // you can also check hsai->State
				 printf("DMA receive initiated. status=%lu, state=%d\r\n", g_i2s_status, hsai_BlockA1.State);
				 printf("    Status: 0=OK, 1=Error, 2=Busy, 3=Timeout; State: 0=Reset, 1=Ready, 2=Busy (internal process), 18=Busy (Tx), 34=Busy (Rx)\r\n");
			 }
		 }
		 else if( uart_buff[0] == 's') { // Print SAI status and toggle pin
			 HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
			 printf("I2S Status = %lu. SAI Status = %d, Xfer count=%d, Xfer size=%d, %lu samples read, buffer <%d> active, RX in progress: %d\r\n",
					 g_i2s_status, hsai_BlockA1.State, hsai_BlockA1.XferCount, hsai_BlockA1.XferSize,
					 g_int16s_read, g_i2s_buff_sel, g_i2s_rx_in_progess);

			 // hsai_BlockA1.State: 0=Reset, 1=Ready, 2=Busy (internal process), 18=Busy (Tx), 34=Busy (Rx)

			 HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
		 }
		 else if( uart_buff[0] == 't') { // toggle GPIO pin
			 HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
			 HAL_Delay(10);
			 HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
		 }
		 else if( uart_buff[0] == 'l') { // print and reset log
			 printf("Log contents[cp=%u]:\r\n<%s>\r\n", g_log.current_pos, g_log.buffer);
			 memset(g_log.buffer, 0, LOG_BUFFER_SIZE);
			 g_log.current_pos = 0;
		 }
		 else if( uart_buff[0] == '0') { // print buffer 0
			 printf("Buffer 0: \r\n");
			 print_vals_int16((int16_t *)g_i2s_buffer0, g_i2s_chunk_size_bytes/2);
			 // print_bytes(g_i2s_buffer0, g_i2s_chunk_size_bytes);
		 }
		 else if( uart_buff[0] == '1') { // print buffer 1b
			 printf("Buffer 1: \r\n");
			 print_vals_int16((int16_t *)g_i2s_buffer1, g_i2s_chunk_size_bytes/2);
			 // print_bytes(g_i2s_buffer1, g_i2s_chunk_size_bytes);
		 }
		 else if( uart_buff[0] == 'w') { // print wav_record as bytes
			 printf("Wav record as ints: \r\n");
			 print_vals_int16(g_wav_record, g_i2s_chunk_size_bytes/2);
			 // print_bytes((uint8_t *)g_wav_record, g_int16s_read*2);
		 }
		 else if( uart_buff[0] == 'b') { // print wav_record as bytes
			 printf("Set a breakpoint here if you want: \r\n");
		 }
		 else {
			 printf("Unexpected character: %c\r\n", uart_buff[0]);
		 }

	  }
	  //

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }

  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 2;
  RCC_OscInitStruct.PLL.PLLN = 30;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief LPUART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_LPUART1_UART_Init(void)
{

  /* USER CODE BEGIN LPUART1_Init 0 */

  /* USER CODE END LPUART1_Init 0 */

  /* USER CODE BEGIN LPUART1_Init 1 */

  /* USER CODE END LPUART1_Init 1 */
  hlpuart1.Instance = LPUART1;
  hlpuart1.Init.BaudRate = 115200;
  hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
  hlpuart1.Init.StopBits = UART_STOPBITS_2;
  hlpuart1.Init.Parity = UART_PARITY_NONE;
  hlpuart1.Init.Mode = UART_MODE_TX_RX;
  hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hlpuart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  hlpuart1.FifoMode = UART_FIFOMODE_DISABLE;
  if (HAL_UART_Init(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&hlpuart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&hlpuart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LPUART1_Init 2 */

  /* USER CODE END LPUART1_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief SAI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SAI1_Init(void)
{

  /* USER CODE BEGIN SAI1_Init 0 */

  /* USER CODE END SAI1_Init 0 */

  /* USER CODE BEGIN SAI1_Init 1 */

  /* USER CODE END SAI1_Init 1 */
  hsai_BlockA1.Instance = SAI1_Block_A;
  hsai_BlockA1.Init.AudioMode = SAI_MODESLAVE_RX;
  hsai_BlockA1.Init.Synchro = SAI_ASYNCHRONOUS;
  hsai_BlockA1.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
  hsai_BlockA1.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
  hsai_BlockA1.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
  hsai_BlockA1.Init.MonoStereoMode = SAI_STEREOMODE;
  hsai_BlockA1.Init.CompandingMode = SAI_NOCOMPANDING;
  hsai_BlockA1.Init.TriState = SAI_OUTPUT_NOTRELEASED;
  if (HAL_SAI_InitProtocol(&hsai_BlockA1, SAI_I2S_STANDARD, SAI_PROTOCOL_DATASIZE_16BIT, 2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SAI1_Init 2 */

  /* USER CODE END SAI1_Init 2 */

}

/**
  * @brief USB_OTG_FS Initialization Function
  * @param None
  * @retval None
  */
static void MX_USB_OTG_FS_PCD_Init(void)
{

  /* USER CODE BEGIN USB_OTG_FS_Init 0 */

  /* USER CODE END USB_OTG_FS_Init 0 */

  /* USER CODE BEGIN USB_OTG_FS_Init 1 */

  /* USER CODE END USB_OTG_FS_Init 1 */
  hpcd_USB_OTG_FS.Instance = USB_OTG_FS;
  hpcd_USB_OTG_FS.Init.dev_endpoints = 6;
  hpcd_USB_OTG_FS.Init.speed = PCD_SPEED_FULL;
  hpcd_USB_OTG_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
  hpcd_USB_OTG_FS.Init.Sof_enable = ENABLE;
  hpcd_USB_OTG_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.lpm_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.battery_charging_enable = ENABLE;
  hpcd_USB_OTG_FS.Init.use_dedicated_ep1 = DISABLE;
  hpcd_USB_OTG_FS.Init.vbus_sensing_enable = ENABLE;
  if (HAL_PCD_Init(&hpcd_USB_OTG_FS) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB_OTG_FS_Init 2 */

  /* USER CODE END USB_OTG_FS_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMAMUX1_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  HAL_PWREx_EnableVddIO2();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LD3_Pin|LD2_Pin|GPIO_PIN_8, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(USB_PowerSwitchOn_GPIO_Port, USB_PowerSwitchOn_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD3_Pin LD2_Pin */
  GPIO_InitStruct.Pin = LD3_Pin|LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : USB_OverCurrent_Pin */
  GPIO_InitStruct.Pin = USB_OverCurrent_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USB_OverCurrent_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : USB_PowerSwitchOn_Pin */
  GPIO_InitStruct.Pin = USB_PowerSwitchOn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(USB_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PB8 */
  GPIO_InitStruct.Pin = GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
