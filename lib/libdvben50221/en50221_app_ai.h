/*
    en50221 encoder An implementation for libdvb
    an implementation for the en50221 transport layer

    Copyright (C) 2004, 2005 Manu Abraham (manu@kromtek.com)
    Copyright (C) 2005 Julian Scheel (julian at jusst dot de)
    Copyright (C) 2006 Andrew de Quincey (adq_dvb@lidskialf.net)

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as
    published by the Free Software Foundation; either version 2.1 of
    the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
*/

#ifndef __EN50221_APPLICATION_AI_H__
#define __EN50221_APPLICATION_AI_H__

#include <stdlib.h>
#include <stdint.h>
#include <en50221_session.h>
#include <en50221_app_rm.h>

/**
 * Type definition for application callback function - called when we receive
 * an application info object.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Resource id concerned.
 * @param application_type Type of application.
 * @param application_manufacturer Manufacturer of application.
 * @param manufacturer_code Manufacturer specific code.
 * @param menu_string_length Length of menu string.
 * @param menu_string The menu string itself.
 */
typedef void (*en50221_app_ai_callback)(void *arg, uint8_t slot_id, uint8_t session_number,
                                        uint8_t application_type, uint16_t application_manufacturer,
                                        uint16_t manufacturer_code, uint8_t menu_string_length,
                                        uint8_t *menu_string);

/**
 * Opaque type representing an application information resource.
 */
typedef void *en50221_app_ai;

/**
 * Create an instance of an application information resource.
 *
 * @param sl Session layer to communicate with.
 * @param rm Resource manager instance to register with.
 * @return Instance, or NULL on failure.
 */
extern en50221_app_rm en50221_app_ai_create(en50221_session_layer sl, en50221_app_rm rm);

/**
 * Destroy an instance of an application information resource.
 *
 * @param rm Instance to destroy.
 */
extern void en50221_app_ai_destroy(en50221_app_ai ai);

/**
 * Register a callback for reception of application_info objects.
 *
 * @param ai Application information instance.
 * @param callback Callback function.
 * @param arg Private argument passed during calls to the callback.
 */
extern void en50221_app_ai_register_callback(en50221_app_ai ai, en50221_app_ai_callback, void *arg);

/**
 * send a enquiry for the app_info provided by a module
 *
 * @param ai Application information instance.
 * @param session_number Session to send on.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_ai_enquiry(en50221_app_ai ai, uint8_t session_number);

/**
 * send a enter_menu tag, this will make the application
 * open a new MMI session to provide a Menu, or so.
 *
 * @param ai Application information instance.
 * @param session_number Session to send on.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_ai_entermenu(en50221_app_ai ai, uint8_t session_number);

#endif
