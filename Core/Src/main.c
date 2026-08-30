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
#include <stdlib.h>
#include <stdio.h>
#include "GPIO.h"
#include "UART.h"
#include "ADC.h"
#include "Math.h"
#include "AM2302.h"
#include "SPI.h"
#include "FaultDetect.h"
#include "SD_Log.h"
#include "fault_models.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// Sensor data structure for SPI transmission
typedef struct __attribute__((packed)) {
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

// One CSV measurement row printed over UART per this interval (~5-10s per
// the paper data-collection spec). Tune as needed.
#define CSV_LOG_INTERVAL_MS 7000
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

// MQ-9 raw ADC + fault-detection bitfield for the CSV log (not sent over SPI)
uint16_t current_mq9_raw = 0;
uint8_t current_mq9_quality = 0;
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
void CSV_Log_Task(void *pvParameters);
#if ENABLE_BENCH
void Bench_Task(void *pvParameters);
#endif

// Helper functions
void Update_Sensor_Data(float mcu_temp, float mq9, float am2302_temp, float am2302_hum);
void Update_MQ9_Diagnostics(uint16_t raw_adc, uint8_t quality_flags);
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
				  256,
				  NULL,
				  1,
				  NULL);

  // SPI_Sensor_Data_Task (ESP32 link) is intentionally NOT started for the
  // Nucleo-only / UART-CSV paper data run: it shares SPI2 + PC0 with the
  // microSD wiring in Part 3, and its debug prints would otherwise interleave
  // with the CSV log on UART. The function and its SPI/ESP32 logic are left
  // untouched below - re-enable this xTaskCreate to go back to ESP32 mode.
  /*
  xTaskCreate(SPI_Sensor_Data_Task,
                  "SPI Sensor Data",
                  512,
                  NULL,
                  2,
                  NULL);
  */

  xTaskCreate(CSV_Log_Task,
                  "CSV Log",
                  384,
                  NULL,
                  1,
                  NULL);

#if ENABLE_BENCH
  // One-shot inference-latency benchmark for the active fault_classify()
  // model (see Core/Inc/bench_config.h). Set ENABLE_BENCH to 0 there to
  // compile this out once you're done collecting numbers.
  xTaskCreate(Bench_Task,
                  "Bench",
                  384,
                  NULL,
                  1,
                  NULL);
#endif

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
        .crc_enable = false,          // Enable CRC for error checking
        .crc_polynomial = 0x107      // Standard CRC-8 polynomial
    };

    // Initialize SPI2 peripheral
    SPI_Init(&hspi2, SPI2, &spi_config);

    // Optional: Configure NSS pin as GPIO output (PC0)
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;  // Enable GPIOC clock
    GPIOC->MODER |= GPIO_MODER_MODER0_0;  // PC0 as output
    GPIOC->OTYPER &= ~GPIO_OTYPER_OT_0;   // Push-pull output
    GPIOC->ODR |= GPIO_ODR_OD0;           // Start with NSS high (slave not selected)

    // Print initialization message ("#" prefix = debug output, not a CSV row)
    UART_Init();
    Print_Message("# SPI2 Initialized for ESP32 communication\r\n", 44);
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
  * @brief Update MQ-9 raw ADC + fault-detection flags (thread-safe).
  *        Reuses xSPIMutex since it already guards this same group of
  *        "latest sensor snapshot" globals.
  */
void Update_MQ9_Diagnostics(uint16_t raw_adc, uint8_t quality_flags)
{
    if(xSemaphoreTake(xSPIMutex, (TickType_t)10) == pdTRUE) {
        current_mq9_raw = raw_adc;
        current_mq9_quality = quality_flags;
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

    //FreeRTOS tick count converted to milliseconds
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
    uint8_t rx_buffer[sizeof(SensorData_t)];

    // Wait for other sensors to start producing data
    vTaskDelay(pdMS_TO_TICKS(3000));

    UART_Init();
    Print_Message("# SPI Sensor Data Task Started\r\n", 32);
    SPI2_Init();
    while(1) {
        // Prepare the sensor data structure
    	Prepare_SPI_Data(&sensor_data);

    	    if(xSemaphoreTake(xSPIMutex, (TickType_t)20) == pdTRUE) {
    	        GPIOC->ODR &= ~GPIO_ODR_OD0; // CS Low

    	        // --- ADD DELAY ---
    	        for(volatile int i=0; i<500; i++);

    	        // Transmit directly from the struct pointer
    	        bool success = SPI_TransmitReceive(&hspi2, (uint8_t*)&sensor_data, rx_buffer, sizeof(SensorData_t));

    	        for(volatile int i=0; i<500; i++);
    	        GPIOC->ODR |= GPIO_ODR_OD0; // CS High
    	        xSemaphoreGive(xSPIMutex);

            // Debug output ("#" prefix = debug output, not a CSV row)
            if(success) {
                int len = sprintf(debug_buffer,
                    "# SPI Sent: MCU:%.1fC, MQ9:%.1fppm, DHT:%.1fC/%.1f%%\r\n",
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
                    Print_Message("# SPI Transmission Failed\r\n", 27);
                    xSemaphoreGive(xUARTMutex);
                }
            }

            // Check for CRC errors
            if(SPI_CheckCRCError(&hspi2)) {
                if(xSemaphoreTake(xUARTMutex, (TickType_t)10) == pdTRUE) {
                    Print_Message("# SPI CRC Error Detected\r\n", 26);
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
	const char Hearth_beat[] = "# Heart beat\r\n";
	while(1){
		if(xSemaphoreTakeRecursive(xRecursiveMutex, (TickType_t)5) == pdTRUE){
			// LED doubles as an SD-card status indicator (Part 3): a normal
			// slow 5s/5s heartbeat means "alive, SD fine (or SD logging
			// disabled)"; a fast blink means the SD card has failed - both
			// are visible across a room without needing a serial monitor.
			uint32_t on_delay_ms = 5000;
			uint32_t off_delay_ms = 5000;
			if (SD_Log_GetStatus() == SD_LOG_FAILED) {
				on_delay_ms = 150;
				off_delay_ms = 150;
			}

			Hearth_beat_ON();
			if(xSemaphoreTake(xUARTMutex, (TickType_t)20) == pdTRUE) {
				Print_Message((void *)Hearth_beat, sizeof(Hearth_beat)-1);
				xSemaphoreGive(xUARTMutex);
			}
			vTaskDelay(pdMS_TO_TICKS(on_delay_ms));
			Hearth_beat_OFF();
			vTaskDelay(pdMS_TO_TICKS(off_delay_ms));
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
			// UPDATE GLOBAL DATA
			Update_Sensor_Data(temperature, current_mq9_ppm, current_am2302_temp, current_am2302_humidity);
			//float ADC_ReadTempSensor(void)
			// Convert float to string ("#" prefix = debug output, not a CSV row)
			int len = sprintf(buffer, "# Temp Value: %.2f\r\n", temperature);
			// Send over UART
			if(xSemaphoreTake(xUARTMutex, (TickType_t)20) == pdTRUE) {
				Print_Message(buffer, len);
				xSemaphoreGive(xUARTMutex);
			}
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
	    FaultDetect_Init();
	    char buffer[160];

	    while(1){
	        if(xSemaphoreTakeRecursive(xRecursiveMutex, (TickType_t)5) == pdTRUE){

	            uint16_t raw = ADC_ReadChannel(0);   // ADC raw counts
	            float Rs = MQ9_GetRs(raw);           // sensor resistance
	            float ratio = Rs / Ro_MQ9;           // Rs/Ro
	            float ppm = MQ9_GetPPM(ratio);       // estimated ppm

	            // Rule-based fault check for this measurement cycle (MQ-9 only)
	            uint8_t quality_flags = FaultDetect_Update(raw, ppm);

	            // Build the 5-feature vector for the on-device ML classifier,
	            // in the exact order fault_models.h expects: {raw, ppm, mcu_t,
	            // am_t, hum}. mcu_t/am_t/hum come from the latest snapshot
	            // published by MCU_Temperature_Task / AM2302_Task, read the
	            // same way CSV_Log_Task does (xSPIMutex-guarded).
	            float fm_x[FM_N_FEATURES];
	            fm_x[0] = (float)raw;
	            fm_x[1] = ppm;
	            if(xSemaphoreTake(xSPIMutex, (TickType_t)10) == pdTRUE) {
	                fm_x[2] = current_mcu_temp;
	                fm_x[3] = current_am2302_temp;
	                fm_x[4] = current_am2302_humidity;
	                xSemaphoreGive(xSPIMutex);
	            } else {
	                fm_x[2] = current_mcu_temp;
	                fm_x[3] = current_am2302_temp;
	                fm_x[4] = current_am2302_humidity;
	            }
	            int fm_class = fault_classify(fm_x);

	            // UPDATE GLOBAL DATA (ppm for the SPI struct, raw+flags for the CSV log)
	            Update_Sensor_Data(current_mcu_temp, ppm, current_am2302_temp, current_am2302_humidity);
	            Update_MQ9_Diagnostics(raw, quality_flags);

	            // "#" prefix marks this as debug output, not a CSV data row
	            int len = sprintf(buffer,
	                              "# MQ9 raw=%u, Rs=%.1f Ohm, ratio=%.2f, ppm=%.1f, quality_flags=%u, ml_class=%s\r\n",
	                              raw, Rs, ratio, ppm, quality_flags, FM_CLASS_NAMES[fm_class]);

	            if(xSemaphoreTake(xUARTMutex, (TickType_t)20) == pdTRUE) {
	                Print_Message(buffer, len);
	                xSemaphoreGive(xUARTMutex);
	            }

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
    //vTaskDelay in an RTOS environment.
    // "#" prefix on all debug lines below marks them as non-CSV output
    if(xSemaphoreTake(xUARTMutex, (TickType_t)20) == pdTRUE) {
        Print_Message("# AM2302 - Waiting for sensor stabilization...\r\n", 48);
        xSemaphoreGive(xUARTMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(2500));

    if(xSemaphoreTake(xUARTMutex, (TickType_t)20) == pdTRUE) {
        Print_Message("# AM2302 Task Started\r\n", 23);
        xSemaphoreGive(xUARTMutex);
    }

    while(1) {
        if(xSemaphoreTake(xUARTMutex, (TickType_t)20) == pdTRUE) {
            Print_Message("# AM2302 - Attempting to read...\r\n", 34);
            xSemaphoreGive(xUARTMutex);
        }

        if (AM2302_Read(&temperature, &humidity)) {
            // Success - format and print data
            int len = sprintf(buffer,
                             "# AM2302 - Success! Temp: %.1fC, Humidity: %.1f%%\r\n",
                             temperature, humidity);

            // Update global data for SPI task
            Update_Sensor_Data(current_mcu_temp, current_mq9_ppm, temperature, humidity);

            // UART mutex is used for printing to prevent garbled output
            if(xSemaphoreTake(xUARTMutex, (TickType_t)10) == pdTRUE) {
                Print_Message(buffer, len);
                xSemaphoreGive(xUARTMutex);
            }

        } else {
            // Error reading sensor
            if(xSemaphoreTake(xUARTMutex, (TickType_t)10) == pdTRUE) {
                Print_Message("# AM2302 - Read Failed (Checksum or Timeout)\r\n", 46);
                xSemaphoreGive(xUARTMutex);
            }
        }

        // Wait at least 2 seconds between reads as per datasheet recommendations
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

/**
  * @brief Prints the CSV data log: one header line at startup, then one CSV
  *        measurement row every CSV_LOG_INTERVAL_MS, combining the latest
  *        snapshot from all other sensor tasks.
  *
  *        Column order (fixed): timestamp_ms,raw_mq9_adc,computed_ppm,
  *        mcu_temp_c,am2302_temp_c,humidity_pct,quality_flags
  *
  *        Each row is built into a single local buffer with one sprintf()
  *        call, then emitted as a single Print_Message() call while holding
  *        xUARTMutex for its entire duration - this is what makes a row
  *        atomic: no other task's debug output can land in the middle of it.
  */
void CSV_Log_Task(void *pvParameters)
{
    UART_Init();

    char line[128];

    // Header printed exactly once at startup, and written once to a fresh
    // /ECOHIVE/LOG_XXXX.CSV file on the microSD card (Part 3). SD_Log_Init()
    // is a no-op when ENABLE_SD_LOGGING is 0.
    int header_len = sprintf(line,
        "timestamp_ms,raw_mq9_adc,computed_ppm,mcu_temp_c,am2302_temp_c,humidity_pct,quality_flags\r\n");
    if(xSemaphoreTake(xUARTMutex, (TickType_t)50) == pdTRUE) {
        Print_Message(line, header_len);
        xSemaphoreGive(xUARTMutex);
    }
    SD_Log_Init(line, (uint16_t)header_len);

    while(1) {
        // Snapshot the latest values published by the other sensor tasks.
        // Defaults cover the (rare) case the mutex take below times out.
        float mcu_temp = 0.0f, mq9_ppm = MQ9_PPM_INVALID, am2302_temp = 0.0f, am2302_hum = 0.0f;
        uint16_t mq9_raw = 0;
        uint8_t quality_flags = 0;

        if(xSemaphoreTake(xSPIMutex, (TickType_t)10) == pdTRUE) {
            mcu_temp      = current_mcu_temp;
            mq9_ppm       = current_mq9_ppm;
            am2302_temp   = current_am2302_temp;
            am2302_hum    = current_am2302_humidity;
            mq9_raw       = current_mq9_raw;
            quality_flags = current_mq9_quality;
            xSemaphoreGive(xSPIMutex);
        }

        uint32_t timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // mq9_ppm can never be NaN/Inf here: MQ9_GetPPM() always returns a
        // finite value (MQ9_PPM_INVALID sentinel on fault), so %.2f below
        // can never print "nan"/"inf" into the CSV.
        int len = sprintf(line, "%lu,%u,%.2f,%.2f,%.2f,%.2f,%u\r\n",
                           (unsigned long)timestamp_ms,
                           mq9_raw,
                           mq9_ppm,
                           mcu_temp,
                           am2302_temp,
                           am2302_hum,
                           quality_flags);

        if(xSemaphoreTake(xUARTMutex, (TickType_t)50) == pdTRUE) {
            Print_Message(line, len);
            xSemaphoreGive(xUARTMutex);
        }

        // Same buffer, same bytes, written to the SD card too (Part 3) so
        // the file always matches what you saw live over UART. No-op when
        // ENABLE_SD_LOGGING is 0; internally retries/recovers on its own
        // and never blocks this task indefinitely on a card fault.
        SD_Log_WriteRow(line, (uint16_t)len);

        vTaskDelay(pdMS_TO_TICKS(CSV_LOG_INTERVAL_MS));
    }
}


#if ENABLE_BENCH
/**
  * @brief Enables the DWT cycle counter if it isn't already running.
  *        Never resets/re-zeroes CYCCNT if some other part of the code has
  *        already turned it on - Bench_Task only ever reads the *delta*
  *        between two CYCCNT snapshots, so an already-running counter is
  *        fine to measure against as-is.
  */
static void Bench_EnableDWT(void)
{
    if (!(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk)) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }
}

/**
  * @brief One-shot inference-latency benchmark for the active fault_classify()
  *        model (MODEL_TREE / MODEL_LOGREG / MODEL_MLP, selected in
  *        Core/Inc/bench_config.h). Runs BENCH_ITERATIONS calls back-to-back
  *        inside a FreeRTOS critical section (so no other task/ISR can
  *        preempt mid-loop and pollute the cycle count), reports the total,
  *        the average cycles/call, and the average microseconds/call at
  *        BENCH_SYSCLK_HZ, then deletes itself - it only ever needs to run
  *        once per boot.
  *
  *        Line format (parseable, one line, "#BENCH" prefix so it's easy to
  *        grep out of the UART log alongside the other "#"-prefixed debug
  *        lines):
  *          #BENCH model=<name> iterations=<n> total_cycles=<n> avg_cycles=<f> avg_us=<f> sysclk_hz=<n>
  */
void Bench_Task(void *pvParameters)
{
    (void)pvParameters;
    UART_Init();

    // Let the startup prints from the other tasks (heartbeat, sensor inits)
    // clear out first so the #BENCH line isn't interleaved mid-boot.
    vTaskDelay(pdMS_TO_TICKS(500));

    Bench_EnableDWT();

    // Fixed representative feature vector. The benchmark measures per-call
    // CPU/inference overhead, not classification accuracy - for all three
    // models here (fixed-depth tree, fixed-size matrix multiply, fixed-size
    // MLP forward pass) the per-call cost does not depend on which class is
    // returned, so a single fixed input is a fair, repeatable timing probe.
    static const float x[FM_N_FEATURES] = {967.7f, 144.0f, 29.6f, 26.0f, 57.9f};
    volatile int sink = 0;

    // Critical section: disables interrupts/preemption for the timed loop
    // only, so the measured cycles are exactly fault_classify()'s own cost -
    // not diluted or spiked by the RTOS tick, another task, or (if
    // ENABLE_SD_LOGGING were left on) an SD card write landing mid-loop.
    taskENTER_CRITICAL();
    uint32_t cycles_start = DWT->CYCCNT;
    for (uint32_t i = 0; i < BENCH_ITERATIONS; i++) {
        sink = fault_classify(x);
    }
    uint32_t cycles_end = DWT->CYCCNT;
    taskEXIT_CRITICAL();
    (void)sink;

    uint32_t total_cycles = cycles_end - cycles_start;
    float avg_cycles = (float)total_cycles / (float)BENCH_ITERATIONS;
    float avg_us = avg_cycles / ((float)BENCH_SYSCLK_HZ / 1000000.0f);

    char buf[160];
    int len = sprintf(buf,
        "#BENCH model=%s iterations=%lu total_cycles=%lu avg_cycles=%.1f avg_us=%.3f sysclk_hz=%lu\r\n",
        FM_MODEL_NAME,
        (unsigned long)BENCH_ITERATIONS,
        (unsigned long)total_cycles,
        avg_cycles,
        avg_us,
        (unsigned long)BENCH_SYSCLK_HZ);

    if(xSemaphoreTake(xUARTMutex, (TickType_t)200) == pdTRUE) {
        Print_Message(buf, len);
        xSemaphoreGive(xUARTMutex);
    }

    vTaskDelete(NULL);
}
#endif /* ENABLE_BENCH */

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
