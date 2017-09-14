
/*	Example tickless code for Microchip Atmel SamG55 ARM Cortex-M4F
	#define configCPU_CLOCK_HZ						( SystemCoreClock )
	#define configTICK_RATE_HZ						( 1000 )
	#define configUSE_TICKLESS_IDLE					1
*/

#define MAX_DEEP_SLEEP_TIME_MS 5000

static bool supress_tick_flag = false;

void vPortSetupTimerInterrupt(void)
{
	rtt_init(RTT, ((32768 + 500) / configTICK_RATE_HZ));
	NVIC_DisableIRQ(RTT_IRQn);
	NVIC_ClearPendingIRQ(RTT_IRQn);
	NVIC_SetPriority(RTT_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
	NVIC_EnableIRQ(RTT_IRQn);
	rtt_enable_interrupt(RTT, RTT_MR_ALMIEN);
	rtt_write_alarm_time(RTT, rtt_read_timer_value(RTT) + 1);
}


RAMFUNC void RTT_Handler(void)
{
	uint32_t rtt_status = rtt_get_status(RTT);

	if ((rtt_status & RTT_SR_ALMS) == RTT_SR_ALMS) {
		if (supress_tick_flag == false) {
			portSET_INTERRUPT_MASK_FROM_ISR();
			if( xTaskIncrementTick() != pdFALSE ) {
				portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
			}
			portCLEAR_INTERRUPT_MASK_FROM_ISR( 0 );
		}
		rtt_write_alarm_time(RTT, rtt_read_timer_value(RTT) + 1);
	}

	NVIC_ClearPendingIRQ(RTT_IRQn);
}

void vPortSuppressTicksAndSleep( TickType_t xExpectedIdleTime )
{

	supress_tick_flag = true;

	uint32_t ulReloadValue, ulCompleteTickPeriods, ulCompletedSysTickDecrements, ulSysTickCTRL;
	TickType_t xModifiableIdleTime;

	if( xExpectedIdleTime > MAX_DEEP_SLEEP_TIME_MS ) {
		xExpectedIdleTime = MAX_DEEP_SLEEP_TIME_MS;
	}

	uint32_t time_0 = rtt_read_timer_value(RTT);
	ulReloadValue = time_0 + ( xExpectedIdleTime - 2 );

	__disable_irq();

	if(eTaskConfirmSleepModeStatus() != eNoTasksWaitingTimeout) { 
		supress_tick_flag = false;

		__enable_irq();
	} else {
		rtt_write_alarm_time(RTT, ulReloadValue); //setting alarm 1 tick before wakeup

		pmc_switch_mck_to_sclk(PMC_MCKR_PRES_CLK_1);
		pmc_switch_mainck_to_fastrc(CKGR_MOR_MOSCRCF_24_MHz);
		pmc_switch_mck_to_mainck(PMC_PCK_PRES_CLK_1);
		pmc_disable_pllack();
		pmc_set_flash_in_wait_mode(PMC_FSMR_FLPM_FLASH_STANDBY | PMC_FSMR_RTTAL);
		pmc_enable_waitmode();
		sysclk_init();

		__enable_irq();

		uint32_t time_1 = rtt_read_timer_value(RTT);
		uint32_t actual_sleep_time = time_1 - time_0;

		if (!(actual_sleep_time < xExpectedIdleTime)) {
			actual_sleep_time = xExpectedIdleTime - 1;
		}

		portENTER_CRITICAL();
		{
			vTaskStepTick( actual_sleep_time );
			supress_tick_flag = false;
		}
		portEXIT_CRITICAL();
	}
}
