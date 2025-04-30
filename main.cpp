/**
 * @file           : main.c
 * @brief          : Main program body
 * @author				 : Vladimir Vakhter (vvvakhter@wpi.edu)
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 WPI ICAS Lab
 * All rights reserved.
 * Not to be disclosed or used without permission of ICAS Lab
 *
 ******************************************************************************
 */

/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

#include "dma.h"
#include "ff.h"
#include "gpio.h"
#include "i2c.h"
#include "icas/lifetime_correction.h"
#include "icas/profiling.h"
#include "icas/timestamp.h"
#include "ipcc.h"
#include "quadspi.h"
#include "rtc.h"
#include "tim.h"
#include "usart.h"
#include <chrono>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdarg.h>
#include <stdio.h>

#include "stm32wb35xx.h"

extern "C"
{
#include "adi/adi-engineer-zone/adi_adpd_ssm.h"
#include "icas/adpd4102_dcfg_ptco2.h"
#include "icas/adxl367.h"
#include "icas/cli.h"
#include "icas/filesystem/diskio_test.h"
#include "icas/measurement-settings.h"
#include "icas/qspi/flash_w25q32jv.h"
#include "icas/tau_calculations.h"
#include "icas/temp_tmp117.h"
#include "icas/test_functions.h"
#include "icas/temp_compensation.h"
}

#include "icas/data_saver.h"
#include "icas/filesystem/filesystem.h"
#include "icas/mono_exp_decay.h"
// #include "icas/profiling.h"
#include "icas/tui/tui.h"

// #include "icas/lifetime_correction.h"

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

/* USER CODE BEGIN PV */

/* ---------------------- state of the patch ---------------------- */
bool display_device_state = false;
StateTOM device_state = POWERED_OFF_TOM_STATE;

/* ---------------------- measuring execution time of specific code snippets ---------------------- */
#ifdef MEASURE_MS_TIME_INTERVAL
uint32_t start_ms = 0;
uint32_t stop_ms = 0;
uint32_t delta_ms = 0;
#endif

/* ---------------------- LP mode used in the system ---------------------- */
LP_Mode lp_mode = USE_STOP2_MODE;

/* ---------------------- decay measurement variables ---------------------- */
uint8_t decay_curve_num = 0;               // counter of decay curves within a measurement
static tAdiAdpdSSmInst oAdiAppInst;        // ADPD state machine global structure instance
uint8_t aFifoDataBuf[MAX_SAMPLES_IN_FIFO]; // FIFO content storage
uint32_t gAdpdDataReady = 0;               // FIFO data ready flag
mono_exp_decay_data exp_decay_data;

#ifdef VERBOSE_OUT
ADI_ADPD_COMM_MODE bus_mode; // communication interface between ADPD and MCU
#endif

uint32_t numMeasurements = 1;     // number of measurement to be taken
uint32_t numMeasurementsCntr = 0; // counter of measurements
uint16_t nAdpdFifoLevelSize;      // size of the data available in ADPD's FIFO
uint16_t decay_curve_new[NUM_INT] = {};
uint8_t tx_buf_ext_flash[TX_BUF_FLASH_SIZE] = {};
uint16_t loop = 0U;
uint8_t total_num_decay_curves;

uint32_t terminate_experiment_at_secs = 0;

#if defined(VERIFY_CFG_LOAD) || defined(READ_DEFAULT_CFG) || defined(VERIFY_CFG_PERIODIC_DATA_ISSUE)
uint8_t i2cDataExchBuffer[20]; // buffer for the I2C data exchange
#endif

/* ---------------------- debouncing ---------------------- */
bool disp_btn_debounce = true; // display button debouncing flag (updated automatically)

/* ---------------------- CLI ---------------------- */
int scanfReturnStatus = -1;                           // return status of scanf()
int course_of_action = LAUNCH_NEW_MEASUREMENTS;       // defines how the system will proceed
int measurement_settings = USE_DEFAULT_MEAS_SETTINGS; // defines the settings of experiments

/* ---------------------- temperature measurements (tmp117) ---------------------- */
float decay_curve_avg_temp = 0;    // temperature value for a given decay curve
uint16_t temp_twos_comp_value = 0; // the temperature in the two's compliment format
uint16_t temp_reg_value;
uint8_t buffer_i2c_temp[20];

/* ---------------------- acceleration measurements (adxl367) ---------------------- */
struct adxl367_dev_stm32* accel_dev_desc;
int16_t g_x_raw, g_y_raw, g_z_raw = 0;             // raw values of acceleration
struct adxl367_fractional_val_stm32 g_x, g_y, g_z; // converted (float) values of acceleration
// enum adxl367_range accel_range = ADXL367_2G_RANGE;
enum adxl367_range accel_range = ADXL367_8G_RANGE;

/* ---------------------- external Flash memory (w25q32jv) operations ---------------------- */
uint32_t flashAddress = 0x000000;     // current flash address
uint32_t num_data_bytes_in_ext_flash; // the total number of data bytes in the external Flash

/* ---------------------- For filesystem ---------------------- */
FATFS fso;          // Filesystem object
FIL fil;            // File object for current measurement session
DWORD fileslot = 0; // For manual measurements
// NOTE: For auto-measurements, we save the results to "/measurements/auto"
uint32_t written_measurements = 0;
// Working file path
TCHAR fil_path[64];
// Current datapoint
save_datapoint_t datapoint{};

/* ---------------------- For signal processing ---------------------- */
centered_moving_average* tau_cma_calc;

/* ---------------------- Timestamp ---------------------- */
stopwatch_t sw{};

/* ---------------------- calibration ---------------------- */
uint16_t luminescence_base_value = 0; // the "dark" base luminescence value with no film

/* ---------------------- globals for tau calculations ---------------------- */
// short 		combined_curve[2*NUM_INT] = {0};		// Interwoven waveforms
// uint16_t 	decay_curve[NUM_INT] = {0};			// blue LED, Slot A, channel 1 waveform no offset
// uint16_t 	decay_curve_offset[NUM_INT] = {0};	// waveform offset 1us
// uint16_t	combined_decay[COMBINED_DECAY_SIZE] = {0};
// uint16_t 	decay_portion[ARRAY_SIZE] = {0};
// int				decay_size = 0;
//  FIXME: tau calculations - only for capture multiple times with a shift
//	int		count_runs = 0;							// up to 100
//	bool	readyToProcess = false;
// float 		y_data[ARRAY_SIZE] = {0};	// Normally int, float for ADI

// the base "dark measurement" value (obtained with no film)
// uint16_t dark_data_tom003f_12[NUM_INT] = {
//     6771, 6760, 6758, 6755, 6757, 6756, 6756, 6755, 6754, 6755, 6756, 6754, 6755, 6755, 6754, 6753,
//     6754, 6755, 6755, 6755, 6755, 6755, 6754, 6754, 6755, 6757, 6755, 6754, 6753, 6755, 6754, 6754,
//     6753, 6754, 6754, 6754, 6756, 6756, 6755, 6756, 6754, 6754, 6753, 6754, 6754, 6754, 6755, 6754,
//     6755, 6757, 6757, 6756, 6755, 6753, 6753, 6754, 6752, 6754, 6753, 6755, 6753, 6755, 6754, 6754,
//     6755, 6754, 6755, 6755, 6755, 6754, 6755, 6753, 6754, 6753, 6754, 6754, 6755, 6753, 6756, 6755};
uint16_t dark_data_tom003f_12[NUM_INT] = {6771, 6760, 6758, 6755, 6757, 6756, 6756, 6755, 6754, 6755, 6756, 6754,
                                          6755, 6755, 6754, 6753, 6754, 6755, 6755, 6755, 6755, 6755, 6754, 6754,
                                          6755, 6757, 6755, 6754, 6753, 6755, 6754, 6754, 6753, 6754, 6754, 6754,
                                          6756, 6756, 6755, 6756, 6754, 6754, 6753, 6754, 6754, 6754, 6755, 6754,
                                          6755, 6757, 6757, 6756, 6755, 6753, 6753, 6754, 6752, 6754, 6753, 6755};
uint16_t dark_data_base_value = 6755; // average of data points from 'dark_data_tom003f_12[]'
float calibrated_lifetime;
float time_step_us = 2.0f; // 2us for 500kHz data rate (1us for interwoven waveforms as Evan and Kleo did)

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
/* USER CODE BEGIN PFP */
void Configure_PWR(void);
void SYSCLKConfig_STOP(void);

/* CLI */
void configureStdinCLI(void);
void showWelcomeMsgCLI(void);
int selectCourseOfActionCLI(void);

/* Course of user's action */
int dumpPrevResultsFromExtFlash(void);
int startNewMeasurements(void);
int calibrateSensor(void);
int deleteMeasurement(void);
int reformatDisk(void);
int listMeasurements(void);

/* MCU */
void softResetMCU(void);

/* LP modes */
void enterLPMode(LP_Mode lp_mode); // a wrapper for all LP modes
void enterSleepMode(void);
void enterLPSleepMode(void);
void enterStop2Mode(void);
void enterStandbyMode(void);

/* Indication (LED) */
void indicateDevicePowerOn(void);
void indicateExperimentBoundary(void);
void indicatePeriodicityInLuminescenceData(void);
void indicateError(void);
void displayCurrentDeviceState(void);

/* Accelerometer */
void initADXL367(void);

/* Temperature sensor */
void initTMP117(void);

/*Temperature compensation */
float get_scaling_factor(float);



/* AFE */
int initADPD4102(void);

/* External Flash */
int initW25Q32J(void);

/* Monitor the battery status */
void monitorVbat(void); // TODO: this function is not implemented

/* Utility functions */
void printResults();

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* IPCC initialisation */
  MX_IPCC_Init();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  MX_RTC_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_TIM16_Init();
  MX_QUADSPI_Init();
  /* USER CODE BEGIN 2 */

  // Run profiling accuracy tests
  // test_timing_accuracy();

  // Run matrix class tests
  // run_dmatrix_tests();

  /* Uncomment if you made a change to the I/O layer of FatFs and want to test if everything works. This will
   wipe-out the entire flash */
  // diskio_test();

  device_state = POWERED_ON_TOM_STATE;   // update the state of the TOM patch
  enable_power_for_PtCO2_measurements(); // enable voltage regulators for PtcO2
  configureStdinCLI();                   // configure stdin for CLI

  // VT100 escape code: reset terminal to initial state
  printf("%cc", ASCII_ESC);
  // Try to mount the default logical drive, if it does not exist try to create a FAT volume
  FRESULT res = try_mount_and_create_fs_if_not_present(&fso, "", 1);
  if (res)                // Mounting or fs creation failed, enable FS_VERBOSE to debug
    enterLPMode(lp_mode); // Enter LP mode because we do not have a filesystem to work with

  // Create directory for measurements, we'll get FR_EXIST if we already have one
  // FRESULT mkdir_res = f_mkdir("0:/measurements");
  // if (mkdir_res != FR_OK)
  //{
  //  if (mkdir_res != FR_EXIST)
  //  {
  //    printf("> Could not create measurements directory: %d\r\n", mkdir_res);
  //    enterLPMode(lp_mode);
  //  }
  //  // else: "measurements" directory already exists, nothing to do
  //}
  res = create_measurement_dirs();
  if (res)
    enterLPMode(lp_mode);

#ifdef LAUNCH_MEASUREMENTS_MANUALLY
  showWelcomeMsgCLI(); // welcome the user
  if (selectCourseOfActionCLI() == ADI_HAL_ERROR)
  {
    return 1; // prompt the user to select how to proceed
  }
#else
  course_of_action = LAUNCH_NEW_MEASUREMENTS; // automatically start new measurements
#endif
  // displayCurrentDeviceState(); // display the current AFE's status

  // proceed according to the user's input/preference
  switch (course_of_action)
  {
  case DUMP_EXT_FLASH_PREV_RES: {
    dump_prev_measurements();
    printf("> Press any key to restart the MCU.\r\n");
    consume_key();
    NVIC_SystemReset();
    return 0;
  }
  case LAUNCH_NEW_MEASUREMENTS: {

    if (startNewMeasurements() == ADI_HAL_ERROR)
    {
      return 1;
    }
    break;
  }

  case CALIBRATE_SENSOR: {
    if (calibrateSensor() == ADI_HAL_ERROR)
    {
      return 1;
    }
    break;
  }
  case REFORMAT_DISK: {
    printf("> Trying to reformat disk...\r\n");
    FRESULT res = make_filesystem("0:");
    if (res)
    {
      return 1;
    }
    printf("\x1b[5;32mSuccessfully reformatted the disk.\x1b[0;0m\r\n");
    printf("> Now resetting the MCU.\r\n");
    HAL_Delay(1500);
    NVIC_SystemReset(); // Reset MCU
    return 0;
  }
  case EXPLORE_DISK: {
    list_dir_tree("0:");
    printf("> Press any key to restart the MCU.\r\n");
    consume_key();
    NVIC_SystemReset();
    return 0;
  }
  default:
    printf("> Invalid course of action selected\r\n");
    return 1;
  }

  total_num_decay_curves = (course_of_action == LAUNCH_NEW_MEASUREMENTS) ? NUM_DECAY_CURVES
                           : (course_of_action == CALIBRATE_SENSOR)      ? NUM_MEASUREMENTS_FOR_CALIBRATION
                                                                         : NUM_DECAY_CURVES;

  // Create centered moving average
  tau_cma_calc = create_centered_moving_average(5);

  // Path to the created file
  FRESULT open_res = f_open(&fil, fil_path, FA_WRITE);
  if (open_res)
  {
    printf("Could not open the measurement file: %d\r\n", open_res);
    enterLPMode(lp_mode);
  }
  // Move the file pointer 2 bytes forward (skip version)
  f_lseek(&fil, 2);

  opt_context tau_ctx(3); // Optimization context with three parameters
  opt_config tau_cfg{};
  opt_lma_config lma_cfg{};
  opt_gna_config gna_cfg{};

  // TODO: Move this to a dedicated structure (like a lifetime_solver or something)
  tau_cfg.algorithm = OPT_LEVENBERG_MARQUARDT;
  tau_cfg.algorithm_config = &lma_cfg;
  tau_cfg.max_iterations = 1000;
  tau_cfg.min_update_step = 5e-4f;

  tau_ctx.iteration_terms = &mono_exp_iteration_terms_optimized_T<LEVENBERG_MARQUARDT_TERMS_MASK>;
  tau_ctx.residual = &mono_exp_residuals;
  tau_ctx.jacobian = &mono_exp_jacobian;
  tau_ctx.gradient = &mono_exp_gradient;
  tau_ctx.loss = &mono_exp_loss;
  tau_ctx.fit_data = &exp_decay_data;
  tau_ctx.n_data = NUM_INT;
  tau_ctx.n_params = 3;

  // Ambient air parameters
  tau_ctx.params.at(0) = 0.2088f;
  tau_ctx.params.at(1) = 0.9279f;
  tau_ctx.params.at(2) = -3.2117f;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (true)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if ((course_of_action == LAUNCH_NEW_MEASUREMENTS) || (course_of_action == CALIBRATE_SENSOR))
    {
      if (gAdpdDataReady == 1)
      { // on receive of an IRQ from AFE (GPIO0)

#ifdef MEASURE_MS_PROCESSING_PER_DECAY_CURVE
        start_ms = HAL_GetTick();
#endif

        // put ADPD4102 in the IDLE mode
#ifdef VERBOSE_OUT
        printf("Put ADPD4102 into the IDLE Mode: ");
#endif
        if (adi_adpdssm_setOperationMode(E_ADI_ADPD_MODE_IDLE) != ADI_ADPD_DRV_SUCCESS)
        {
          printf("Error: Cannot set ADPD4102 to the IDLE mode.\r\n");
        }
#ifdef VERBOSE_OUT
        else
          printf("OK.\r\n\n");
#endif

        // reset the IRQ flag
        gAdpdDataReady = 0;

        // not using ADPD400x auto clear int flag -> need to clear
        adi_adpddrv_RegWrite(ADPD4x_REG_INT_STATUS_DATA, 0x8000); // register 0x0001 (INT_STATUS_DATA) of ADPD4102, bit
                                                                  // 15 (INT_FIFO_TH): write one to clear the interrupt

        // read temperature
        if (getResultTemperature1(&hi2c1, buffer_i2c_temp, &temp_reg_value))
        {
          decay_curve_avg_temp = (float)(decay_curve_avg_temp + temp_reg_value) / 2.0F;
          temp_twos_comp_value = (uint16_t)decay_curve_avg_temp;
        }

        // read the size of the luminescent data available in the FIFO
        if (adi_adpdssm_getFifoLvl(&nAdpdFifoLevelSize) != ADI_ADPD_DRV_SUCCESS)
        {
          printf("FIFO level: Error reading.\r\n");
          continue;
        }
#ifdef VERBOSE_OUT
        else
          printf("FIFO level: %d.\r\n", nAdpdFifoLevelSize);
#endif

        // read the luminescent data available in the FIFO
        if (adi_adpddrv_ReadFifoData(nAdpdFifoLevelSize, &aFifoDataBuf[0]) != ADI_ADPD_DRV_SUCCESS)
        {
          printf("Reading FIFO: Error.\r\n");
          continue;
        }
#ifdef VERBOSE_OUT
        else
          printf("Reading FIFO: OK.\r\n");
#endif

        // a counter to traverse through the FIFO data
        loop = 0U;

        //				// dynamically allocate an array for fluorescence values (blue LED, Slot A,
        // channel 1) - to be written in the flash 				uint16_t* decay_curve_new =
        // (uint16_t*)calloc(NUM_INT, sizeof(uint16_t)); 				if (decay_curve_new == NULL)
        // {printf("NULL array: %s\r\n", getVarName(decay_curve_new)); return 1;}	// exit (could not allocate)

        for (uint16_t i = 0; i < NUM_INT /*(nAdpdFifoLevelSize / 2)*/; i++)
        {
          // parse the data in the impulse response mode (reference: Jiayuan's code, adpd410x.c, adpd410x_read_fifo())
          decay_curve_new[i] += ((aFifoDataBuf[loop] << 8) + aFifoDataBuf[loop + 1]);

          // average data from several sweeps
          if (decay_curve_num != 0)
            decay_curve_new[i] /= 2;

          // skip the IR_MODE_DATAWIDTH bytes which have been processed already
          loop += AFE_IR_FLUO_SAMPLE_BYTES_NUM;
        }

        // check the luminescence data for periodicity
        if (adpd4102checkOutputOscillation(decay_curve_new))
        {
          // toggle yellow LED
          indicatePeriodicityInLuminescenceData();

          // print error message
          printf("Periodic data issue. Restarting the AFE...\r\n\n");

          // print results
          printResults();

          // restart AFE
          restartAFE(&oAdiAppInst, dcfg_ADPD4102_icas_ptco2);
        }

#ifdef VERBOSE_OUT
        printf("Got decay curve %d\r\n", decay_curve_num);
#endif
        // a sweep of data has been processed
        decay_curve_num++;

        if (decay_curve_num != total_num_decay_curves)
        {
          // put ADPD4102 in the GO mode
          if (adi_adpdssm_setOperationMode(E_ADI_ADPD_MODE_SAMPLE) != ADI_ADPD_DRV_SUCCESS)
          {
            printf("> Error: Cannot set ADPD4102 to the Sample mode.\r\n");
          }
          /* enter an LP mode */
          enterLPMode(lp_mode);
        }
        else
        {
          printf("\x1b[1;32m-----------------  Measurement #%u  -----------------\x1b[0;0m\r\n",
                 numMeasurementsCntr + 1);

          // Set the temperature
          datapoint.temperature = temp_twos_comp_value;
          // int iTemp = twosCompToInt(&temp_twos_comp_value);
          // float fTemp = (float)iTemp * TMP117_RESOLUTION;
          // fTemp = (int)(fTemp * 1000) / 1000.0f;

          float temperature = printTemperature(&temp_twos_comp_value);
          float scaling_factor = get_scaling_factor(temperature);
          printf("- Scaling factor : %f\r\n", scaling_factor);

          printf("Fluorescense[ADC CODE]:\r\n");
           for (uint8_t i = 0; i < NUM_INT; i++)
           {
            printf("%d\r\n", decay_curve_new[i]);
           }

           for (int i = 0; i < NUM_INT; i++)
          {
            exp_decay_data.luminescence[i] =
                (float)(decay_curve_new[i] - dark_data_tom003f_12[i]) / O2_LUMINESCENCE_SCALE;
            exp_decay_data.time[i] = (time_step_us * (float)i) / O2_TIME_SCALE;
          }
          // Save the parameters
          dmatrix_t parameters = tau_ctx.params;
          opt_state tau_state;
          duration_t d;

          // Test Gauss-Newton
          tau_ctx.iteration_terms = &mono_exp_iteration_terms_optimized_T<GAUSS_NEWTON_TERMS_MASK>;
          tau_cfg.algorithm = OPT_GAUSS_NEWTON;
          tau_cfg.algorithm_config = &gna_cfg;
          reset_profiler(&d);
          opt_solve(&tau_cfg, &tau_ctx, &tau_state);
          stop_profiler(&d);
          print_duration(&d, "Computation time");
          printf("Termination reason: %s\r\n",
                 opt_termination_reason_to_string((opt_termination_reason)tau_state.termination_reason));
          printf("Total iterations: %u\r\n", tau_state.iterations);
          printf("Loss: %f\r\n", tau_state.loss);
          dmatrix_print(tau_ctx.params, "Parameters");
          // Set the lifetime in the current datapoint
          float32_t lifetime = -O2_TIME_SCALE / tau_ctx.params.at(2);
          datapoint.lifetime = lifetime;
          printf("Lifetime_GN: %f\r\n", lifetime);
          calibrated_lifetime = lifetime * scaling_factor;
          printf("Calibrated_Lifetime_GN:  %f\r\n",calibrated_lifetime);
          printf("Measurement timestamp (ms): %d \r\n", datapoint.ts);
          // NOTE: Restore parameters;
          tau_ctx.params = parameters;
          printf("************************************\r\n");

          // Test BFGS
          tau_ctx.iteration_terms = &mono_exp_iteration_terms_optimized_T<BFGS_TERMS_MASK>;
          tau_cfg.algorithm = OPT_BFGS;
          tau_cfg.algorithm_config = nullptr;
          reset_profiler(&d);
          opt_solve(&tau_cfg, &tau_ctx, &tau_state);
          stop_profiler(&d);
          print_duration(&d, "Computation time");
          printf("Termination reason: %s\r\n",
                 opt_termination_reason_to_string((opt_termination_reason)tau_state.termination_reason));
          printf("Total iterations: %u\r\n", tau_state.iterations);
          printf("Loss: %f\r\n", tau_state.loss);
          dmatrix_print(tau_ctx.params, "Parameters");
          // Set the lifetime in the current datapoint
          lifetime = -O2_TIME_SCALE / tau_ctx.params.at(2);
          datapoint.lifetime = lifetime;
          printf("Lifetime_BFGS: %f\r\n", lifetime);
          calibrated_lifetime = lifetime * scaling_factor;
          printf("Calibrated_Lifetime_BFGS:  %f\r\n",calibrated_lifetime);
          printf("Measurement timestamp (ms): %d \r\n", datapoint.ts);
          // NOTE: Restore parameters;
          tau_ctx.params = parameters;
          printf("*****************************************\r\n");

          // Test Gradient Descent
          tau_ctx.iteration_terms = &mono_exp_iteration_terms_optimized_T<GD_TERMS_MASK>;
          tau_cfg.algorithm = OPT_GRADIENT_DESCENT;
          tau_cfg.algorithm_config = nullptr;
          reset_profiler(&d);
          opt_solve(&tau_cfg, &tau_ctx, &tau_state);
          stop_profiler(&d);
          print_duration(&d, "Computation time");
          printf("Termination reason: %s\r\n",
                 opt_termination_reason_to_string((opt_termination_reason)tau_state.termination_reason));
          printf("Total iterations: %u\r\n", tau_state.iterations);
          printf("Loss: %f\r\n", tau_state.loss);
          dmatrix_print(tau_ctx.params, "Parameters");
          // Set the lifetime in the current datapoint
          lifetime = -O2_TIME_SCALE / tau_ctx.params.at(2);
          datapoint.lifetime = lifetime;
          printf("Lifetime_GD: %f\r\n", lifetime);
          calibrated_lifetime = lifetime * scaling_factor;
          printf("Calibrated_Lifetime_GD:  %f\r\n",calibrated_lifetime);
          printf("Measurement timestamp (ms): %d \r\n", datapoint.ts);
          // NOTE: Restore parameters;
          tau_ctx.params = parameters;
          printf("*****************************************\r\n");

          // Test Gradient Descent Momentum
          tau_ctx.iteration_terms = &mono_exp_iteration_terms_optimized_T<GD_MOMENTUM_TERMS_MASK>;
          tau_cfg.algorithm = OPT_GD_MOMENTUM;
          opt_gd1o_config gd1o_cfg{};
          tau_cfg.algorithm_config = &gd1o_cfg;
          reset_profiler(&d);
          opt_solve(&tau_cfg, &tau_ctx, &tau_state);
          stop_profiler(&d);
          print_duration(&d, "Computation time");
          printf("Termination reason: %s\r\n",
                 opt_termination_reason_to_string((opt_termination_reason)tau_state.termination_reason));
          printf("Total iterations: %u\r\n", tau_state.iterations);
          printf("Loss: %f\r\n", tau_state.loss);
          dmatrix_print(tau_ctx.params, "Parameters");
          // Set the lifetime in the current datapoint
          lifetime = -O2_TIME_SCALE / tau_ctx.params.at(2);
          datapoint.lifetime = lifetime;
          printf("Lifetime_GDM: %f\r\n", lifetime);
          calibrated_lifetime = lifetime * scaling_factor;
          printf("Calibrated_Lifetime_GDM: %f\r\n",calibrated_lifetime);
          printf("Measurement timestamp (ms): %d \r\n", datapoint.ts);
          // NOTE: Restore parameters;
          tau_ctx.params = parameters;
          printf("*****************************************\r\n");

          // Test Gradient Descent Adam
          tau_ctx.iteration_terms = &mono_exp_iteration_terms_optimized_T<GD_ADAM_TERMS_MASK>;
          tau_cfg.algorithm = OPT_GD_ADAM;
          opt_gd1o_config adam_cfg{};
          tau_cfg.algorithm_config = &adam_cfg;
          reset_profiler(&d);
          opt_solve(&tau_cfg, &tau_ctx, &tau_state);
          stop_profiler(&d);
          print_duration(&d, "Computation time");
          printf("Termination reason: %s\r\n",
                 opt_termination_reason_to_string((opt_termination_reason)tau_state.termination_reason));
          printf("Total iterations: %u\r\n", tau_state.iterations);
          printf("Loss: %f\r\n", tau_state.loss);
          dmatrix_print(tau_ctx.params, "Parameters");
          // Set the lifetime in the current datapoint
          lifetime = -O2_TIME_SCALE / tau_ctx.params.at(2);
          datapoint.lifetime = lifetime;
          printf("Lifetime_GDA: %f\r\n", lifetime);
          calibrated_lifetime = lifetime * scaling_factor;
          printf("Calibrated_Lifetime_GDA:  %f\r\n",calibrated_lifetime);
          printf("Measurement timestamp (ms): %d \r\n", datapoint.ts);
          // NOTE: Restore parameters;
          tau_ctx.params = parameters;
          printf("*****************************************\r\n");

          // Test Levenberg-Marquardt
          tau_ctx.iteration_terms = &mono_exp_iteration_terms_optimized_T<LEVENBERG_MARQUARDT_TERMS_MASK>;
          tau_cfg.algorithm = OPT_LEVENBERG_MARQUARDT;
          tau_cfg.algorithm_config = &lma_cfg;
          reset_profiler(&d);
          opt_solve(&tau_cfg, &tau_ctx, &tau_state);
          stop_profiler(&d);
          print_duration(&d, "Computation time");
          printf("Termination reason: %s\r\n",
                 opt_termination_reason_to_string((opt_termination_reason)tau_state.termination_reason));
          printf("Total iterations: %u\r\n", tau_state.iterations);
          printf("Loss: %f\r\n", tau_state.loss);
          dmatrix_print(tau_ctx.params, "Parameters");
          // Set the lifetime in the current datapoint
          lifetime = -O2_TIME_SCALE / tau_ctx.params.at(2);
          datapoint.lifetime = lifetime;
          printf("Lifetime_LM: %f\r\n", lifetime);
          calibrated_lifetime = lifetime * scaling_factor;
          printf("Calibrated_Lifetime_LM:  %f\r\n",calibrated_lifetime);
          printf("Measurement timestamp (ms): %d \r\n", datapoint.ts);
          // Do not restore parameters here, as the next measurement will give a different decay curve


          // TODO: Write to flash not so often. We want to watch out to not wear out the FAT sector
          UINT bw;
          f_write(&fil, &datapoint, sizeof(datapoint), &bw);
          if (bw != sizeof(datapoint))
          {
            printf("> Writing to file has failed. Aborting experiment \r\n");
            enterLPMode(lp_mode);
          }

          // Sync the data every N measurements
          if (numMeasurementsCntr % 64 == 0)
          {
            FRESULT sync = f_sync(&fil);
            if (sync)
            {
              printf("> Sync has failed. Aborting experiment.\r\n");
              enterLPMode(lp_mode);
            }
            printf("> Synced working file to disk.\r\n");
          }

          bool max_measurements_reached = numMeasurementsCntr == (numMeasurements - 1);
          bool experiment_max_duration_reached = stopwatch_get_s(&sw) > terminate_experiment_at_secs;
          // If the experiment ends before the current page gets full, save it upto the valid point and enter LP mode.
          if (max_measurements_reached || experiment_max_duration_reached)
          {
            printf("-------------------End of Measurement---------------\r\n");
            FRESULT sync = f_sync(&fil);
            if (sync)
            {
              printf("> Sync has failed, last measurements could not be written.\r\n");
              enterLPMode(lp_mode);
            }
            printf("> Synced working file to disk.\r\n");
            device_state = STOPPED_TOM_STATE;
            displayCurrentDeviceState();
          }
          else
          {
            printf("-------------------End of Measurement---------------\r\n");
            printf("\r\n");
            // Reset the decay curve number
            decay_curve_num = 0;
            ++numMeasurementsCntr;

#ifdef ADD_HAL_DELAY_BETWEEN_MEASUREMENTS
            HAL_Delay(DELAY_BETWEEN_MEASUREMENTS_MS);
#endif
            datapoint.ts = (uint32_t)stopwatch_get_ms(&sw);
            if (adi_adpdssm_setOperationMode(E_ADI_ADPD_MODE_SAMPLE) != ADI_ADPD_DRV_SUCCESS)
            {
              printf("Error: Cannot set ADPD4102 to the Sample mode.\r\n");
            }
          }
          // Reset the decay curve buffer
          memset(decay_curve_new, 0, NUM_INT * sizeof(uint16_t));
          enterLPMode(lp_mode);
        }
      }
      if (display_device_state)
        displayCurrentDeviceState(); // show TOM's state on a display button pressed event
      if (decay_curve_num != total_num_decay_curves)
        enterLPMode(lp_mode); // enter an LP mode
    }
  } // end of while(1), and the next curly brace is the end of main()
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

  /** Macro to configure the PLL multiplication factor
   */
  __HAL_RCC_PLL_PLLM_CONFIG(RCC_PLLM_DIV2);
  /** Macro to configure the PLL clock source
   */
  __HAL_RCC_PLL_PLLSOURCE_CONFIG(RCC_PLLSOURCE_HSE);
  /** Configure LSE Drive Capability
   */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);
  /** Configure the main internal regulator output voltage
   */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType =
      RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_LSE | RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_10;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure the SYSCLKSource, HCLK, PCLK1 and PCLK2 clocks dividers
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK4 | RCC_CLOCKTYPE_HCLK2 | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV16;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.AHBCLK2Divider = RCC_SYSCLK_DIV16;
  RCC_ClkInitStruct.AHBCLK4Divider = RCC_SYSCLK_DIV16;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
 * @brief Peripherals Common Clock Configuration
 * @retval None
 */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
   */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SMPS;
  PeriphClkInitStruct.SmpsClockSelection = RCC_SMPSCLKSOURCE_HSI;
  PeriphClkInitStruct.SmpsDivSelection = RCC_SMPSCLKDIV_RANGE1;

  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN Smps */

  /* USER CODE END Smps */
}

/* USER CODE BEGIN 4 */

/*  Declarations for callbacks
 * ----------------------------------------------------------------------------------------------------------------- */
/**
 *  @brief    Callback function for ADPD4102 FIFO.
 *  @param    None
 *  @return   None
 */
static void AdpdFifoCallback()
{
  gAdpdDataReady = 1;

#ifdef MEASURE_TIME_INTERVAL
  stop_ms = HAL_GetTick();
  delta_ms = stop_ms - start_ms;
  printf("\nStart, ms: %lu\r\n", start_ms);
  printf("Stop, ms: %lu\r\n", stop_ms);
  printf("Delta, ms: %lu\r\n\n", delta_ms);
#endif
}

/**
 *  @brief    Callback: timer has reset
 *  @param    htim - pointer to a timer
 *  @return   None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
  // Check which version of the timer triggered this callback
  if (htim == &htim16)
  {
    // after 50 ms, we check if the display button is in reset state or released
    if (HAL_GPIO_ReadPin(DISP_GPIO_Port, DISP_Pin) == GPIO_PIN_RESET)
    {
      // useful workload
      //			test_leds();
      //			displayCurrentDeviceState();
      display_device_state = true;   // enable displaying the current device status
      disp_btn_debounce = true;      // enable debouncing for the next event
      HAL_TIM_Base_Stop_IT(&htim16); // stop timer 16 generation in interrupt mode
    }
  }
  else
  {
    __NOP();
  }
}

/**
 *  @brief    Callback: external hardware interrupt occured
 *  @param    GPIO_Pin - pin number
 *  @return   None
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  // Display button press triggered this callback
  if ((GPIO_Pin == DISP_Pin) && disp_btn_debounce)
  {
    // Debounce the display button
    HAL_TIM_Base_Start_IT(&htim16); // start timer 16 with an interrupt
    disp_btn_debounce = false;      // ensure that bouncing will not restart the timer
    // GPIO0 of ADPD4102 triggered this callback
  }
  else if (GPIO_Pin == GPIO0_AFE_Pin)
  {
    AdpdFifoCallback();
  }
  else
  {
    __NOP();
  }
}

/*  Declarations for ADPD4102 driver functions
 * ----------------------------------------------------------------------------------------------------------------- */
uint16_t Adpd400x_SPI_Transmit(uint8_t* pData, uint16_t Size)
{
  // SPI is not used/initialized in our application
  return ADI_HAL_ERROR;
}

uint16_t Adpd400x_SPI_Receive(uint8_t* pTxData, uint8_t* pRxData, uint16_t TxSize, uint16_t RxSize)
{
  // SPI is not used/initialized in our application
  return ADI_HAL_ERROR;
}

/*
 * @brief  	transmit data via I2C
 * @param 	tx_buf		pointer to the Tx buffer
 * @param		tx_size		size of the Tx buffer in bytes
 * @return	ADI_HAL_OK (= 0) if the data exchange was successful,
 * 					ADI_HAL_ERROR (= -1) - otherwise
 */
uint16_t Adpd400x_I2C_Transmit(uint8_t* tx_buf, uint16_t tx_size)
{
  HAL_StatusTypeDef ret; // return status of i2c

  // transmit the data to the sensor
  ret = HAL_I2C_Master_Transmit(&hi2c1, ADPD4102_I2C_ADDR, // blocking function
                                tx_buf, tx_size, HAL_MAX_DELAY);

  if (ret != HAL_OK)
  {
    char uart_buf[50];
    int uart_buf_len;
    uart_buf_len = sprintf(uart_buf, "Error Tx I2C\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, uart_buf_len, 100);
    return ADI_HAL_ERROR;
  }

  return ADI_HAL_OK;
}

/*
 * @brief  	read data via I2C
 * @param 	tx_buf	pointer to the Tx buffer
 * @param 	buffer	pointer to the Rx buffer
 * @param		tx_size	size of the Tx buffer in bytes
 * @param 	rx_size	size of the Rx buffer
 * @return	ADI_HAL_OK (= 0) if the data exchange was successful,
 * 					ADI_HAL_ERROR (= -1) - otherwise
 */
uint16_t Adpd400x_I2C_TxRx(uint8_t* tx_buf, uint8_t* rx_buf, uint16_t tx_size, uint16_t rx_size)
{
  // transmit data
  if (Adpd400x_I2C_Transmit(tx_buf, tx_size) != ADI_HAL_OK)
    return ADI_HAL_ERROR;

  HAL_StatusTypeDef ret; // return status of i2c

  // receive the two-byte (most significant byte first) value from the sensor
  ret = HAL_I2C_Master_Receive(&hi2c1, ADPD4102_I2C_ADDR, rx_buf, rx_size, HAL_MAX_DELAY);
  if (ret != HAL_OK)
  {
    char uart_buf[50];
    int uart_buf_len;
    uart_buf_len = sprintf(uart_buf, "Error Rx I2C\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, uart_buf_len, 100);
    return ADI_HAL_ERROR;
  }

  return ADI_HAL_OK;
}

void MCU_HAL_Delay(uint32_t delay)
{
  HAL_Delay(delay);
}
/* ----------------------------------------------------------------------------------------------------------------------
 */

// redirect printf to UART instance
PUTCHAR_PROTOTYPE
{
  HAL_UART_Transmit(&huart1, (uint8_t*)&ch, 1, HAL_MAX_DELAY);
  return ch;
}

// redirect scanf to UART instance
GETCHAR_PROTOTYPE
{
  uint8_t ch = 0;

  /* Clear the Overrun flag just before receiving the first character */
  __HAL_UART_CLEAR_OREFLAG(&huart1);

  /* Wait for reception of a character on the USART RX line and echo this
   * character on console */
  HAL_UART_Receive(&huart1, (uint8_t*)&ch, 1, HAL_MAX_DELAY);
  HAL_UART_Transmit(&huart1, (uint8_t*)&ch, 1, HAL_MAX_DELAY);
  return ch;
}

/**
 * @brief force a software reset of the MCU
 *
 */
void softResetMCU(void)
{
  // set the CPU1/CPU2's SYSRESETREQ bits (refer to PM0214, rev10, p.228)
  NVIC_SystemReset();
}

/**
 * @brief		Indicate the end of experiment (blink LED D11(green))
 * @param  	None
 * @retval 	None
 */
void indicateExperimentBoundary(void)
{
  for (int i = 0; i < 6; i++)
  {                                                         // blink 6 times
    HAL_GPIO_TogglePin(DISP_LED2_GPIO_Port, DISP_LED2_Pin); // on
    HAL_Delay(500);
    HAL_GPIO_TogglePin(DISP_LED2_GPIO_Port, DISP_LED2_Pin); // off
    HAL_Delay(500);
  }
}

/**
 * @brief Indicate an error in the code (flash LED D10 (red))
 *
 */
void indicateError(void)
{
  HAL_GPIO_WritePin(DISP_LED0_GPIO_Port, DISP_LED0_Pin, GPIO_PIN_SET);
  HAL_Delay(5000); // 5 sec
  HAL_GPIO_WritePin(DISP_LED0_GPIO_Port, DISP_LED0_Pin, GPIO_PIN_RESET);
}

/**
 * @brief Indicate the power on of the device (flash LED D11(green))
 *
 */
void indicateDevicePowerOn(void)
{
  HAL_GPIO_WritePin(DISP_LED2_GPIO_Port, DISP_LED2_Pin, GPIO_PIN_SET);
  HAL_Delay(5000); // 5 sec
  HAL_GPIO_WritePin(DISP_LED2_GPIO_Port, DISP_LED2_Pin, GPIO_PIN_RESET);
}

/**
 * @brief display the current state of the TOM patch
 *
 */
void displayCurrentDeviceState(void)
{
  switch (device_state)
  {
  case POWERED_ON_TOM_STATE:
    indicateDevicePowerOn();
    break;
  case MEASURING_TOM_STATE:
  case STOPPED_TOM_STATE:
    indicateExperimentBoundary();
    break;
  case ERROR_TOM_STATE:
    indicateError();
    break;
  default:
    break;
  }
  display_device_state = false;
}

/**
 * @brief		Configure stdin
 * @param  	None
 * @retval 	None
 */
void configureStdinCLI(void)
{
  // Disable internal buffering for the input stream (otherwise, the default syscalls.c
  // file automatically generated by STM32CubeIDE results in unexpected behavior)
  setvbuf(stdin, NULL, _IONBF, 0);
}

/**
 * @brief		Show the welcome message on CLI
 * @param  	None
 * @retval 	None
 */
void showWelcomeMsgCLI(void)
{
  printf("================================================================================\r\n");
  printf("%c[1m", ASCII_ESC); // VT100 escape code: turn bold mode on
  //	printf("%c[4m", ASCII_ESC);		// VT100 escape code: turn underline mode on
  printf("\tWelcome to the transcutaneous oxygen monitor's control interface\r\n");
  printf("%c[m", ASCII_ESC); // Turn off character attributes
  printf("================================================================================\r\n");
}

/**
 * @brief     Select the course of action: get previous results or start new measurements
 * @param     None
 * @retval    Status: 0 - OK, otherwise - error
 */
int selectCourseOfActionCLI(void)
{
  // Define our menu options
  menu_option options[] = {{"calibrate the sensor", 0},
                           {"get previous results", 1},
                           {"start new measurements", 2},
                           {"delete measurements on disk", 3},
                           {"reformat disk (all data will be lost)", 4},
                           {"list files & dirs", 5}};
  const int numOptions = sizeof(options) / sizeof(options[0]);

  // Use the generic menu handler with a custom title
  course_of_action = handle_interactive_menu(options, numOptions, "Select how to proceed:");

  return (course_of_action >= 0) ? ADI_HAL_OK : ADI_HAL_ERROR;
}

/**
 * @brief		Course of action: dump previously recorded data from the external Flash chip
 * @param  	None
 * @retval 	Status: 0 - OK, otherwise - error
 */
int dumpPrevResultsFromExtFlash(void)
{
  printf("\n\n\rYou selected to read out the previous results. Starting, please wait...\n\n\r");
  HAL_Delay(500);

  // power-up the memory chip
  if (W25Q32J_releasePwrDown(&hqspi) != HAL_OK)
  {
    printf("Power-up W25Q32JV: Error\r\n");
    return ADI_HAL_ERROR; // exit
  }
  else
    printf("Power-up W25Q32JV: OK\r\n");

  // read Mfr and Device ID
  read_flash_chip_jedec_id(&hqspi);
  printf("\r\n");

  // debug
  // =====================================================================================================
  //		/* erase the chip */
  //		printf("Erasing W25Q32JV: wait...\r\n");
  //		flash_eraseChip(&hqspi);
  //
  //		/* program the Flash */
  //		uint32_t flashAddress_test = 256;
  //		uint32_t flashDataLength_test = 8;
  //
  //		uint8_t* tx_buf_test = (uint8_t*)calloc(flashDataLength_test, sizeof(uint8_t));
  //		if (tx_buf_test == NULL) {printf("NULL array: %s\r\n", getVarName(tx_buf_test)); return 1;}	// exit
  //(could not allocate)
  //
  //		tx_buf_test[0] = 0x01; tx_buf_test[1] = 0x02; tx_buf_test[2] = 0x03; tx_buf_test[3] = 0x04;
  //		tx_buf_test[4] = 0x05; tx_buf_test[5] = 0x06;	tx_buf_test[6] = 0x07; tx_buf_test[7] = 0x08;
  ////		tx_buf_test[254] = 0x09; tx_buf_test[255] = 0x0A; //tx_buf_test[256] = 0x0B; tx_buf_test[257] = 0x0C;
  //
  //		printf("Programming W25Q32JV: wait...\r\n");
  //		if (W25Q32J_programPage_QSPI(&hqspi, tx_buf_test, (flashDataLength_test-1), flashAddress_test) !=
  // HAL_OK) Error_Handler();
  //
  //		/* deallocate the buffer */
  //		free(tx_buf_test);
  // =====================================================================================================

  /* read the parsing header out */
  flashAddress = 0; // reset the address (as a precaution)

  uint8_t parsing_header[AUXILARY_PARSING_BYTES_NUM];
  if (W25Q32J_readData_QSPI(&hqspi, parsing_header, AUXILARY_PARSING_BYTES_NUM, flashAddress) != HAL_OK)
    Error_Handler();

  uint32_t num_data_bytes =
      (parsing_header[3] << 24) | (parsing_header[2] << 16) | (parsing_header[1] << 8) | parsing_header[0];
  uint16_t samples_per_dec_curve = (parsing_header[5] << 8) | parsing_header[4];
  uint16_t calibration_value = (parsing_header[7] << 8) | parsing_header[6];
  uint16_t bytes_per_measurement = TMP_SAMPLE_BYTES_NUM + ACCEL_AXIS_NUM * ACCEL_PER_AXIS_BYTES_NUM +
                                   samples_per_dec_curve * AFE_IR_FLUO_SAMPLE_BYTES_NUM;
  uint32_t total_num_measurements = num_data_bytes / bytes_per_measurement;

  printf("Total number of data bytes: %lu\r\n", num_data_bytes);
  printf("Samples per decay curve: %d\r\n", samples_per_dec_curve);
  printf("Calibration value: %d\r\n", calibration_value);
  printf("Bytes per measurement: %u\r\n", bytes_per_measurement);
  printf("Total number of measurements (decay curves): %lu\r\n", total_num_measurements);

  flashAddress += AUXILARY_PARSING_BYTES_NUM;

  uint32_t measurement_ctr = 0;

  /* read data out (measurement by measurement) */
  printf("Reading out W25Q32JV: wait...\r\n");
  for (int i = 0; i < total_num_measurements; i++)
  {
    // allocate the Rx buffer
    uint8_t* rx_buf = (uint8_t*)calloc(bytes_per_measurement, sizeof(uint8_t));
    if (rx_buf == NULL)
    {
      printf("NULL array: %s\r\n", getVarName(rx_buf));
      device_state = ERROR_TOM_STATE; // update the state of the TOM patch
      displayCurrentDeviceState();
      return ADI_HAL_ERROR; // exit (could not allocate)
    }

    // read out the data bytes
    if (W25Q32J_readData_QSPI(&hqspi, rx_buf, bytes_per_measurement, flashAddress) != HAL_OK)
      Error_Handler();

    // increase the counter of measurements
    measurement_ctr++;

// parse and display the received data
#ifdef PARSE_FLASH_SHOW_DATA_DEBUG
    // print raw data (bytes)
    for (int i = 0; i < bytes_per_measurement; i++)
    {
      printf("%d", rx_buf[i]);
      if (i != (bytes_per_measurement - 1))
        printf(", ");
      if (i % 8 == 7)
        printf("\r\n"); // show in chunks of 8 bytes
    }
    printf("\r\n\n");
#endif

    printf("\nMeasurement #%lu.\r\n", measurement_ctr);

    // temperature
    uint16_t temp_twos_comp_value = (rx_buf[0] << 8) | rx_buf[1];
    printTemperature(&temp_twos_comp_value);

    // acceleration
    g_x_raw = (rx_buf[2] << 8) | rx_buf[3];
    g_y_raw = (rx_buf[4] << 8) | rx_buf[5];
    g_z_raw = (rx_buf[6] << 8) | rx_buf[7];
    accel_print_nodev(accel_range, &g_x_raw, &g_y_raw, &g_z_raw, &g_x, &g_y, &g_z);

    // decay curve
    printf("Fluorescense[ADC CODE]:\r\n");
    for (uint32_t j = 8; j <= bytes_per_measurement - 2; j += 2)
    {
      printf("%d\r\n", (rx_buf[j] << 8) | rx_buf[j + 1]);
    }

    // calculate the next data block's address
    flashAddress += bytes_per_measurement;

    // deallocate the Rx buffer
    free(rx_buf);
  }

  printf("\nThe experiment has been completed.\r\n");

  // power-down the memory chip
  if (W25Q32J_pwr_down(&hqspi) != HAL_OK)
  {
    printf("Power-down W25Q32JV: Error\r\n");
    return ADI_HAL_ERROR; // exit
  }
#ifdef VERBOSE_OUT
  else
    printf("Power-down W25Q32JV: OK\r\n");
#endif

  return ADI_HAL_OK;
}

/**
 * @brief  Course of action: start new measurements
 * @param  None
 * @retval	Status: 0 - OK, otherwise - error
 */
int startNewMeasurements(void)
{
#ifdef LAUNCH_MEASUREMENTS_MANUALLY
  printf("\n\n\rStarting measurements, please wait...\n\n\r");
  // HAL_Delay(1000);
#endif

// monitor the battery voltage (not implemented yet)
#ifdef MONITOR_VBAT
  monitorVbat();
#endif

// use SMPS (not completed yet)
#ifdef USE_SMPS
  useSMPS();
#endif

  initADXL367();

  // initialize the temperature sensor
  initTMP117();

  // initialize the analog front-end
  if (initADPD4102() == ADI_HAL_ERROR)
    return ADI_HAL_ERROR;

// ask how many measurements the user wants to take
#ifdef LAUNCH_MEASUREMENTS_MANUALLY
  if (course_of_action == CALIBRATE_SENSOR)
  {
    // initialize the external Flash module
    if (initW25Q32J() == ADI_HAL_ERROR)
      return ADI_HAL_ERROR;
    numMeasurements = NUM_MEASUREMENTS_FOR_CALIBRATION;
  }
  else if (course_of_action == LAUNCH_NEW_MEASUREMENTS)
  {
    // HACK: goto? TODO: fix this with a while or something
  get_fileslot:
    fflush(stdin);
    printf("%c[4m", ASCII_ESC); // VT100 escape code: turn underline mode on
    printf("\nEnter the fileslot that you want to save the data:\r\n");
    printf("%c[m", ASCII_ESC); // Turn off character attributes
    printf("\n>>>: ");
    fflush(stdout); // flush the output buffer and ensure that the prompt is displayed before waiting for input
    scanfReturnStatus = scanf("%lu", &fileslot);
    // 10000 is arbitrary for now, but we dont want to have too many files (a lot of table updates)
    if (scanfReturnStatus == EOF || fileslot > 10000)
    {
      printf("Enter a valid file slot.\r\n");
      goto get_fileslot;
    }

    // We will save it as a binary file for now, we can add a CSV option
    sprintf(fil_path, "0:measurements/%lu.dat", fileslot);
    FILINFO fno;
    // Check if file already exists
    FRESULT stat_res = f_stat(fil_path, &fno);
    if (stat_res == FR_OK)
    {
      printf("File already exists, delete it first.\r\n");
      goto get_fileslot;
    }
    // Same thing here (get rid of goto)
  get_decay_curves:
    printf("%c[4m", ASCII_ESC); // VT100 escape code: turn underline mode on
    printf("\nEnter the total number of measurements (decay curves):\r\n");
    printf("%c[m", ASCII_ESC); // Turn off character attributes
    printf("\n>>>: ");
    fflush(stdout); // flush the output buffer and ensure that the prompt is displayed before waiting for input

    scanfReturnStatus = scanf("%lu", &numMeasurements);
    if ((scanfReturnStatus != 1) || (numMeasurements < 1))
    {
      printf("\n\n\rError: Invalid input.\n\r");
      printf("Please enter a number between 1 and 50000.\n\r");
      goto get_decay_curves;
    }
  }
#else
  numMeasurements = NUM_MEASUREMENTS_IN_AUTO_MODE;
  FRESULT find_next = find_next_auto_measurements_data_file(fil_path);
  if (find_next != 0u)
  {
    printf("> The next data file to write for automatic measurements could not be found.\r\n");
    return ADI_HAL_ERROR;
  }
#endif

  printf("\n\rNumber of measurements to be taken: %u\n\r", numMeasurements);
  size_t data_size_in_bytes = numMeasurements * sizeof(save_datapoint_t);
  // Calculate the free space
  FATFS* fs;
  // Free cluster, free sector & total sector count
  DWORD free_clst, free_sect, tot_sect;
  FRESULT free_res = f_getfree("", &free_clst, &fs);
  if (free_res != FR_OK)
  {
    printf("> Failed to get free space on disk: %d\r\n", free_res);
    return ADI_HAL_ERROR;
  }
  tot_sect = (fs->n_fatent - 2) * fs->csize;
  free_sect = free_clst * fs->csize;
  // Sector size = 4096 bytes, 1 sector = 4 KiB
  printf("> %-20s%10lu KiB\r\n", "Total space:", tot_sect * 4);
  printf("> %-20s%10lu KiB\r\n", "Available space:", free_sect * 4);

  if (free_sect * W25Q32J_SECTOR_SIZE < data_size_in_bytes)
  {
    printf("> Not available space on disk, try deleting unused measurements.\r\n");
    return ADI_HAL_ERROR;
  }

  // Now, we can create the file
  FRESULT cr_res = f_open(&fil, fil_path, FA_CREATE_NEW | FA_WRITE | FA_READ);
  if (cr_res)
  {
    printf("> An error occured while creating measurement file: %d\r\n", cr_res);
    return ADI_HAL_ERROR;
  }
  printf("> Measurement file \"%s\" created successfully.\r\n", fil_path);
  // TODO: Decide if we want to allocate the entire file now. Sometimes this can cause a lot more space to be allocated
  // on disk since sometimes measurements are ended abruptly (via external input, reset, etc.). I think allocating as we
  // write along is the more sensible option.
  // We can pre-allocate a small amount, not the entire file
  FRESULT exp_res =
      f_expand(&fil, sizeof(save_datapoint_t) + 2 /*Allocate for 1 datapoint + 2 magic bytes*/, 1 /*Allocate now*/);
  if (exp_res)
  {
    printf("> An error occured while expanding file: %d\r\n", exp_res);
    return ADI_HAL_ERROR;
  }
  // Expand will move the file pointer, reset it
  FRESULT seek_res = f_lseek(&fil, 0);
  if (seek_res != FR_OK)
    return ADI_HAL_ERROR;
  UINT bw;
  // Write the version bytes
  uint16_t wver = 1;
  FRESULT write_res = f_write(&fil, &wver, sizeof(uint16_t), &bw);
  if (write_res != FR_OK)
  {
    return ADI_HAL_ERROR;
  }
  f_sync(&fil); // Minimize critical section
  printf("> File version written: %u\r\n", TOM_FILE_VERSION);
  f_close(&fil);

  // Reset RTC
  RTC_DateTypeDef sDate;
  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = RTC_MONTH_JANUARY;
  sDate.Date = 1;
  sDate.Year = 0;
  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }
  // Reset RTC time
  RTC_TimeTypeDef sTime;
  sTime.Hours = 0;
  sTime.Minutes = 0;
  sTime.Seconds = 0;
  sTime.SubSeconds = 0x0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }
  // We terminate the experiment if we reach this amount of seconds
  terminate_experiment_at_secs = 259200;
  printf("> Experiment will terminate if duration exceeds %u (s)\r\n", terminate_experiment_at_secs);

  // set sensor to the Sample ("Go") Mode
  uint16_t ret_code = adi_adpdssm_setOperationMode(E_ADI_ADPD_MODE_SAMPLE);
  if (ret_code != ADI_ADPD_DRV_SUCCESS)
    printf("> Error starting measurements: check your system and restart.\r\n");

  reset_stopwatch(&sw); // Timestamps
  return ADI_HAL_OK;
}

/**
 * @brief  Calibrate the sensor
 * @param  None
 * @retval None
 */
int calibrateSensor(void)
{
  // display instructions
  printf("\n\nYou selected to (re)calibrate the sensor. To do so:\r\n\n");
  printf("\t 1. Detach the film from the sensor head.\r\n");
  printf("\t 2. Place the board into a dark chamber to isolate it from the ambient light.\r\n");
  printf("\t NOTE: seal the dark chamber well to minimize the error in calibration.\r\n");

  // ask for ENTER key
  printf("Press [Enter] key to continue.");
  fflush(stdin); // option ONE to clean stdin
  getchar();     // wait for ENTER

  // start new measurements
  if (startNewMeasurements() == ADI_HAL_ERROR)
    return ADI_HAL_ERROR;
  return ADI_HAL_OK;
}

int deleteMeasurement(void)
{

  return ADI_HAL_OK;
}

int reformatDisk(void)
{
  return ADI_HAL_OK;
}

int listMeasurements(void)
{

  return ADI_HAL_OK;
}

/**
 * @brief  Function to configure and initialize PWR IP.
 * @param  None
 * @retval None
 */
void Configure_PWR(void)
{
  /* Ensure that MSI is wake-up system clock */
  LL_RCC_SetClkAfterWakeFromStop(
      LL_RCC_STOP_WAKEUPCLOCK_MSI); // Wakeup system clock: HSI16 when STOPWUCK=1 in RCC_CFGR. MSI with the frequency
                                    // before entering the Stop mode when STOPWUCK=0
                                    //  LL_RCC_SetClkAfterWakeFromStop(LL_RCC_STOP_WAKEUPCLOCK_HSI);

  /* Select the SMPS step-down converter clock */
  //  LL_RCC_SetSMPSClockSource(LL_RCC_SMPS_CLKSOURCE_HSI);
  //  LL_RCC_SetSMPSPrescaler(LL_RCC_SMPS_DIV_2);						// 16MHz HSI / 2 = 8MHz

  /* In case of debugger probe attached, work-around of issue specified in "ES0394 - STM32WB55Cx/Rx/Vx device errata":
    2.2.9 Incomplete Stop 2 mode entry after a wakeup from debug upon EXTI line 48 event
      "With the JTAG debugger enabled on GPIO pins and after a wakeup from debug triggered by an event on EXTI
      line 48 (CDBGPWRUPREQ), the device may enter in a state in which attempts to enter Stop 2 mode are not fully
      effective ..."
  */
  LL_EXTI_DisableIT_32_63(LL_EXTI_LINE_48);
  LL_C2_EXTI_DisableIT_32_63(LL_EXTI_LINE_48);
}

void enterStandbyMode(void)
{
#ifdef VERBOSE_OUT_LP_MODES
  printf("Entering Standby mode\r\n");
#endif

  if (decay_curve_num != NUM_DECAY_CURVES)
  {
    /* Enable periodic wake-up using RTC
     * WakeUp time base: 16 / LSI1 = 16 / 32kHz = 0.5 ms
     * WakeUP counter: WakeUp time / WakeUP time base
     * To set the period of the wake-up timer to 10ms, we need to set the counter to: 10ms / 0.5ms = 20 = 0x14
     *
     */
    if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 0x14, RTC_WAKEUPCLOCK_RTCCLK_DIV16) != HAL_OK)
      Error_Handler();
  }

  // clear the WU flag
  __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
  // clear the RTC WU flag
  __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc, RTC_FLAG_WUTF);

  /* prevent wakeup by Systick interrupt */
  //	HAL_SuspendTick();

  /* put CPU2 in STOP2 mode */
  LL_C2_PWR_SetPowerMode(LL_PWR_MODE_STOP2);

  HAL_PWR_EnterSTANDBYMode();

  /* resume Tick increment */
  //	HAL_ResumeTick();

  // check if the SB status flag is set
  if (__HAL_PWR_GET_FLAG(PWR_FLAG_SB) != RESET)
  {
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);
#ifdef VERBOSE_OUT_LP_MODES
    printf("Exited Standby mode\r\n");
#endif
  }

  if (decay_curve_num != NUM_DECAY_CURVES)
  {
    // disable the RTC wakeup
    HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
  }
}

void enterStop2Mode(void)
{
#ifdef VERBOSE_OUT_LP_MODES
  printf("Entering STOP2 mode\r\n");
#endif

  //	  LL_GPIO_InitTypeDef gpio_initstruct = {LL_GPIO_PIN_ALL, LL_GPIO_MODE_ANALOG,
  //	                                         LL_GPIO_SPEED_FREQ_HIGH, LL_GPIO_OUTPUT_PUSHPULL,
  //	                                         LL_GPIO_PULL_NO, LL_GPIO_AF_0};

  /* Set all GPIO in analog state to reduce power consumption,                */
  /* Note: Debug using ST-Link is not possible during the execution of this   */
  /*       example because communication between ST-link and the device       */
  /*       under test is done through UART. All GPIO pins are disabled (set   */
  /*       to analog input mode) including  UART I/O pins.                    */
  //  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA |
  //                            LL_AHB2_GRP1_PERIPH_GPIOB |
  //                            LL_AHB2_GRP1_PERIPH_GPIOC |
  //                            LL_AHB2_GRP1_PERIPH_GPIOD |
  //                            LL_AHB2_GRP1_PERIPH_GPIOE |
  //                            LL_AHB2_GRP1_PERIPH_GPIOH);
  //
  //  LL_GPIO_Init(GPIOA, &gpio_initstruct);
  //  LL_GPIO_Init(GPIOB, &gpio_initstruct);
  //  LL_GPIO_Init(GPIOC, &gpio_initstruct);
  //  LL_GPIO_Init(GPIOD, &gpio_initstruct);
  //  LL_GPIO_Init(GPIOE, &gpio_initstruct);
  //  LL_GPIO_Init(GPIOH, &gpio_initstruct);
  //
  //  LL_AHB2_GRP1_DisableClock(LL_AHB2_GRP1_PERIPH_GPIOA |
  //                            LL_AHB2_GRP1_PERIPH_GPIOB |
  //                            LL_AHB2_GRP1_PERIPH_GPIOC |
  //                            LL_AHB2_GRP1_PERIPH_GPIOD |
  //                            LL_AHB2_GRP1_PERIPH_GPIOE |
  //                            LL_AHB2_GRP1_PERIPH_GPIOH);

  if (decay_curve_num != NUM_DECAY_CURVES)
  {
    /* Enable periodic wake-up using RTC
     * WakeUp time base: 16 / LSI1 = 16 / 32kHz = 0.5 ms
     * WakeUP counter: WakeUp time / WakeUP time base
     * To set the period of the wake-up timer to 10ms, we need to set the counter to: 10ms / 0.5ms = 20 = 0x14
     *
     */
    if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 0x14, RTC_WAKEUPCLOCK_RTCCLK_DIV16) != HAL_OK)
      Error_Handler();
  }

  /* disable debug and trace in low-power modes (DBFMCU_CR register) */
  DBGMCU->CR = 0;
  /* Configure Power IP */
  Configure_PWR();

  /* prevent wakeup by Systick interrupt */
  HAL_SuspendTick();

  /* put CPU2 in STOP2 mode */
  LL_C2_PWR_SetPowerMode(LL_PWR_MODE_STOP2);
  /* enter Stop mode */
  HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);

  if (decay_curve_num != NUM_DECAY_CURVES)
  {
    // disable the RTC wakeup
    HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
  }

  /* reconfigure system clock */
  SystemClock_Config();
  //	SYSCLKConfig_STOP();

  /* resume Tick increment */
  HAL_ResumeTick();

#ifdef VERBOSE_OUT_LP_MODES
  printf("Exited STOP2 mode\r\n");
#endif
}

/**
 * @brief print data collected for one decay curve
 *
 * @retval None
 */
void printResults()
{
/* print values */
// =================================================================================================================================
#ifdef VERBOSE_OUT
  printf("- Time [us]:\r\n");
  for (uint8_t i = 0; i < NUM_INT; i++)
  {
    printf("%d\r\n", 2 * i);
  }
#endif

  // temperature
  printTemperature(&temp_twos_comp_value);
  // acceleration
  // accel_print(accel_dev_desc, &g_x_raw, &g_y_raw, &g_z_raw, &g_x, &g_y, &g_z);
  // fluorescence (decay curve)
  printf("Fluorescense[ADC CODE]:\r\n");
  for (uint8_t i = 0; i < NUM_INT; i++)
  {
    printf("* %d\r\n", decay_curve_new[i]);
    printf("+ %d\r\n", decay_curve_new[i] - dark_data_tom003f_12[i]);
  }
}

/**
 * @brief initialize the accelerometer (ADXL367)
 *
 */
void initADXL367()
{
  /* ADXL367 device description */
  accel_dev_desc = (struct adxl367_dev_stm32*)no_os_calloc(1, sizeof(*accel_dev_desc));
  accel_dev_desc->i2c_slave_address = ADXL367_I2C_SLAVE_ADDR_ASEL_H;
  accel_dev_desc->i2c_desc = &hi2c1;

  /* convert raw acceleration offsets for XYZ axes in the two's complement format */
  uint16_t x_offset_twos_comp = calcTwosCompOffset_stm32(accel_dev_desc, 37);   // 37
  uint16_t y_offset_twos_comp = calcTwosCompOffset_stm32(accel_dev_desc, -29);  // -29
  uint16_t z_offset_twos_comp = calcTwosCompOffset_stm32(accel_dev_desc, -127); // (-123 - 131)/2 = -127

  /* configure ADXL367 */
  bool accel_init_status = adxl367_init_stm32(accel_dev_desc);
  bool accel_self_test_status = adxl367_self_test_stm32(accel_dev_desc);
  bool accel_set_range_status =
      adxl367_set_range_stm32(accel_dev_desc, accel_range); // default configuration (+/- 2g range)
  bool accel_set_offset_status =
      adxl367_set_offset_stm32(accel_dev_desc, x_offset_twos_comp, y_offset_twos_comp, z_offset_twos_comp);
#ifdef LAUNCH_MEASUREMENTS_MANUALLY
  printf("accel_init_status: %s\r\n", accel_init_status ? "true" : "false");
  printf("accel_self_test_status: %s\r\n", accel_self_test_status ? "true" : "false");
  printf("accel_set_range_status: %s\r\n", accel_set_range_status ? "true" : "false");
  if (accel_init_status && accel_self_test_status && accel_set_range_status && accel_set_offset_status)
  {
    printf("Initializing accelerometer (ADXL367): OK\r\n");
  }
  else
    printf("Initializing accelerometer (ADXL367): Error\r\n");
#endif
}

/**
 * @brief Initialize the temperature sensor (TMP117)
 *
 */
void initTMP117()
{
  /*-----------  Temperature sensor -----------*/
  bool temp_init_status = false;
  /* set averaging */
  temp_init_status = setAveraging(&hi2c1, buffer_i2c_temp, TMP117_AVG_8);
  /* set alert mode */
  temp_init_status = setAlertMode(&hi2c1, buffer_i2c_temp, TMP117_ALERT_MODE);
  /* set conversion mode */
  temp_init_status = setConversionMode(&hi2c1, buffer_i2c_temp, TMP117_CC_MODE);

#ifdef LAUNCH_MEASUREMENTS_MANUALLY
  if (temp_init_status)
    printf("Initializing temp.sensor (TMP117): OK\r\n");
  else
    printf("Initializing temp.sensor (TMP117): Error\r\n");
#endif

  /* get sensor ID */
  // if (getDeviceID(&hi2c1, buffer_i2c_temp, &temp_reg_value)) printf("\nTemp.sensor ID: 0x%04x\r\n", temp_reg_value);
  /* get sensor config. register */
  //	if (getConfig(&hi2c1, buffer_i2c_temp, &temp_reg_value)) printf("Temp.sensor config.: 0x%04x\r\n",
  // temp_reg_value);
}

/**
 * @brief monitor the state of the VBAT battery voltage
 *
 */
void monitorVbat()
{
  printf("monitorVbat() function is not implemented.\r\n");
}

/**
 * @brief use SMPS (switched-mode power supply)
 *
 */
void useSMPS()
{
  // read out SMPS programming (read/write) registers
  printf("SMPS programming registers:\r\n\n");

  uint8_t smpsen = READ_BIT(PWR->CR5, PWR_CR5_SMPSEN);
  printf("PWR_CR5.SMPSEN bit value: ");
  printf("0x%01x\r\n", smpsen);

  uint8_t smpsvos_0 = READ_BIT(PWR->CR5, PWR_CR5_SMPSVOS_0);
  uint8_t smpsvos_1 = READ_BIT(PWR->CR5, PWR_CR5_SMPSVOS_1);
  uint8_t smpsvos_2 = READ_BIT(PWR->CR5, PWR_CR5_SMPSVOS_2);
  uint8_t smpsvos_3 = READ_BIT(PWR->CR5, PWR_CR5_SMPSVOS_3);
  uint8_t smpsvos = smpsvos_0 | smpsvos_1 | smpsvos_2 | smpsvos_3;
  printf("PWR_CR5.SMPSVOS[3..0] value: ");
  printf("0x%02x\r\n", smpsvos);

  uint8_t smpssc_0 = READ_BIT(PWR->CR5, PWR_CR5_SMPSSC_0);
  uint8_t smpssc_1 = READ_BIT(PWR->CR5, PWR_CR5_SMPSSC_1);
  uint8_t smpssc_2 = READ_BIT(PWR->CR5, PWR_CR5_SMPSSC_2);
  uint8_t smpssc = (smpssc_0 >> 4) | (smpssc_1 >> 4) | (smpssc_2 >> 4);
  printf("PWR_CR5.SMPSSC[2..0] value: ");
  printf("0x%02x\r\n", smpssc);

  uint8_t eborhsmpsfb = READ_BIT(PWR->CR3, PWR_CR3_EBORHSMPSFB);
  printf("PWR_CR3.EBORHSMPSFB bit value: ");
  printf("0x%01x\r\n", (eborhsmpsfb >> 8));

  uint8_t borhc_bit = READ_BIT(PWR->CR5, PWR_CR5_BORHC);
  printf("PWR_CR5.BORHC bit value: ");
  printf("0x%01x\r\n", (borhc_bit >> 8));

  uint8_t smpssel_0 = READ_BIT(RCC->SMPSCR, RCC_SMPSCR_SMPSSEL_0);
  uint8_t smpssel_1 = READ_BIT(RCC->SMPSCR, RCC_SMPSCR_SMPSSEL_1);
  uint8_t smpssel = smpssel_0 | smpssel_1;
  printf("RCC_SMPSCR.SMPSSEL[1..0] value: ");
  printf("0x%01x\r\n", smpssel);

  uint8_t smpsdiv_0 = READ_BIT(RCC->SMPSCR, RCC_SMPSCR_SMPSDIV_0);
  uint8_t smpsdiv_1 = READ_BIT(RCC->SMPSCR, RCC_SMPSCR_SMPSDIV_1);
  uint8_t smpsdiv = (smpsdiv_0 >> 4) | (smpsdiv_1 >> 4);
  printf("RCC_SMPSCR.SMPSDIV[1..0] value: ");
  printf("0x%01x\r\n", smpsdiv);

  // INFO PWR_SCR.CSMPSFBF is write-only

  // read out SMPS status (read/write) registers

  printf("SMPS programming registers:\r\n\n");

  uint8_t smpsf = READ_BIT(PWR->SR2, PWR_SR2_SMPSF);
  printf("PWR_SR2.SMPSF bit value: ");
  printf("0x%01x\r\n", smpsf);

  uint8_t smpsfbf = READ_BIT(PWR->SR1, PWR_SR1_SMPSFBF);
  printf("PWR_SR1.SMPSFBF bit value: ");
  printf("0x%01x\r\n", smpsfbf);

  uint8_t smpsbf = READ_BIT(PWR->SR2, PWR_SR2_SMPSBF);
  printf("PWR_SR2.SMPSBF bit value: ");
  printf("0x%01x\r\n", smpsbf);

  uint8_t smpssws_0 = READ_BIT(RCC->SMPSCR, RCC_SMPSCR_SMPSSWS_0);
  uint8_t smpssws_1 = READ_BIT(RCC->SMPSCR, RCC_SMPSCR_SMPSSWS_1);
  uint8_t smpssws = (smpssws_0 >> 8) | (smpssws_1 >> 8);
  printf("RCC_SMPSCR.SMPSSWS[1..0] value: ");
  printf("0x%01x\r\n", smpssws);

  //	SET_BIT(REG, BIT)				// that is how you set/clear a bit in a register
  //	CLEAR_BIT(REG, BIT)

  // enable SMPS
  //	LL_PWR_SMPS_Enable();									// there are some
  // wrappers for SET_BIT in the LL API 	SET_BIT(PWR->CR5, PWR_CR5_SMPSEN);

  // see this header as some masks (like SMPS_VOLTAGE_BASE_MV) are defined there: stm32wbxx_ll_pwr.h
}

/**
 * @brief initialize the QSPI Flash (W25Q32J)
 *
 * @return 0 - OK, otherwise - error
 */
int initW25Q32J()
{
  // power-up the memory chip
  if (W25Q32J_releasePwrDown(&hqspi) != HAL_OK)
  {
    printf("Power-up W25Q32JV: Error\r\n");
    return ADI_HAL_ERROR; // exit
  }

  // erase a memory block
  printf("W25Q32JV External Flash erasing: wait...\r\n");
  //	flash_eraseBlock64KB(&hqspi, flashAddress);
  //	flash_eraseHalfBlock32KB(&hqspi, flashAddress);s
  //	flash_eraseSector32KB(&hqspi, flashAddress);
  flash_eraseChip(&hqspi);

  //	printf("%c[2K", ASCII_ESC);		// VT100 escape code: clear entire line
  printf("W25Q32JV External Flash erasing: OK\r\n");

  // power-down the memory chip
  if (W25Q32J_pwr_down(&hqspi) != HAL_OK)
  {
    printf("Power-down W25Q32JV: Error\r\n");
    return ADI_HAL_ERROR; // exit
  }
  else
    printf("Power-down W25Q32JV: OK\r\n");

  return ADI_HAL_OK;
}

/**
 * @brief initialize the AFE (ADPD4102)
 *
 * @return Status: 0 - OK, otherwise - error
 */
int initADPD4102()
{
  /*-----------  ADPD -----------*/
  // open ADPD driver
  printf("Initialzing AFE's driver: ");
  if (adi_adpddrv_OpenDriver() != ADI_ADPD_DRV_SUCCESS)
  {
    printf("Error: Cannot open driver.\n\n\r");
  }
  else
    printf("OK\n\n\r");

// check the communication interface
#ifdef VERBOSE_OUT
  printf("Communication interface: ");
  bus_mode = adi_adpddrv_GetComMode();
  switch (bus_mode)
  {
  case E_ADI_ADPD_I2C_BUS:
    printf("I2C.\r\n");
    break;
  case E_ADI_ADPD_SPI_BUS:
    printf("SPI.\r\n");
    break;
  default:
    printf("Unknow bus.\r\n");
    break;
  }
#endif

#ifdef READ_DEFAULT_CFG
  // read default configuration
  printf("\nReading the current AFE's configuration:\r\n");
  test_adpd4102_get_config(&hi2c1, &huart1, i2cDataExchBuffer, dcfg_ADPD4102_icas_ptco2, CONFIG_LENGTH);
#endif

  printf("AFE's configuration:\n\r"
         "- Slot: A\n\r"
         "- Channel: 1\n\r"
         "- Mode: impulse response (IR)\n\r"
         "- Sampling period [us]: 2\n\r");

  // display the measurement parameters
  printf("\nDefault AFE's settings:\n\r"
         "- Decay curves to get averaged per one measurement: %d \n\r"
         "- Samples per one decay curve: %d\r\n"
         "- LED pulse offset [us]: %d\r\n"
         "- LED pulse width [us]: %d\r\n"
         "- LED current [mA]: %d\r\n"
         "- Sampling start adjustment [us]: %d\r\n"
         "- Acquisition window lit offset [us]: %d\r\n"
         "- TIA gain [KOhm]: %d\r\n", //%0.1f\r\n
         NUM_DECAY_CURVES, NUM_INT, LED_OFFSET_uSEC, LED_WIDTH_uSEC, LED_CURRENT_mA, SAMPLING_START_OFFSET_uSEC,
         LIT_OFFSET_uS, (uint8_t)TIA_GAIN_CH1_A_kOHM);

#ifdef LAUNCH_MEASUREMENTS_MANUALLY
  if (course_of_action == CALIBRATE_SENSOR)
  {
    measurement_settings = USE_DEFAULT_MEAS_SETTINGS;
  }
  else if (course_of_action == LAUNCH_NEW_MEASUREMENTS)
  {
    printf("%c[4m", ASCII_ESC); // VT100 escape code: turn underline mode on
    printf("\nSelect how to proceed:\r\n\n");
    printf("%c[m", ASCII_ESC); // Turn off character attributes
    printf("\t 1 - use default settings\r\n");
    printf("\t 2 - customize settings\r\n");
    printf("\n>>>: ");
    fflush(stdout); // flush the output buffer and ensure that the prompt is displayed before waiting for input
    scanfReturnStatus = scanf("%d", &measurement_settings);
    if ((scanfReturnStatus != 1) ||
        ((measurement_settings != USE_DEFAULT_MEAS_SETTINGS) && (measurement_settings != USE_CUSTOM_MEAS_SETTINGS)))
    {
      printf("\n\n\rError: Invalid input.\n\r");
      printf("Please restart the MCU and enter a correct number.\n\r");
      return ADI_HAL_ERROR;
    }
    scanfReturnStatus = -1;
  }
#else
  measurement_settings = USE_DEFAULT_MEAS_SETTINGS;
#endif

  if (measurement_settings == USE_DEFAULT_MEAS_SETTINGS)
  {
    printf("\n\n\rYou selected to use default measurement settings. Preparing, please wait...\n\n\r");
    HAL_Delay(1000);
  }
  else
  {
    printf("\n\n\rYou selected to customize measurement settings. Preparing, please wait...\n\n\r");
    HAL_Delay(1000);

    cli cli_handler;
    int32_t ret_val = cli_init(&cli_handler);
    if (ret_val == FAILURE)
    {
      printf("Could not initialize CLI parser. Please restart the MCU.\r\n");
      return ADI_HAL_ERROR;
    }
    else
    {
      char cmd[20];
      int8_t status = SUCCESS;
      // loop until the user explicitly wants to exit the interactive configuration session
      do
      {
        printf("\nType a command and press 'Enter' (the commands are not case sensitive): ");
        scanf("%19s", cmd);
        printf("\n\rEntered command: %s\r\n", cmd);
        cli_parse(&cli_handler, cmd);
        // status = cli_parse(&cli_handler, cmd);
        if (strcmp(cmd, "exit") != 0)
          status = FAILURE;
        else
          status = SUCCESS;
      } while (status != SUCCESS);
    }

    /**
     * @brief Help command helper function. Display help function prompt.
     * @param [in] dev - The device structure.
     * @return SUCCESS in case of success, FAILURE otherwise.
     */
    //	static int32_t cn0503_help_prompt(struct cn0503_dev *dev)
    //	{
    //		int32_t ret;
    //
    //		ret = cli_write_string(dev->cli_handler, (uint8_t*)"\tCN0503 application.\n");
    //
    //		if(ret != SUCCESS) return ret;
    //
    //		ret = cli_write_string(dev->cli_handler,
    //				       (uint8_t*)
    //				       "Type a command and press 'Enter'. The commands are not case sensitive.\n");
    //
    //		if(ret != SUCCESS) return ret;
    //		return cli_write_string(dev->cli_handler, (uint8_t*)"Available commands.\n\n");
    //	}
  }

  // load configuration using ADI's driver (0xFFU - for generic config)
  printf("Loading AFE's configuration: ");
  if (adi_adpdssm_loadDcfg(dcfg_ADPD4102_icas_ptco2, 0xFFU) != ADI_ADPD_SSM_SUCCESS)
  {
    printf("Error.\r\n");
  }
  else
  {
    printf("OK.\r\n");
#ifdef VERIFY_CFG_LOAD
    // verify that configuration has been written successfully
    printf("Current configuration:\r\n");
    test_adpd4102_get_config(&hi2c1, &huart1, i2cDataExchBuffer, dcfg_ADPD4102_icas_ptco2, CONFIG_LENGTH);
#endif
  }

  // initialize the helper function module with driver instance
  adi_adpdssm_slotinit(&oAdiAppInst);

  return ADI_HAL_OK;
}

/**
 * @brief Enter a preferred LP mode
 * @param LP_Mode lp_mode - the selected LP mode
 */
void enterLPMode(LP_Mode lp_mode)
{
  switch (lp_mode)
  {
  case USE_SLEEP_MODE:
    enterSleepMode();
    break;
  case USE_LP_SLEEP_MODE:
    enterLPSleepMode();
    break;
  case USE_STOP2_MODE:
    enterStop2Mode();
    /* In order to save more energy when the application is in low-power mode, it is recommended
    to put the Quad-SPI memory in low-power mode before entering the STM32 in low-power mode.*/
    break;
  case USE_STANDBY_MODE:
    enterStandbyMode();
    break;
  default:
    __NOP(); // do nothing (stay in the normal mode)
    break;
  }
}

void enterSleepMode(void)
{
#ifdef VERBOSE_OUT_LP_MODES
  printf("Entering SLEEP mode\r\n");
#endif

  if (decay_curve_num != NUM_DECAY_CURVES)
  {
    /* Enable periodic wake-up using RTC
     * WakeUp time base: 16 / LSI1 = 16 / 32kHz = 0.5 ms
     * WakeUP counter: WakeUp time / WakeUP time base
     * To set the period of the wake-up timer to 10ms, we need to set the counter to: 10ms / 0.5ms = 20 = 0x14
     *
     */
    if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 0x14, RTC_WAKEUPCLOCK_RTCCLK_DIV16) != HAL_OK)
      Error_Handler();
  }

  /* Suspend Tick increment to prevent wakeup by Systick interrupt.         */
  /* Otherwise the Systick interrupt will wake up the device within 1ms     */
  /* (HAL time base).
   */
  HAL_SuspendTick();

  HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);

  if (decay_curve_num != NUM_DECAY_CURVES)
  {
    // disable the RTC wakeup
    HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
  }

  HAL_ResumeTick();

#ifdef VERBOSE_OUT_LP_MODES
  printf("Exited SLEEP mode\r\n");
#endif
}

void enterLPSleepMode(void)
{
#ifdef VERBOSE_OUT_LP_MODES
  printf("Entering LPSLEEP mode\r\n");
#endif

  if (decay_curve_num != NUM_DECAY_CURVES)
  {
    /* Enable periodic wake-up using RTC
     * WakeUp time base: 16 / LSI1 = 16 / 32kHz = 0.5 ms
     * WakeUP counter: WakeUp time / WakeUP time base
     * To set the period of the wake-up timer to 10ms, we need to set the counter to: 10ms / 0.5ms = 20 = 0x14
     *
     */
    if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 0x14, RTC_WAKEUPCLOCK_RTCCLK_DIV16) != HAL_OK)
      Error_Handler();
  }

  /* Suspend Tick increment to prevent wakeup by Systick interrupt.         */
  /* Otherwise the Systick interrupt will wake up the device within 1ms     */
  /* (HAL time base).
   */
  HAL_SuspendTick();

  HAL_PWR_EnterSLEEPMode(PWR_LOWPOWERREGULATOR_ON, PWR_SLEEPENTRY_WFI);

  if (decay_curve_num != NUM_DECAY_CURVES)
  {
    // disable the RTC wakeup
    HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
  }

  HAL_ResumeTick();

  //	/* System is Low Power Run mode when exiting Low Power Sleep mode,
  //	 disable low power run mode and reset the clock to initialization configuration */
  //	HAL_PWREx_DisableLowPowerRunMode();

#ifdef VERBOSE_OUT_LP_MODES
  printf("Exited LP SLEEP mode\r\n");
#endif
}

/**
 * @brief  Configures system clock after wake-up from STOP: enable MSI, PLL
 *         and select MSI as system clock source.
 * @param  None
 * @retval None
 */
void SYSCLKConfig_STOP(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  uint32_t pFLatency = 0;

  /* Get the Oscillators configuration according to the internal RCC registers */
  HAL_RCC_GetOscConfig(&RCC_OscInitStruct);

  /* After wake-up from STOP reconfigure the system clock: Enable MSI and PLL */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /* Get the Clocks configuration according to the internal RCC registers */
  HAL_RCC_GetClockConfig(&RCC_ClkInitStruct, &pFLatency);

  /* Select MSI as system clock source */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, pFLatency) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
 * @brief		Indicate the AFE's periodic data issue (blink LED D12(yellow) 3 times)
 * @param  	None
 * @retval 	None
 */
void indicatePeriodicityInLuminescenceData(void)
{
  HAL_GPIO_TogglePin(DISP_LED2_GPIO_Port, DISP_LED1_Pin); // on
  HAL_Delay(1500);                                        // 1.5s
  HAL_GPIO_TogglePin(DISP_LED2_GPIO_Port, DISP_LED1_Pin); // off
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

#ifdef USE_FULL_ASSERT /** @brief  Reports the name of the source file and the source line number where the            \
                          assert_param error has occurred. @param  file: pointer to the source file name @param  line: \
                          assert_param error line source number @retval None */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
