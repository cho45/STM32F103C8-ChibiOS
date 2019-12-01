#include <stdio.h>
#include <string.h>

#include "ch.h"
#include "hal.h"

#include "shell.h"
#include "chprintf.h"

#include "usbcfg.h"

static THD_WORKING_AREA(waThreadShell, 2048);
static THD_WORKING_AREA(waThread1, 128);

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
	}
}

int main(void) {
	halInit();
	chSysInit();

	sduObjectInit(&SDU1);
	sduStart(&SDU1, &serusbcfg);

	usbDisconnectBus(serusbcfg.usbp);
	chThdSleepMilliseconds(1500);
	usbStart(serusbcfg.usbp, &usbcfg);
	usbConnectBus(serusbcfg.usbp);

	shellInit();

	chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

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
