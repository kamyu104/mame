/**********************************************************************

    8 bit latch interface and emulation

    2008/08     couriersud

**********************************************************************/

#include "driver.h"
#include "memconv.h"
#include "sound/discrete.h"
#include "latch8.h"

typedef struct _latch8_t latch8_t;
struct _latch8_t
{
	latch8_config	*intf;
	UINT8			 value;
	UINT8			 has_node_map;
	UINT8			 has_devread;
	UINT8			 has_read;
	const device_config	*devices[8];
};

/* ----------------------------------------------------------------------- */

INLINE latch8_t *get_safe_token(const device_config *device) {
	assert( device != NULL );
	assert( device->token != NULL );
	assert( device->type == LATCH8 );
	return ( latch8_t * ) device->token;
}

static void update(const device_config *device, UINT8 new_val, UINT8 mask)
{
	latch8_t *latch8 = get_safe_token(device);
	UINT8 old_val = latch8->value;
	
	latch8->value = (latch8->value & ~mask) | (new_val & mask);

	if (latch8->has_node_map)
	{
		int i;
		UINT8 changed = old_val ^ latch8->value;
		for (i=0; i<8; i++)
			if (((changed & (1<<i)) != 0) && latch8->intf->node_map[i] != 0)
				discrete_sound_w(device->machine, latch8->intf->node_map[i] , (latch8->value >> i) & 1);
	}
}

static TIMER_CALLBACK( latch8_timerproc )
{
	const device_config *device = ptr;
	UINT8 new_val = param & 0xFF;
	UINT8 mask = param >> 8;
	
	update(device, new_val, mask);
}

/* ----------------------------------------------------------------------- */

READ8_DEVICE_HANDLER( latch8_r )
{
	latch8_t *latch8 = get_safe_token(device);
	UINT8 res;
	
	assert(offset == 0);

	res = latch8->value;
	if (latch8->has_devread)
	{
		int i;
		for (i=0; i<8; i++)
		{
			const device_config *read_dev = latch8->devices[i];
			if (read_dev != NULL)
			{
				res &= ~( 1 << i);
				res |= ((latch8->intf->devread[i].devread_handler(read_dev, 0) >> latch8->intf->devread[i].from_bit) & 0x01) << i;
			}
		}
	}
	if (latch8->has_read)
	{
		int i;
		for (i=0; i<8; i++)
		{
			if (latch8->intf->devread[i].read_handler != NULL)
			{
				res &= ~( 1 << i);
				res |= ((latch8->intf->devread[i].read_handler(device->machine, 0) >> latch8->intf->devread[i].from_bit) & 0x01) << i;
			}
		}
	}

	return (res & ~latch8->intf->maskout) ^ latch8->intf->xor;
}


WRITE8_DEVICE_HANDLER( latch8_w )
{
	latch8_t *latch8 = get_safe_token(device);
	assert(offset == 0);

	if (latch8->intf->nosync != 0xff)
		timer_call_after_resynch((void *)device, (0xFF << 8) | data, latch8_timerproc);
	else
		update(device, data, 0xFF);
}


WRITE8_DEVICE_HANDLER( latch8_reset)
{
	latch8_t *latch8 = get_safe_token(device);

	assert(offset == 0);

	latch8->value = 0;
}

/* read bit x                 */
/* return (latch >> x) & 0x01 */ 

INLINE UINT8 latch8_bitx_r(const device_config *device, offs_t offset, int bit)
{
	latch8_t *latch8 = get_safe_token(device);

	assert( offset == 0);
	
	return (latch8->value >> bit) & 0x01;
}

READ8_DEVICE_HANDLER( latch8_bit0_r) { return latch8_bitx_r(device, offset, 0); }
READ8_DEVICE_HANDLER( latch8_bit1_r) { return latch8_bitx_r(device, offset, 1); }
READ8_DEVICE_HANDLER( latch8_bit2_r) { return latch8_bitx_r(device, offset, 2); }
READ8_DEVICE_HANDLER( latch8_bit3_r) { return latch8_bitx_r(device, offset, 3); }
READ8_DEVICE_HANDLER( latch8_bit4_r) { return latch8_bitx_r(device, offset, 4); }
READ8_DEVICE_HANDLER( latch8_bit5_r) { return latch8_bitx_r(device, offset, 5); }
READ8_DEVICE_HANDLER( latch8_bit6_r) { return latch8_bitx_r(device, offset, 6); }
READ8_DEVICE_HANDLER( latch8_bit7_r) { return latch8_bitx_r(device, offset, 7); }

READ8_DEVICE_HANDLER( latch8_bit0_q_r) { return latch8_bitx_r(device, offset, 0) ^ 1; }
READ8_DEVICE_HANDLER( latch8_bit1_q_r) { return latch8_bitx_r(device, offset, 1) ^ 1; }
READ8_DEVICE_HANDLER( latch8_bit2_q_r) { return latch8_bitx_r(device, offset, 2) ^ 1; }
READ8_DEVICE_HANDLER( latch8_bit3_q_r) { return latch8_bitx_r(device, offset, 3) ^ 1; }
READ8_DEVICE_HANDLER( latch8_bit4_q_r) { return latch8_bitx_r(device, offset, 4) ^ 1; }
READ8_DEVICE_HANDLER( latch8_bit5_q_r) { return latch8_bitx_r(device, offset, 5) ^ 1; }
READ8_DEVICE_HANDLER( latch8_bit6_q_r) { return latch8_bitx_r(device, offset, 6) ^ 1; }
READ8_DEVICE_HANDLER( latch8_bit7_q_r) { return latch8_bitx_r(device, offset, 7) ^ 1; }

/* write bit x from data into bit determined by offset */
/* latch = (latch & ~(1<<offset)) | (((data >> x) & 0x01) << offset) */

INLINE void latch8_bitx_w(const device_config *device, int bit, offs_t offset, UINT8 data)
{
	latch8_t *latch8 = get_safe_token(device);
	UINT8 mask = (1<<offset);
	UINT8 masked_data = (((data >> bit) & 0x01) << offset);

	assert( offset < 8);
	
	/* No need to synchronize ? */
	if (latch8->intf->nosync & mask)
		update(device, masked_data, mask);
	else
		timer_call_after_resynch((void *) device, (mask << 8) | masked_data, latch8_timerproc);
}

WRITE8_DEVICE_HANDLER( latch8_bit0_w ) { latch8_bitx_w(device, 0, offset, data); }
WRITE8_DEVICE_HANDLER( latch8_bit1_w ) { latch8_bitx_w(device, 1, offset, data); }
WRITE8_DEVICE_HANDLER( latch8_bit2_w ) { latch8_bitx_w(device, 2, offset, data); }
WRITE8_DEVICE_HANDLER( latch8_bit3_w ) { latch8_bitx_w(device, 3, offset, data); }
WRITE8_DEVICE_HANDLER( latch8_bit4_w ) { latch8_bitx_w(device, 4, offset, data); }
WRITE8_DEVICE_HANDLER( latch8_bit5_w ) { latch8_bitx_w(device, 0, offset, data); }
WRITE8_DEVICE_HANDLER( latch8_bit6_w ) { latch8_bitx_w(device, 0, offset, data); }
WRITE8_DEVICE_HANDLER( latch8_bit7_w ) { latch8_bitx_w(device, 0, offset, data); }

/* ----------------------------------------------------------------------- */

/* device interface */

static DEVICE_START( latch8 )
{
	latch8_t *latch8 = get_safe_token(device);
	char unique_tag[30];
	int i;

	/* validate arguments */
	assert(strlen(device->tag) < 20);

	latch8->intf = device->inline_config;

	latch8->value = 0x0;
	
	/* setup nodemap */
	for (i=0; i<8; i++)
		if (latch8->intf->node_map[i] )
			latch8->has_node_map = 1;

	/* setup device read handlers */
	for (i=0; i<8; i++)
		if (latch8->intf->devread[i].tag != NULL)
		{
			if (latch8->devices[i] != NULL)
				fatalerror("Device %s: Bit %d already has a handler.\n", device->tag, i);
			latch8->devices[i] = devtag_get_device(device->machine, 
					latch8->intf->devread[i].type,
					latch8->intf->devread[i].tag);
			if (latch8->devices[i] == NULL)
				fatalerror("Device %s: Unable to find device %s\n", device->tag, latch8->intf->devread[i].tag);
			latch8->has_devread = 1;
		}

	/* setup machine read handlers */
	for (i=0; i<8; i++)
		if (latch8->intf->devread[i].read_handler != NULL)
		{
			if (latch8->devices[i] != NULL)
				fatalerror("Device %s: Bit %d already has a handler.\n", device->tag, i);
			latch8->has_read = 1;
		}

	state_save_combine_module_and_tag(unique_tag, "latch8", device->tag);

	state_save_register_item(unique_tag, 0, latch8->value);
}


static DEVICE_RESET( latch8 )
{
	latch8_t *latch8 = get_safe_token(device);

	latch8->value = 0;
}


static DEVICE_SET_INFO( latch8 )
{
	switch (state)
	{
		/* no parameters to set */
	}
}


DEVICE_GET_INFO( latch8 )
{
	switch (state)
	{
		/* --- the following bits of info are returned as 64-bit signed integers --- */
		case DEVINFO_INT_TOKEN_BYTES:					info->i = sizeof(latch8_t);				break;
		case DEVINFO_INT_INLINE_CONFIG_BYTES:			info->i = sizeof(latch8_config);							break;
		case DEVINFO_INT_CLASS:							info->i = DEVICE_CLASS_PERIPHERAL;		break;

		/* --- the following bits of info are returned as pointers to data or functions --- */
		case DEVINFO_FCT_SET_INFO:						info->set_info = DEVICE_SET_INFO_NAME(latch8); break;
		case DEVINFO_FCT_START:							info->start = DEVICE_START_NAME(latch8);break;
		case DEVINFO_FCT_STOP:							/* Nothing */							break;
		case DEVINFO_FCT_RESET:							info->reset = DEVICE_RESET_NAME(latch8);break;

		/* --- the following bits of info are returned as NULL-terminated strings --- */
		case DEVINFO_STR_NAME:							info->s = "8 bit latch";				break;
		case DEVINFO_STR_FAMILY:						info->s = "Latches";					break;
		case DEVINFO_STR_VERSION:						info->s = "1.0";						break;
		case DEVINFO_STR_SOURCE_FILE:					info->s = __FILE__;						break;
		case DEVINFO_STR_CREDITS:						info->s = "Copyright Nicola Salmoria and the MAME Team"; break;
	}
}
