/*
 * section and descriptor parser
 *
 * Copyright (C) 2005 Kenneth Aafloy (kenneth@linuxtv.org)
 * Copyright (C) 2005 Andrew de Quincey (adq_dvb@lidskialf.net)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifndef _UCSI_DVB_TERRESTRIAL_DELIVERY_DESCRIPTOR
#define _UCSI_DVB_TERRESTRIAL_DELIVERY_DESCRIPTOR 1

#include <ucsi/descriptor.h>
#include <ucsi/common.h>

struct dvb_terrestrial_delivery_descriptor {
	struct descriptor d;

	uint32_t centre_frequency;
  EBIT2(uint8_t bandwidth		: 3; ,
	uint8_t reserved_1		: 5; );
  EBIT3(uint8_t constellation		: 2; ,
	uint8_t hierarchy_information	: 3; ,
	uint8_t code_rate_hp_stream	: 3; );
  EBIT4(uint8_t code_rate_lp_stream	: 3; ,
	uint8_t guard_interval		: 2; ,
	uint8_t transmission_mode	: 2; ,
	uint8_t other_frequency_flag	: 1; );
	uint32_t reserved_2;
} packed;

static inline struct dvb_terrestrial_delivery_descriptor*
	dvb_terrestrial_delivery_descriptor_parse(struct descriptor* d)
{
	if (d->len != (sizeof(struct dvb_terrestrial_delivery_descriptor) - 2))
		return NULL;

	bswap32((uint8_t*) d + 2);
	bswap32((uint8_t*) d + 9);

	return (struct dvb_terrestrial_delivery_descriptor*) d;
}

#endif
