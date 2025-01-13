### I2S Connections 

On the L4 DUT board (NUCLEO L4R5ZI)

* `SAI1_FS_A` = PE4  = CN9.16
* `SAI1_SCK_A` = PE5 = CN9.18
* `SAI1_SD_A` = PE6  = CN9.20

On the runner board (STM32H573I-DK)

* `SAI1_FS_B` on PF9 (CN3.2, CN6.2). 
* `SAI1_SCK_B` on PF8 (CN3.3, CN6.3)
* `SAI1_SD_B` on PF6 (CN3.1, CN6.1)
* `SAI1_MCLK_B` on PF7 (CN3.4, CN6.4)

DUT board pinout taken from STM document "um2179-stm32-nucleo144-boards-mb1312-stmicroelectronics.pdf".
<img src="figures/ref_board_l4_pinout.png" width="600">

* Integrating CMSIS-DSP, see info [here](https://community.st.com/t5/stm32-mcus/configuring-dsp-libraries-on-stm32cubeide/ta-p/49637).
    * Currently running V1.6.0 of CMSIS-DSP
    * Include paths, etc are stored in the file `<project_name>/.cproject.`

    
### Running the model

* To include X-Cube-AI in the project, I followed the instructions [here](https://community.st.com/t5/stm32-mcus/how-to-install-x-cube-ai-through-stm32cubemx/ta-p/585776) and [here](https://www.st.com/resource/en/user_manual/dm00570145-getting-started-with-x-cube-ai-expansion-package-for-artificial-intelligence-ai-stmicroelectronics.pdf).
    * The X-Cube-AI getting started guide states a run-time efficiency of: STM32 Arm® Cortex®-M4/M33: ~9 cycles/MACC, STM32 Arm® Cortex®-M7:- ~6 cycles/MACC.

* Running the analyzer on the tflite model with no compression and balanced optimization gives the following information: 652,551 MACCs, 55.95 KiB flash, 10.41 KiB RAM.


### Notes from the Driver Source

This information is in the header of `stm32l4xx_hal_sai.c`.

```
    The SAI HAL driver can be used as follows:

    (#) Declare a SAI_HandleTypeDef handle structure (eg. SAI_HandleTypeDef hsai). <<== hsai_BlockA1
    (#) Initialize the SAI low level resources by implementing the HAL_SAI_MspInit() Api:
        (##) Enable the SAI interface clock.
        (##) SAI pins configuration:
            (+++) Enable the clock for the SAI GPIOs.
            (+++) Configure these SAI pins as alternate function pull-up.
        (##) NVIC configuration if you need to use interrupt process (HAL_SAI_Transmit_IT()
             and HAL_SAI_Receive_IT() APIs):
            (+++) Configure the SAI interrupt priority.
            (+++) Enable the NVIC SAI IRQ handle.

        (##) DMA Configuration if you need to use DMA process (HAL_SAI_Transmit_DMA()
             and HAL_SAI_Receive_DMA() APIs):
            (+++) Declare a DMA handle structure for the Tx/Rx stream.
            (+++) Enable the DMAx interface clock.
            (+++) Configure the declared DMA handle structure with the required Tx/Rx parameters.
            (+++) Configure the DMA Tx/Rx Stream.
            (+++) Associate the initialized DMA handle to the SAI DMA Tx/Rx handle.
            (+++) Configure the priority and enable the NVIC for the transfer complete interrupt on the
                DMA Tx/Rx Stream.

    (#) The initialization can be done by two ways
        (##) Expert mode : Initialize the structures Init, FrameInit and SlotInit and call HAL_SAI_Init().
        (##) Simplified mode : Initialize the high part of Init Structure and call HAL_SAI_InitProtocol().

  [..]
    (@) The specific SAI interrupts (FIFO request and Overrun underrun interrupt)
        will be managed using the macros __HAL_SAI_ENABLE_IT() and __HAL_SAI_DISABLE_IT()
        inside the transmit and receive process.
  [..]
    (@) Make sure that either:
        (+@) PLLSAI1CLK output is configured or
        (+@) PLLSAI2CLK output is configured or
        (+@) PLLSAI3CLK output is configured or
        (+@) External clock source is configured after setting correctly
             the define constant EXTERNAL_SAI1_CLOCK_VALUE or EXTERNAL_SAI2_CLOCK_VALUE in the stm32l4xx_hal_conf.h file.

  [..]
    (@) In master Tx mode: enabling the audio block immediately generates the bit clock
        for the external slaves even if there is no data in the FIFO, However FS signal
        generation is conditioned by the presence of data in the FIFO.

  [..]
    (@) In master Rx mode: enabling the audio block immediately generates the bit clock
        and FS signal for the external slaves.

  [..]
    (@) It is mandatory to respect the following conditions in order to avoid bad SAI behavior:
        (+@) First bit Offset <= (SLOT size - Data size)
        (+@) Data size <= SLOT size
        (+@) Number of SLOT x SLOT size = Frame length
        (+@) The number of slots should be even when SAI_FS_CHANNEL_IDENTIFICATION is selected.

  [..]
    (@) For STM32L4Rx/STM32L4Sx devices, PDM interface can be activated through HAL_SAI_Init function.
        Please note that PDM interface is only available for SAI1 sub-block A.
        PDM microphone delays can be tuned with HAL_SAIEx_ConfigPdmMicDelay function.

  [..]
    Three operation modes are available within this driver :

    *** Polling mode IO operation ***
    =================================
    [..]
      (+) Send an amount of data in blocking mode using HAL_SAI_Transmit()
      (+) Receive an amount of data in blocking mode using HAL_SAI_Receive()

    *** Interrupt mode IO operation ***
    ===================================
    [..]
      (+) Send an amount of data in non-blocking mode using HAL_SAI_Transmit_IT()
      (+) At transmission end of transfer HAL_SAI_TxCpltCallback() is executed and user can
          add his own code by customization of function pointer HAL_SAI_TxCpltCallback()
      (+) Receive an amount of data in non-blocking mode using HAL_SAI_Receive_IT()
      (+) At reception end of transfer HAL_SAI_RxCpltCallback() is executed and user can
          add his own code by customization of function pointer HAL_SAI_RxCpltCallback()
      (+) In case of flag error, HAL_SAI_ErrorCallback() function is executed and user can
          add his own code by customization of function pointer HAL_SAI_ErrorCallback()

    *** DMA mode IO operation ***
    =============================
    [..]
      (+) Send an amount of data in non-blocking mode (DMA) using HAL_SAI_Transmit_DMA()
      (+) At transmission end of transfer HAL_SAI_TxCpltCallback() is executed and user can
          add his own code by customization of function pointer HAL_SAI_TxCpltCallback()
      (+) Receive an amount of data in non-blocking mode (DMA) using HAL_SAI_Receive_DMA()
      (+) At reception end of transfer HAL_SAI_RxCpltCallback() is executed and user can
          add his own code by customization of function pointer HAL_SAI_RxCpltCallback()
      (+) In case of flag error, HAL_SAI_ErrorCallback() function is executed and user can
          add his own code by customization of function pointer HAL_SAI_ErrorCallback()
      (+) Pause the DMA Transfer using HAL_SAI_DMAPause()
      (+) Resume the DMA Transfer using HAL_SAI_DMAResume()
      (+) Stop the DMA Transfer using HAL_SAI_DMAStop()

    *** SAI HAL driver additional function list ***
    ===============================================
    [..]
      Below the list the others API available SAI HAL driver :

      (+) HAL_SAI_EnableTxMuteMode(): Enable the mute in tx mode
      (+) HAL_SAI_DisableTxMuteMode(): Disable the mute in tx mode
      (+) HAL_SAI_EnableRxMuteMode(): Enable the mute in Rx mode
      (+) HAL_SAI_DisableRxMuteMode(): Disable the mute in Rx mode
      (+) HAL_SAI_FlushRxFifo(): Flush the rx fifo.
      (+) HAL_SAI_Abort(): Abort the current transfer

    *** SAI HAL driver macros list ***
    ==================================
    [..]
      Below the list of most used macros in SAI HAL driver :

      (+) __HAL_SAI_ENABLE(): Enable the SAI peripheral
      (+) __HAL_SAI_DISABLE(): Disable the SAI peripheral
      (+) __HAL_SAI_ENABLE_IT(): Enable the specified SAI interrupts
      (+) __HAL_SAI_DISABLE_IT(): Disable the specified SAI interrupts
      (+) __HAL_SAI_GET_IT_SOURCE(): Check if the specified SAI interrupt source is
          enabled or disabled
      (+) __HAL_SAI_GET_FLAG(): Check whether the specified SAI flag is set or not

    *** Callback registration ***
    =============================
    [..]
    The compilation define USE_HAL_SAI_REGISTER_CALLBACKS when set to 1
    allows the user to configure dynamically the driver callbacks.
    Use functions HAL_SAI_RegisterCallback() to register a user callback.

    [..]
    Function HAL_SAI_RegisterCallback() allows to register following callbacks:
      (+) RxCpltCallback     : SAI receive complete.
      (+) RxHalfCpltCallback : SAI receive half complete.
      (+) TxCpltCallback     : SAI transmit complete.
      (+) TxHalfCpltCallback : SAI transmit half complete.
      (+) ErrorCallback      : SAI error.
      (+) MspInitCallback    : SAI MspInit.
      (+) MspDeInitCallback  : SAI MspDeInit.
    [..]
    This function takes as parameters the HAL peripheral handle, the callback ID
    and a pointer to the user callback function.

    [..]
    Use function HAL_SAI_UnRegisterCallback() to reset a callback to the default
    weak function.
    HAL_SAI_UnRegisterCallback() takes as parameters the HAL peripheral handle,
    and the callback ID.
    [..]
    This function allows to reset following callbacks:
      (+) RxCpltCallback     : SAI receive complete.
      (+) RxHalfCpltCallback : SAI receive half complete.
      (+) TxCpltCallback     : SAI transmit complete.
      (+) TxHalfCpltCallback : SAI transmit half complete.
      (+) ErrorCallback      : SAI error.
      (+) MspInitCallback    : SAI MspInit.
      (+) MspDeInitCallback  : SAI MspDeInit.

    [..]
    By default, after the HAL_SAI_Init and if the state is HAL_SAI_STATE_RESET
    all callbacks are reset to the corresponding legacy weak functions:
    examples HAL_SAI_RxCpltCallback(), HAL_SAI_ErrorCallback().
    Exception done for MspInit and MspDeInit callbacks that are respectively
    reset to the legacy weak functions in the HAL_SAI_Init
    and HAL_SAI_DeInit only when these callbacks are null (not registered beforehand).
    If not, MspInit or MspDeInit are not null, the HAL_SAI_Init and HAL_SAI_DeInit
    keep and use the user MspInit/MspDeInit callbacks (registered beforehand).

    [..]
    Callbacks can be registered/unregistered in READY state only.
    Exception done for MspInit/MspDeInit callbacks that can be registered/unregistered
    in READY or RESET state, thus registered (user) MspInit/DeInit callbacks can be used
    during the Init/DeInit.
    In that case first register the MspInit/MspDeInit user callbacks
    using HAL_SAI_RegisterCallback before calling HAL_SAI_DeInit
    or HAL_SAI_Init function.

    [..]
    When the compilation define USE_HAL_SAI_REGISTER_CALLBACKS is set to 0 or
    not defined, the callback registering feature is not available
    and weak callbacks are used.
```
