/* USER CODE BEGIN Header */
/**
  *******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "semphr.h"
#include <stdlib.h>  //rand()
#include <stdio.h>   //sprintf()
#include "GPIO.h"
#include "UART.h"
#include "ADC.h"
#include "Math.h"
#include "AM2302.h"
#include "SPI.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// Sensor data structure for SPI transmission
typedef struct {
    float mcu_temperature;
    float mq9_ppm;
    float am2302_temperature;
    float am2302_humidity;
    uint32_t timestamp;
    uint8_t checksum;
} SensorData_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SPI_DATA_RATE_MS 2000  // Send data every 2 seconds
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* Definitions for defaultTask */

/* USER CODE BEGIN PV */
SemaphoreHandle_t xRecursiveMutex;
SemaphoreHandle_t xUARTMutex;
SemaphoreHandle_t xSPIMutex;  // SPI mutex for thread-safe access

SPI_Handle hspi2;  // SPI handle for communication with ESP32

// Global sensor data variables
float current_mcu_temp = 0.0f;
float current_mq9_ppm = 0.0f;
float current_am2302_temp = 0.0f;
float current_am2302_humidity = 0.0f;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
void SystemCoreClockUpdate(void);
void GPIOA_Init(void); // GPIO initialization function
void UART_Init(void);  // UART initialization function
void ADC_Init(void);  // ADC initialization function
void SPI2_Init(void);  // SPI initialization function
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void Hearth_beat_Task(void *pvParameters);
void UART_Task(void *pvParameters);
void MCU_Temperature_Task(void *pvParameters);
void MQ9_Task(void *pvParameters);
void AM2302_Task(void *pvParameters);
void SPI_Sensor_Data_Task(void *pvParameters);

// Helper functions
void Update_Sensor_Data(float mcu_temp, float mq9, float am2302_temp, float am2302_hum);
uint8_t Calculate_Checksum(uint8_t *data, uint32_t size);
void Prepare_SPI_Data(SensorData_t *sensor_data);
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  SystemCoreClockUpdate();
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  /* USER CODE BEGIN 2 */
  //Priorities of tasks must be consider for better operation
  xRecursiveMutex = xSemaphoreCreateRecursiveMutex();
  xUARTMutex = xSemaphoreCreateMutex();
  xSPIMutex = xSemaphoreCreateMutex();

  xTaskCreate(Hearth_beat_Task,
    		  	  "Heart Beat",
    			  128,
    			  NULL,
    			  1,
    			  NULL);

  /*xTaskCreate(UART_Task,
     		  	  "UART",
     			  256,
     			  NULL,
     			  1,
     			  NULL);
     			  */
  xTaskCreate(MCU_Temperature_Task,
      		  	  "MCU Temp",
      			  256,
      			  NULL,
      			  1,
      			  NULL);
  xTaskCreate(MQ9_Task,
        		  "MQ9 Temp",
        		  256,
        		  NULL,
				  1,
        		  NULL);
  xTaskCreate(AM2302_Task,
              	  "AM2302 Sensor",
				  256,    // Increased stack for sensor operations
				  NULL,
				  1,      // Same priority as other sensor tasks
				  NULL);
  xTaskCreate(SPI_Sensor_Data_Task,
                  "SPI Sensor Data",
                  512,  // Larger stack for SPI operations
                  NULL,
                  2,    // Higher priority to ensure timely data transmission
                  NULL);

  vTaskStartScheduler();
  /* USER CODE END 2 */

  /* Init scheduler */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */


  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}


/* USER CODE BEGIN 4 */
/**
  * @brief Initialize SPI2 for communication with ESP32
  */
void SPI2_Init(void)
{
    // Configure SPI parameters
    SPI_Config spi_config = {
        .spi_frequency = 1000000,    // 1 MHz clock
        .data_size = 8,              // 8-bit data frames
        .lsb_first = false,          // MSB first (standard)
        .cpol = false,               // Clock idle low
        .cpha = false,               // Data sampled on first edge
        .crc_enable = true,          // Enable CRC for error checking
        .crc_polynomial = 0x107      // Standard CRC-8 polynomial
    };

    // Initialize SPI2 peripheral
    SPI_Init(&hspi2, SPI2, &spi_config);

    // Optional: Configure NSS pin as GPIO output (PC0)
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;  // Enable GPIOC clock
    GPIOC->MODER |= GPIO_MODER_MODER0_0;  // PC0 as output
    GPIOC->OTYPER &= ~GPIO_OTYPER_OT_0;   // Push-pull output
    GPIOC->ODR |= GPIO_ODR_OD0;           // Start with NSS high (slave not selected)

    // Print initialization message
    UART_Init();
    Print_Message("SPI2 Initialized for ESP32 communication\r\n", 41);
}

/**
  * @brief Update global sensor data (thread-safe)
  */
void Update_Sensor_Data(float mcu_temp, float mq9, float am2302_temp, float am2302_hum)
{
    if(xSemaphoreTake(xSPIMutex, (TickType_t)10) == pdTRUE) {
        current_mcu_temp = mcu_temp;
        current_mq9_ppm = mq9;
        current_am2302_temp = am2302_temp;
        current_am2302_humidity = am2302_hum;
        xSemaphoreGive(xSPIMutex);
    }
}

/**
  * @brief Calculate simple checksum for data integrity
  */
uint8_t Calculate_Checksum(uint8_t *data, uint32_t size)
{
    uint8_t checksum = 0;
    for(uint32_t i = 0; i < size; i++) {
        checksum ^= data[i];  // Simple XOR checksum
    }
    return checksum;
}

/**
  * @brief Prepare sensor data structure for SPI transmission
  */
void Prepare_SPI_Data(SensorData_t *sensor_data)
{
    // Get current sensor data (thread-safe)
    if(xSemaphoreTake(xSPIMutex, (TickType_t)10) == pdTRUE) {
        sensor_data->mcu_temperature = current_mcu_temp;
        sensor_data->mq9_ppm = current_mq9_ppm;
        sensor_data->am2302_temperature = current_am2302_temp;
        sensor_data->am2302_humidity = current_am2302_humidity;
        xSemaphoreGive(xSPIMutex);
    }

    // Add timestamp (FreeRTOS tick count converted to milliseconds)
    sensor_data->timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Calculate checksum
    sensor_data->checksum = Calculate_Checksum((uint8_t*)sensor_data, sizeof(SensorData_t) - 1);
}

/**
  * @brief New task to send sensor data to ESP32 via SPI
  */
void SPI_Sensor_Data_Task(void *pvParameters)
{
    char debug_buffer[128];
    SensorData_t sensor_data;
    uint8_t tx_buffer[sizeof(SensorData_t)];
    uint8_t rx_buffer[sizeof(SensorData_t)];

    // Wait a bit for other sensors to start producing data
    vTaskDelay(pdMS_TO_TICKS(3000));

    UART_Init();
    Print_Message("SPI Sensor Data Task Started\r\n", 30);
    SPI2_Init();
    while(1) {
        // Prepare the sensor data structure
        Prepare_SPI_Data(&sensor_data);

        // Copy to transmission buffer
        memcpy(tx_buffer, &sensor_data, sizeof(SensorData_t));

        // Send data via SPI (thread-safe)
        if(xSemaphoreTake(xSPIMutex, (TickType_t)20) == pdTRUE) {
            // Select ESP32 (set NSS low)
            GPIOC->ODR &= ~GPIO_ODR_OD0;
            for(volatile int i=0; i<100; i++);
            // Send data via SPI (full duplex)
            bool success = SPI_TransmitReceive(&hspi2, tx_buffer, rx_buffer, sizeof(SensorData_t));

            // Deselect ESP32 (set NSS high)
            GPIOC->ODR |= GPIO_ODR_OD0;

            xSemaphoreGive(xSPIMutex);

            // Debug output
            if(success) {
                int len = sprintf(debug_buffer,
                    "SPI Sent: MCU:%.1fC, MQ9:%.1fppm, DHT:%.1fC/%.1f%%\r\n",
                    sensor_data.mcu_temperature,
                    sensor_data.mq9_ppm,
                    sensor_data.am2302_temperature,
                    sensor_data.am2302_humidity);

                if(xSemaphoreTake(xUARTMutex, (TickType_t)10) == pdTRUE) {
                    Print_Message(debug_buffer, len);
                    xSemaphoreGive(xUARTMutex);
                }
            } else {
                if(xSemaphoreTake(xUARTMutex, (TickType_t)10) == pdTRUE) {
                    Print_Message("SPI Transmission Failed\r\n", 26);
                    xSemaphoreGive(xUARTMutex);
                }
            }

            // Check for CRC errors
            if(SPI_CheckCRCError(&hspi2)) {
                if(xSemaphoreTake(xUARTMutex, (TickType_t)10) == pdTRUE) {
                    Print_Message("SPI CRC Error Detected\r\n", 25);
                    xSemaphoreGive(xUARTMutex);
                }
                SPI_ClearCRC(&hspi2);
            }
        }

        // Wait before next transmission
        vTaskDelay(pdMS_TO_TICKS(SPI_DATA_RATE_MS));
    }
}


void Hearth_beat_Task(void *pvParameters)
{
	GPIOA_Init();
	UART_Init();
	const char Hearth_beat[] = "Heart beat\r\n";
	while(1){
		if(xSemaphoreTakeRecursive(xRecursiveMutex, (TickType_t)5) == pdTRUE){
			Hearth_beat_ON();
			Print_Message(Hearth_beat, sizeof(Hearth_beat)-1);
			vTaskDelay(pdMS_TO_TICKS(1000));
			Hearth_beat_OFF();
			vTaskDelay(pdMS_TO_TICKS(500));
			xSemaphoreGiveRecursive(xRecursiveMutex);
		}
		vTaskDelay(1);
	}
}

void UART_Task(void *pvParameters)
{
	UART_Init();
	//const char message1[] = "Hello from UART Task\r\n";
	char buffer[64];
	float Random_value;
	while(1){
		if(xSemaphoreTakeRecursive(xRecursiveMutex, (TickType_t)5) == pdTRUE){
			//Print_Message(message1, sizeof(message1) - 1);
			// Generate a random float between 0.0 and 100.0
			Random_value = (float)(rand() % 10000) / 100.0f;
			// Convert float to string
			int len = sprintf(buffer, "Random Value: %.2f\r\n", Random_value);
			// Send over UART
			Print_Message(buffer, len);
			vTaskDelay(pdMS_TO_TICKS(1000));
			xSemaphoreGiveRecursive(xRecursiveMutex);
		}
		vTaskDelay(1);
	}
}
void MCU_Temperature_Task(void *pvParameters)
{
	UART_Init();
	ADC_Init();
	char buffer[64];
	volatile float temperature;
	while(1){
		if(xSemaphoreTakeRecursive(xRecursiveMutex, (TickType_t)5) == pdTRUE){
			// Read internal temperature sensor
			temperature = ADC_ReadTempSensor();  // °C
			// UPDATE GLOBAL DATA HERE
			Update_Sensor_Data(temperature, current_mq9_ppm, current_am2302_temp, current_am2302_humidity);
			//float ADC_ReadTempSensor(void)
			// Convert float to string
			int len = sprintf(buffer, "Temp Value: %.2f\r\n", temperature);
			// Send over UART
			Print_Message(buffer, len);
			vTaskDelay(pdMS_TO_TICKS(1000));
			xSemaphoreGiveRecursive(xRecursiveMutex);
		}
		vTaskDelay(1);
	}
}
void MQ9_Task(void *pvParameters)
{
	UART_Init();
	    ADC_Init();
	    char buffer[64];

	    while(1){
	        if(xSemaphoreTakeRecursive(xRecursiveMutex, (TickType_t)5) == pdTRUE){

	            uint16_t raw = ADC_ReadChannel(0);   // ADC raw counts
	            float Rs = MQ9_GetRs(raw);           // sensor resistance
	            float ratio = Rs / Ro_MQ9;           // Rs/Ro
	            float ppm = MQ9_GetPPM(ratio);       // estimated ppm

	            // UPDATE GLOBAL DATA HERE
	            Update_Sensor_Data(current_mcu_temp, ppm, current_am2302_temp, current_am2302_humidity);

	            int len = sprintf(buffer,
	                              "MQ9 raw=%u, Rs=%.1f Ohm, ratio=%.2f, ppm=%.1f\r\n",
	                              raw, Rs, ratio, ppm);

	            Print_Message(buffer, len);

	            vTaskDelay(pdMS_TO_TICKS(1000));
	            xSemaphoreGiveRecursive(xRecursiveMutex);
	        }
	        vTaskDelay(1);
	    }
}


void AM2302_Task(void *pvParameters) {
    UART_Init();
    AM2302_Init(); // Initialize hardware

    char buffer[128];
    float temperature = 0.0f;
    float humidity = 0.0f;

    // Wait for sensor stabilization (>2s after power-up).
    // Use vTaskDelay in an RTOS environment.
    Print_Message("AM2302 - Waiting for sensor stabilization...\r\n", 45);
    vTaskDelay(pdMS_TO_TICKS(2500));

    Print_Message("AM2302 Task Started\r\n", 21);

    while(1) {
        // No need for mutex here as AM2302_Read handles its own critical section
        // and doesn't use shared resources that require a mutex.
        // Print debug message *before* the time-sensitive read operation.
        Print_Message("AM2302 - Attempting to read...\r\n", 31);

        if (AM2302_Read(&temperature, &humidity)) {
            // Success - format and print data
            int len = sprintf(buffer,
                             "AM2302 - Success! Temp: %.1fC, Humidity: %.1f%%\r\n",
                             temperature, humidity);

            // Update global data for SPI task
            Update_Sensor_Data(current_mcu_temp, current_mq9_ppm, temperature, humidity);

            // Use UART mutex for printing to prevent garbled output
            if(xSemaphoreTake(xUARTMutex, (TickType_t)10) == pdTRUE) {
                Print_Message(buffer, len);
                xSemaphoreGive(xUARTMutex);
            }

        } else {
            // Error reading sensor
            if(xSemaphoreTake(xUARTMutex, (TickType_t)10) == pdTRUE) {
                Print_Message("AM2302 - Read Failed (Checksum or Timeout)\r\n", 43);
                xSemaphoreGive(xUARTMutex);
            }
        }

        // Wait at least 2 seconds between reads as per datasheet recommendations
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}




/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

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
