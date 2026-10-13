/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
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
#include "EPD_2in13_V4.h"
#include "GUI_Paint.h"
#include <stdbool.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi2;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_SPI2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
volatile int32_t encoder_value = 0;	//Used to read rotary encoder rotation.
volatile uint32_t last_encoder_value = 0;

//Used to read rotary encoder interrupt.
volatile uint32_t last_managed_exti = 0;
volatile uint32_t last_flank_causing_exti = 0;

//States.
enum State { //Are we selecting a value or changing a value.
  Entering_Number,
  Choosing_Number,
};
enum activeVariable { //What value are we selecting or changing.
	Hole,
	HCP,
	Strokes,
};

//Golf variables.
volatile uint32_t extra_strokes = 0;
volatile uint16_t hole = 0;
volatile int strokes[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
volatile int points[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
int par[] = {0, 3, 4, 4, 5, 4, 4, 4, 4, 3, 4, 4, 3, 5, 4, 4, 5, 3, 4};
int hcp_index[] = {0, 17, 9, 11, 3, 1, 15, 7, 5, 13, 6, 14, 18, 4, 8, 2, 12, 16, 10};


UBYTE ImageBuffer[4000]; //Display buffer.

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {  //Register button press.
	if (GPIO_Pin == RE_SW_Pin) {
		if (last_flank_causing_exti == last_managed_exti) {
			last_flank_causing_exti = HAL_GetTick();
		}
	}
}

//Calculate points for individual holes and store in points[].
void calculate_stableford_points() {
    if (hole < 1 || hole > 18) {
        return;
    }

    if (strokes[hole] == 0) {
        points[hole] = 0;
        return;
    }

    int base_extra_strokes = extra_strokes / 18;
    int remaining_strokes = extra_strokes % 18;
    int strokes_for_this_hole = base_extra_strokes;

    if (hcp_index[hole] <= remaining_strokes) {
        strokes_for_this_hole += 1;
    }

    int net_strokes = strokes[hole] - strokes_for_this_hole;
    int p = par[hole] - net_strokes + 2;

    if (p < 0) {
        p = 0;
    }

    points[hole] = p;
}

//Returns total amount of accumulated points by reading and adding up points[].
uint32_t get_total_points(){
	uint32_t total_points = 0;
	for(int i = 0; i < 19; i++){
		total_points += points[i];
	}
	return total_points;
}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

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

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_USART2_UART_Init();
	MX_TIM2_Init();
	MX_SPI2_Init();
	/* USER CODE BEGIN 2 */
	HAL_GPIO_WritePin(SMPS_EN_GPIO_Port, SMPS_EN_Pin, GPIO_PIN_SET);
	HAL_Delay(50);

	HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);

	//Initialize display.
	DEV_Module_Init();
	EPD_2in13_V4_Init();
	EPD_2in13_V4_Clear();
	HAL_Delay(500);

	//Configure ImageBuffer and add base text on screen.
	Paint_NewImage(ImageBuffer, EPD_2in13_V4_WIDTH, EPD_2in13_V4_HEIGHT, ROTATE_90, WHITE);
	Paint_SelectImage(ImageBuffer);
	Paint_Clear(WHITE);
	Paint_DrawString_EN(0, 0, "Ulricehamn GK", &Font24, BLACK, WHITE);
	Paint_DrawString_EN(0, 26, "Extra shots:", &Font24, BLACK, WHITE);
	Paint_DrawString_EN(0, 52, "Hole:", &Font24, BLACK, WHITE);
	Paint_DrawString_EN(0, 78, "Strokes:", &Font24, BLACK, WHITE);
	Paint_DrawString_EN(0, 104, "Pts:", &Font16, BLACK, WHITE);
	Paint_DrawString_EN(135, 52, "Par:", &Font24, BLACK, WHITE);
	Paint_DrawString_EN(80, 104, "Total-Pts:", &Font16, BLACK, WHITE);
	EPD_2in13_V4_Display_Base(ImageBuffer);

	//Add a "selection line" under "Extra shots" value on screen.
	Paint_DrawLine(214, 51, 231, 51, BLACK, 1, LINE_STYLE_DOTTED);
	EPD_2in13_V4_Display_Partial(ImageBuffer);

	//Setup state variables with starting values.
	enum State st;
	st = Choosing_Number;
	enum activeVariable av;
	av = HCP;

	//Variable used to store which way the encoder was turned.
	bool bigger;
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		//Logic for encoder interrupt.
		uint32_t now;

		//Logic to get encoder value
		int32_t raw_count = (int32_t) TIM2->CNT;
		encoder_value = raw_count / 4;

		//Check if button has been pressed. If it has then toggle st between Entering_Number and Choosing_Number.
		if (last_flank_causing_exti != last_managed_exti) {
			now = HAL_GetTick();
			if (now - last_flank_causing_exti >= 20) {	//Delay to eliminate contact bounce.
				last_managed_exti = last_flank_causing_exti;
				if(st == Entering_Number){
					st = Choosing_Number;
				}else if(st == Choosing_Number){
					st = Entering_Number;
				}
			}
		}

		//Check if rotary encoder has been turned.
		if(encoder_value != last_encoder_value){
			//Check which way the encoder was turned.
			if(encoder_value > last_encoder_value){
				bigger = TRUE;
			}else{
				bigger = FALSE;
			}
			last_encoder_value = encoder_value;

			//Check if st is Choosing_Number or Entering_Number.
			switch(st){
				case Choosing_Number: //If Choosing_Number.
					//Check what number we are currently hovering over and depending on that changing av and updating the screen.
					if(av == HCP){
						av = Hole;
						Paint_ClearWindows(213, 50, 232, 52, WHITE);
						Paint_DrawLine(95, 77, 112, 77, BLACK, 1, LINE_STYLE_DOTTED);
						EPD_2in13_V4_Display_Partial(ImageBuffer);
					}else if(av == Hole){ //If we are hovering over Hole value.
						av = Strokes; //Change av to say that we are hovering over Strokes value and update display accordingly.
						Paint_ClearWindows(94, 76, 113, 78, WHITE);
						Paint_DrawLine(146, 103, 163, 103, BLACK, 1, LINE_STYLE_DOTTED);
						EPD_2in13_V4_Display_Partial(ImageBuffer);
					}else if(av == Strokes){
						av = Hole;
						Paint_ClearWindows(145, 102, 164, 104, WHITE);
						Paint_DrawLine(95, 77, 112, 77, BLACK, 1, LINE_STYLE_DOTTED);
						EPD_2in13_V4_Display_Partial(ImageBuffer);
					}
					break;
				case Entering_Number: //If Entering_Number.
					//Check what number we have selected and increase/decrease that number by 1 and update display accordingly.
					if(av == HCP){
						//Increase or decrease value depending on rotation direction.
						if(bigger){extra_strokes += 1;}else if(extra_strokes > 0){extra_strokes -= 1;}
						//Update display.
						Paint_ClearWindows(214, 26, 248, 50, WHITE);
						Paint_DrawNum(214, 26, extra_strokes, &Font24, WHITE, BLACK);
						EPD_2in13_V4_Display_Partial(ImageBuffer);
					}else if(av == Hole){
						if(bigger && hole < 18){hole += 1;}else if(hole > 1){hole -= 1;}
						Paint_ClearWindows(95, 52, 129, 76, WHITE);
						Paint_ClearWindows(146, 78, 180, 102, WHITE);
						Paint_ClearWindows(213, 52, 247, 76, WHITE);
						Paint_DrawNum(95, 52, hole, &Font24, WHITE, BLACK);
						Paint_DrawNum(146, 78, strokes[hole], &Font24, WHITE, BLACK);
						Paint_DrawNum(213, 52, par[hole], &Font24, WHITE, BLACK);
						EPD_2in13_V4_Display_Partial(ImageBuffer);
					}else if(av == Strokes){
						if(bigger){strokes[hole] += 1;}else if(strokes[hole] > 0){strokes[hole] -= 1;}
						Paint_ClearWindows(146, 78, 180, 102, WHITE);
						Paint_DrawNum(146, 78, strokes[hole], &Font24, WHITE, BLACK);
						EPD_2in13_V4_Display_Partial(ImageBuffer);
					}
					break;
			}

			//If we have changed Hole or Strokes value then calculate points and update the display.
			if(av == Hole || av == Strokes){
				calculate_stableford_points();
				Paint_ClearWindows(49, 104, 71, 120, WHITE);
				Paint_ClearWindows(195, 104, 217, 120, WHITE);
				Paint_DrawNum(49, 104, points[hole], &Font16, WHITE, BLACK);
				Paint_DrawNum(195, 104, get_total_points(), &Font16, WHITE, BLACK);
				EPD_2in13_V4_Display_Partial(ImageBuffer);
			}

		}
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Configure the main internal regulator output voltage
	 */
	if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1)
			!= HAL_OK) {
		Error_Handler();
	}

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLM = 1;
	RCC_OscInitStruct.PLL.PLLN = 10;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
	RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
	RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief SPI2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI2_Init(void) {

	/* USER CODE BEGIN SPI2_Init 0 */

	/* USER CODE END SPI2_Init 0 */

	/* USER CODE BEGIN SPI2_Init 1 */

	/* USER CODE END SPI2_Init 1 */
	/* SPI2 parameter configuration*/
	hspi2.Instance = SPI2;
	hspi2.Init.Mode = SPI_MODE_MASTER;
	hspi2.Init.Direction = SPI_DIRECTION_2LINES;
	hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi2.Init.NSS = SPI_NSS_SOFT;
	hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
	hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi2.Init.CRCPolynomial = 7;
	hspi2.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
	hspi2.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
	if (HAL_SPI_Init(&hspi2) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN SPI2_Init 2 */

	/* USER CODE END SPI2_Init 2 */

}

/**
 * @brief TIM2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM2_Init(void) {

	/* USER CODE BEGIN TIM2_Init 0 */

	/* USER CODE END TIM2_Init 0 */

	TIM_Encoder_InitTypeDef sConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };

	/* USER CODE BEGIN TIM2_Init 1 */

	/* USER CODE END TIM2_Init 1 */
	htim2.Instance = TIM2;
	htim2.Init.Prescaler = 0;
	htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim2.Init.Period = 4294967295;
	htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
	sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
	sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
	sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
	sConfig.IC1Filter = 10;
	sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
	sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
	sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
	sConfig.IC2Filter = 10;
	if (HAL_TIM_Encoder_Init(&htim2, &sConfig) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM2_Init 2 */

	/* USER CODE END TIM2_Init 2 */

}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void) {

	/* USER CODE BEGIN USART2_Init 0 */

	/* USER CODE END USART2_Init 0 */

	/* USER CODE BEGIN USART2_Init 1 */

	/* USER CODE END USART2_Init 1 */
	huart2.Instance = USART2;
	huart2.Init.BaudRate = 115200;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart2.Init.OverSampling = UART_OVERSAMPLING_16;
	huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart2) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART2_Init 2 */

	/* USER CODE END USART2_Init 2 */

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	/* USER CODE BEGIN MX_GPIO_Init_1 */

	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOA, SMPS_EN_Pin | SMPS_V1_Pin | SMPS_SW_Pin,
			GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOB, EPD_CS_Pin | EPD_DC_Pin, GPIO_PIN_SET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(LD4_GPIO_Port, LD4_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(EPD_RST_GPIO_Port, EPD_RST_Pin, GPIO_PIN_SET);

	/*Configure GPIO pin : B1_Pin */
	GPIO_InitStruct.Pin = B1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : SMPS_EN_Pin SMPS_V1_Pin SMPS_SW_Pin */
	GPIO_InitStruct.Pin = SMPS_EN_Pin | SMPS_V1_Pin | SMPS_SW_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pin : SMPS_PG_Pin */
	GPIO_InitStruct.Pin = SMPS_PG_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(SMPS_PG_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : EPD_CS_Pin EPD_DC_Pin */
	GPIO_InitStruct.Pin = EPD_CS_Pin | EPD_DC_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/*Configure GPIO pin : LD4_Pin */
	GPIO_InitStruct.Pin = LD4_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(LD4_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : EPD_RST_Pin */
	GPIO_InitStruct.Pin = EPD_RST_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(EPD_RST_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : EPD_BUSY_Pin */
	GPIO_InitStruct.Pin = EPD_BUSY_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(EPD_BUSY_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : RE_SW_Pin */
	GPIO_InitStruct.Pin = RE_SW_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(RE_SW_GPIO_Port, &GPIO_InitStruct);

	/* EXTI interrupt init*/
	HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

	/* USER CODE BEGIN MX_GPIO_Init_2 */

	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
	}
	/* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
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
