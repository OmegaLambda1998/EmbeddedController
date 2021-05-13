/*
 * Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "chipset.h"
#include "gpio.h"
#include "ec_commands.h"
#include "host_command.h"
#include "host_command_customization.h"
#include "hooks.h"
#include "keyboard_customization.h"
#include "lid_switch.h"
#include "lpc.h"
#include "power_button.h"
#include "switch.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "cypress5525.h"
#include "board.h"
#include "ps2mouse.h"
#include "keyboard_8042_sharedlib.h"
#define CPRINTS(format, args...) cprints(CC_SWITCH, format, ## args)

#ifdef CONFIG_EMI_REGION1

static void sci_enable(void);
DECLARE_DEFERRED(sci_enable);

int pos_get_state(void)
{
	if (*host_get_customer_memmap(0x00) & BIT(0))
		return true;
	else
		return false;
}

static void sci_enable(void)
{
	if (*host_get_customer_memmap(0x00) & BIT(0)) {
	/* when host set EC driver ready flag, EC need to enable SCI */
		lpc_set_host_event_mask(LPC_HOST_EVENT_SCI, SCI_HOST_EVENT_MASK);
		s5_power_up_control(0);
		update_soc_power_limit(1);
	} else
		hook_call_deferred(&sci_enable_data, 250 * MSEC);
}
#endif

/*****************************************************************************/
/* Hooks */
/**
 * Notify enter/exit flash through a host command
 */
static enum ec_status flash_notified(struct host_cmd_handler_args *args)
{

	const struct ec_params_flash_notified *p = args->params;

	switch (p->flags & 0x03) {
	case FLASH_FIRMWARE_START:
		CPRINTS("Start flashing firmware, disable power button and Lid");
		gpio_disable_interrupt(GPIO_ON_OFF_BTN_L);
		gpio_disable_interrupt(GPIO_ON_OFF_FP_L);
		gpio_disable_interrupt(GPIO_LID_SW_L);



		if ((p->flags & FLASH_FLAG_PD) == FLASH_FLAG_PD) {
			gpio_disable_interrupt(GPIO_EC_PD_INTA_L);
			gpio_disable_interrupt(GPIO_EC_PD_INTB_L);
		}
	case FLASH_ACCESS_SPI:
		/* Disable LED drv */
		gpio_set_level(GPIO_TYPEC_G_DRV2_EN, 0);
		/* Set GPIO56 as SPI for access SPI ROM */
		gpio_set_alternate_function(1, 0x4000, 2);
		break;

	case FLASH_FIRMWARE_DONE:
		CPRINTS("Flash done, recover the power button, lid");
		gpio_enable_interrupt(GPIO_ON_OFF_BTN_L);
		gpio_enable_interrupt(GPIO_ON_OFF_FP_L);
		gpio_enable_interrupt(GPIO_LID_SW_L);
		gpio_enable_interrupt(GPIO_EC_PD_INTA_L);
		gpio_enable_interrupt(GPIO_EC_PD_INTB_L);

		/* resetup PD controllers */
		if ((p->flags & FLASH_FLAG_PD) == FLASH_FLAG_PD) {
			cypd_reinitialize();
		}

	case FLASH_ACCESS_SPI_DONE:
		/* Set GPIO56 as PWM */
		gpio_set_alternate_function(1, 0x4000, 1);
		/* Enable LED drv */
		gpio_set_level(GPIO_TYPEC_G_DRV2_EN, 1);
		break;
	default:
		return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FLASH_NOTIFIED, flash_notified,
			EC_VER_MASK(0));


#ifdef CONFIG_FACTORY_SUPPORT

static enum ec_status factory_mode(struct host_cmd_handler_args *args)
{
	const struct ec_params_factory_notified *p = args->params;
	int enable = 1;


	if (p->flags)
		factory_setting(enable);
	else
		factory_setting(!enable);

	return EC_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FACTORY_MODE, factory_mode,
			EC_VER_MASK(0));
#endif

static enum ec_status host_custom_command_hello(struct host_cmd_handler_args *args)
{
	const struct ec_params_hello *p = args->params;
	struct ec_response_hello *r = args->response;
	uint32_t d = p->in_data;

	/**
	 * When system boot into OS, host will call this command to verify,
	 * It means system should in S0 state, and we need to set the resume
	 * S0ix flag to avoid the wrong state when unknown reason warm boot.
	 */
	if (chipset_in_state(CHIPSET_STATE_STANDBY))
		*host_get_customer_memmap(EC_EMEMAP_ER1_POWER_STATE) |= EC_PS_RESUME_S0ix;

	/**
	 * When system reboot and go into setup menu, we need to set the power_s5_up flag
	 * to wait SLP_S5 and SLP_S3 signal to boot into OS.
	 */
	s5_power_up_control(1);
	update_me_change(0);

	/**
	 * Moved sci enable on this host command, we need to check acpi_driver ready flag
	 * every boot up (both cold boot and warn boot)
	 */

#ifdef CONFIG_EMI_REGION1
	hook_call_deferred(&sci_enable_data, 250 * MSEC);
#endif


	r->out_data = d + 0x01020304;
	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_CUSTOM_HELLO, host_custom_command_hello, EC_VER_MASK(0));


static enum ec_status disable_ps2_mouse_emulation(struct host_cmd_handler_args *args)
{
	const struct ec_params_ps2_emulation_control *p = args->params;

	set_ps2_mouse_emulation(p->disable);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_DISABLE_PS2_EMULATION, disable_ps2_mouse_emulation, EC_VER_MASK(0));

static enum ec_status cmd_diagnosis(struct host_cmd_handler_args *args)
{

	const struct ec_params_diagnosis *p = args->params;
	uint8_t led_type;

	if (p->error_type & EC_CMD_DIAGNOSIS_LED){

		led_type = p->error_type & EC_CMD_DIAGNOSIS_LED_TYPE;
		switch (led_type) {
		case TYPE_DDR:
			// TODO: set module;
			break;
		default:
			break;
		}

		switch (p->error_subtype) {
		case TYPE_DDR_FAIL:
			// TODO: set pattern;
			break;
		default:
			break;
		}
	}

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_DIAGNOSIS, cmd_diagnosis,
			EC_VER_MASK(0));

static enum ec_status update_keyboard_matrix(struct host_cmd_handler_args *args)
{
	const struct ec_params_update_keyboard_matrix *p = args->params;
	struct ec_params_update_keyboard_matrix *r = args->response;

	int i;

	if (p->num_items > 32) {
		return EC_ERROR_INVAL;
	}
	if (p->write) {
		for (i = 0; i < p->num_items; i++) {
			set_scancode_set2(p->scan_update[i].row,p->scan_update[i].col,p->scan_update[i].scanset);
		}
	}
	r->num_items = p->num_items;
	for (i = 0; i < p->num_items; i++) {
		r->scan_update[i].row = p->scan_update[i].row;
		r->scan_update[i].col = p->scan_update[i].col;
		r->scan_update[i].scanset = get_scancode_set2(p->scan_update[i].row,p->scan_update[i].col);
	}
	args->response_size = sizeof(struct ec_params_update_keyboard_matrix);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_UPDATE_KEYBOARD_MATRIX, update_keyboard_matrix, EC_VER_MASK(0));
