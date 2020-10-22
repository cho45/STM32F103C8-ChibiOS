#include <stdio.h>
#include <string.h>

#include "ch.h"
#include "hal.h"

#include "shell.h"
#include "chprintf.h"

#include "usbcfg.h"

#include "st7735.h"

static THD_WORKING_AREA(waThreadShell, 2048);
static THD_WORKING_AREA(waThread1, 128);
static THD_WORKING_AREA(waThreadDraw, 728);
uint32_t count = 0;

static void cmd_hello(BaseSequentialStream *chp, int argc, char *argv[]) {
	(void)argv;
	(void)argc;
	chprintf(chp, "hello\r\n");
}

static const ShellCommand commands[] = {
	{"hello", cmd_hello},
	{NULL, NULL}
};

static const ShellConfig shell_cfg1 = {
	(BaseSequentialStream *)&SDU1,
	commands
};

static THD_FUNCTION(Thread1, arg) {
	(void)arg;

	chRegSetThreadName("blink");
	while (true) {
		systime_t time = serusbcfg.usbp->state == USB_ACTIVE ? 100 : 500;
		palClearPad(GPIOC, 13);
		chThdSleepMilliseconds(time);
		palSetPad(GPIOC, 13);
		chThdSleepMilliseconds(time);
		count++;
	}
}

static THD_FUNCTION(ThreadDraw, arg) {
	(void)arg;

	char buf[64];
	uint8_t n = 0;

	chRegSetThreadName("draw");

	st7735_init();
	st7735_setRotate(0);

	st7735_fillRect(0, 0, 128, 128, RGB565(0xffffff));
	chThdSleepMilliseconds(1000);
	st7735_fillRect(0, 0, 128, 128, RGB565(0x000000));

	while (1) {
		chEvtWaitAnyTimeout((eventmask_t)1, TIME_MS2I(1000));


		n = 0;
		n = chsnprintf(buf + n, 32 - n, "PEP 0.00uW", count);
		buf[n] = '\0';
		st7735_drawStringScaled(5, 0, n, buf, RGB565(0xffffff), RGB565(0x000000), 1, 2);

		n = 0;
		n = chsnprintf(buf + n, 32 - n, "AVG 0.00uW", count);
		buf[n] = '\0';
		st7735_drawStringScaled(5, 20, n, buf, RGB565(0xffffff), RGB565(0x000000), 1, 2);
	}
}

int main(void) {
	halInit();
	chSysInit();

	// Remap SPI1 pins
	AFIO->MAPR |= AFIO_MAPR_SPI1_REMAP;
	// Only enable SWG
	// Release PA15(JTDI) PB3(JTDO/TRACESWO) PB4(NJRST)
	AFIO->MAPR |= 0b010 << AFIO_MAPR_SWJ_CFG_Pos;

	sduObjectInit(&SDU1);
	sduStart(&SDU1, &serusbcfg);

	usbDisconnectBus(serusbcfg.usbp);
	chThdSleepMilliseconds(1500);
	usbStart(serusbcfg.usbp, &usbcfg);
	usbConnectBus(serusbcfg.usbp);

	shellInit();

	chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);
	chThdCreateStatic(waThreadDraw, sizeof(waThreadDraw), NORMALPRIO+2, ThreadDraw, NULL);

	while (true) {
		if (SDU1.config->usbp->state == USB_ACTIVE) {
			thread_t *shelltp = chThdCreateStatic(
				waThreadShell, 
				sizeof(waThreadShell), 
				NORMALPRIO + 1,
				shellThread,
				(void *)&shell_cfg1
			);
			chThdWait(shelltp);
		}
		chThdSleepMilliseconds(1000);
	}
}
