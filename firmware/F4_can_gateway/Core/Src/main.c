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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
    uint32_t id;
    uint8_t dlc;
    uint8_t data[8];
} TrameCAN;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TAILLE_FILE 32
#define TAILLE_LIGNE 64
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
volatile TrameCAN file[TAILLE_FILE];
volatile uint8_t tete = 0;
volatile uint8_t queue = 0;
volatile uint32_t trames_perdues = 0;

uint8_t octet_recu;
char ligne_pc[TAILLE_LIGNE];
volatile uint8_t pos_ligne = 0;
volatile uint8_t ligne_prete = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CAN1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
static void traiter_commande(char *cmd);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rxHeader;
    uint8_t data[8];

    while (HAL_CAN_GetRxFifoFillLevel(hcan, CAN_RX_FIFO0) > 0)
    {
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rxHeader, data) != HAL_OK)
            break;

        uint8_t suivant = (tete + 1) % TAILLE_FILE;
        if (suivant == queue) {
            trames_perdues++;
            continue;
        }

        file[tete].id = rxHeader.StdId;
        file[tete].dlc = rxHeader.DLC;
        for (int i = 0; i < 8; i++) file[tete].data[i] = data[i];
        tete = suivant;
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        if (!ligne_prete)
        {
            if (octet_recu == '\r' || octet_recu == '\n') {
                if (pos_ligne > 0) {
                    ligne_pc[pos_ligne] = '\0';
                    ligne_prete = 1;
                    pos_ligne = 0;
                }
            } else if (pos_ligne < TAILLE_LIGNE - 1) {
                ligne_pc[pos_ligne++] = octet_recu;
            }
        }
        HAL_UART_Receive_IT(&huart2, &octet_recu, 1);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        HAL_UART_Receive_IT(&huart2, &octet_recu, 1);
    }
}
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

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_CAN1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  CAN_FilterTypeDef filtre = {0};
  filtre.FilterBank = 0;
  filtre.FilterMode = CAN_FILTERMODE_IDMASK;
  filtre.FilterScale = CAN_FILTERSCALE_32BIT;
  filtre.FilterFIFOAssignment = CAN_RX_FIFO0;
  filtre.FilterActivation = ENABLE;
  filtre.SlaveStartFilterBank = 14;
  if (HAL_CAN_ConfigFilter(&hcan1, &filtre) != HAL_OK) Error_Handler();

  if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) Error_Handler();
  if (HAL_CAN_Start(&hcan1) != HAL_OK) Error_Handler();

  char debut[] = "# F4 CAN analyzer pret\r\n";
  HAL_UART_Transmit(&huart2, (uint8_t*)debut, strlen(debut), 100);

  HAL_UART_Receive_IT(&huart2, &octet_recu, 1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	    while (queue != tete)
	    {
	        TrameCAN t;
	        t.id = file[queue].id;
	        t.dlc = file[queue].dlc;
	        for (int i = 0; i < 8; i++) t.data[i] = file[queue].data[i];
	        queue = (queue + 1) % TAILLE_FILE;

	        char ligne[48];
	        int n = sprintf(ligne, "R %03lX %d", t.id, t.dlc);
	        for (int i = 0; i < t.dlc; i++) {
	            n += sprintf(ligne + n, " %02X", t.data[i]);
	        }
	        n += sprintf(ligne + n, "\r\n");

	        HAL_UART_Transmit(&huart2, (uint8_t*)ligne, n, 100);
	        HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_12);
	    }
	    if (ligne_prete)
	    {
	        char commande[TAILLE_LIGNE];
	        strcpy(commande, ligne_pc);
	        ligne_prete = 0;
	        traiter_commande(commande);
	    }
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief CAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 6;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_11TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = ENABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = ENABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */

  /* USER CODE END CAN1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

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
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
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
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_RESET);

  /*Configure GPIO pin : PD12 */
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
static void traiter_commande(char *cmd)
{
    char rep[64];

    if (cmd[0] == 'T' && cmd[1] == ' ')
    {
        char *p = cmd + 2;
        char *fin;

        uint32_t id = strtoul(p, &fin, 16);
        if (fin == p || id > 0x7FF) {
            strcpy(rep, "# ERR identifiant\r\n");
            HAL_UART_Transmit(&huart2, (uint8_t*)rep, strlen(rep), 100);
            return;
        }
        p = fin;

        uint32_t dlc = strtoul(p, &fin, 10);
        if (fin == p || dlc > 8) {
            strcpy(rep, "# ERR longueur\r\n");
            HAL_UART_Transmit(&huart2, (uint8_t*)rep, strlen(rep), 100);
            return;
        }
        p = fin;

        uint8_t data[8] = {0};
        for (uint32_t i = 0; i < dlc; i++) {
            uint32_t v = strtoul(p, &fin, 16);
            if (fin == p || v > 0xFF) {
                strcpy(rep, "# ERR donnees\r\n");
                HAL_UART_Transmit(&huart2, (uint8_t*)rep, strlen(rep), 100);
                return;
            }
            data[i] = (uint8_t)v;
            p = fin;
        }

        CAN_TxHeaderTypeDef txHeader = {0};
        uint32_t txMailbox;
        txHeader.StdId = id;
        txHeader.IDE = CAN_ID_STD;
        txHeader.RTR = CAN_RTR_DATA;
        txHeader.DLC = dlc;

        if (HAL_CAN_AddTxMessage(&hcan1, &txHeader, data, &txMailbox) == HAL_OK) {
            sprintf(rep, "# OK T %03lX\r\n", id);
        } else {
            strcpy(rep, "# ERR bus occupe\r\n");
        }
    }
    else
    {
        strcpy(rep, "# ERR commande inconnue\r\n");
    }

    HAL_UART_Transmit(&huart2, (uint8_t*)rep, strlen(rep), 100);
}
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
