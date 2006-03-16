/*
    en50221 encoder An implementation for libdvb
    an implementation for the en50221 transport layer

    Copyright (C) 2004, 2005 Manu Abraham (manu@kromtek.com)
    Copyright (C) 2005 Julian Scheel (julian at jusst dot de)

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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/delay.h>
#include <time.h>
#include <dvbmisc.h>
#include <sys/uio.h>
#include "en50221_transport.h"
#include "en50221_session.h"
#include "en50221_errno.h"

#define ST_OPEN_SESSION_REQ     0x91    // h<--m
#define ST_OPEN_SESSION_RES     0x92    // h-->m
#define ST_CREATE_SESSION       0x93    // h-->m
#define ST_CREATE_SESSION_RES   0x94    // h<--m
#define ST_CLOSE_SESSION_REQ    0x95    // h<->m
#define ST_CLOSE_SESSION_RES    0x96    // h<->m
#define ST_SESSION_NUMBER       0x90    // h<->m

// for each session we store its identifier, the resource-id
// it is linked to and the callback of the specific resource
struct en50221_session {
    uint32_t resource_id;
    int slot_id; // -1 if unused
    int connection_id; // -1 if unused

    en50221_sl_resource_callback callback;
    void *callback_arg;
};

struct en50221_session_layer_private
{
    uint8_t max_sessions;
    en50221_transport_layer tl;

    en50221_sl_lookup_callback lookup;
    void *lookup_arg;

    int error;

    struct en50221_session *sessions;
};

static void en50221_sl_transport_callback(void *arg, int reason, uint8_t *data, uint32_t data_length,
                                          uint8_t slot_id, uint8_t connection_id);




en50221_session_layer en50221_sl_create(en50221_transport_layer tl,
                                        uint32_t max_sessions)
{
    struct en50221_session_layer_private *private = NULL;
    uint32_t i;

    // setup structure
    private = (struct en50221_session_layer_private*) malloc(sizeof(struct en50221_session_layer_private));
    if (private == NULL)
        goto error_exit;
    private->max_sessions = max_sessions;
    private->lookup = NULL;
    private->lookup_arg = NULL;
    private->tl = tl;
    private->error = 0;

    // create the slots
    private->sessions = malloc(sizeof(struct en50221_session) * max_sessions);
    if (private->sessions == NULL)
        goto error_exit;

    // set them up
    for(i=0; i< max_sessions; i++) {
        private->sessions[i].slot_id = -1;
        private->sessions[i].connection_id = -1;
        private->sessions[i].callback = NULL;
    }
    en50221_tl_register_callback(tl, en50221_sl_transport_callback, private);

    return private;

error_exit:
    en50221_sl_destroy(private);
    return NULL;
}

void en50221_sl_destroy(en50221_session_layer sl)
{
    struct en50221_session_layer_private *private = (struct en50221_session_layer_private *) sl;

    if (private) {
        if (private->sessions) {
            free(private->sessions);
        }
        free(private);
    }
}

void en50221_sl_register_lookup_callback(en50221_session_layer sl, en50221_sl_lookup_callback callback, void *arg)
{
    struct en50221_session_layer_private *private = (struct en50221_session_layer_private *) sl;

    private->lookup = callback;
    private->lookup_arg = arg;
}

int en50221_sl_send_data(en50221_session_layer sl, uint8_t session_number, uint8_t *data, uint16_t data_length)
{
    struct en50221_session_layer_private *private = (struct en50221_session_layer_private *) sl;
    struct iovec iov[2];

    if (session_number >= private->max_sessions) {
        private->error = EN50221ERR_BADSESSIONNUMBER;
        return -1;
    }
    if (private->sessions[session_number].slot_id == -1) {
        private->error = EN50221ERR_BADSLOTID;
        return -1;
    }

    // make up the header
    uint8_t hdr[4];
    hdr[0] = ST_SESSION_NUMBER;
    hdr[1] = 2;
    hdr[2] = session_number << 8;
    hdr[3] = session_number;
    iov[0].iov_base = hdr;
    iov[0].iov_len = 4;

    // make up the data
    iov[1].iov_base = data;
    iov[1].iov_len = data_length;

    // send this command
    uint8_t slot_id = private->sessions[session_number].slot_id;
    uint8_t connection_id = private->sessions[session_number].connection_id;
    if (en50221_tl_send_data(private->tl, slot_id, connection_id, iov, 2)) {
        private->error = en50221_tl_get_error(private->tl);
        return -1;
    }
    return 0;
}

int en50221_sl_broadcast_data(en50221_session_layer sl, uint32_t resource_id,
                              uint8_t *data, uint16_t data_length)
{
    struct en50221_session_layer_private *private = (struct en50221_session_layer_private *) sl;
    int i;

    for(i = 0; i < private->max_sessions; i++)
    {
        if (private->sessions[i].slot_id == -1)
            continue;

        if (private->sessions[i].resource_id == resource_id) {
            if (en50221_sl_send_data(sl, i, data, data_length) < 0) {
                return -1;
            }
        }
    }

    return 0;
}



static void en50221_sl_handle_open_session_request(struct en50221_session_layer_private *private,
                                     uint8_t *data, uint32_t data_length, uint8_t slot_id, uint8_t connection_id)
{
    struct iovec iov[2];

    // check
    if (data_length < 5) {
        print(LOG_LEVEL, ERROR, 1, "Received data with invalid length from module on slot %02x\n", slot_id);
        return;
    }
    if (data[0] != 4) {
        print(LOG_LEVEL, ERROR, 1, "Received data with invalid length from module on slot %02x\n", slot_id);
        return;
    }

    // get the resource id and look it up
    uint32_t resource_id = (data[1] << 24) | (data[2] << 16) | (data[3] << 8) | data[4];
    void *arg;
    en50221_sl_resource_callback resource_callback = NULL;
    int status = S_STATUS_CLOSE_NO_RES;
    if (private->lookup) {
        status = private->lookup(private->lookup_arg, resource_id, &arg, &resource_callback);
    }

    // if we found it, deal with it
    int session_number = -1;
    if (status == S_STATUS_OPEN) {
        // lookup next free session_id:
        int i;
        for(i = 0; i < private->max_sessions; i++) {
            if (private->sessions[i].slot_id == -1) {
                session_number = i;
                break;
            }
        }
        if (session_number == -1) {
            print(LOG_LEVEL, ERROR, 1, "Ran out of sessions for module on slot %02x\n", slot_id);
            status = S_STATUS_CLOSE_NO_RES;
        }
    }

    // make up the header
    uint8_t hdr[9];
    hdr[0] = ST_OPEN_SESSION_RES;
    hdr[1] = 7;
    hdr[2] = status;
    hdr[3] = resource_id >> 24;
    hdr[4] = resource_id >> 16;
    hdr[5] = resource_id >> 8;
    hdr[6] = resource_id;
    hdr[7] = session_number >> 8;
    hdr[8] = session_number;
    iov[0].iov_base = hdr;
    iov[0].iov_len = 9;

    // send this command
    if (en50221_tl_send_data(private->tl, slot_id, connection_id, iov, 2)) {
        print(LOG_LEVEL, ERROR, 1, "Transport layer error %i occurred\n", en50221_tl_get_error(private->tl));
        return;
    }

    // setup the session
    private->sessions[session_number].resource_id = resource_id;
    private->sessions[session_number].slot_id = slot_id;
    private->sessions[session_number].connection_id = connection_id;
    private->sessions[session_number].callback = resource_callback;
    private->sessions[session_number].callback_arg = arg;

    // callback to announce creation
    if (resource_callback)
        resource_callback(arg, S_CALLBACK_REASON_CONNECT, session_number, resource_id, NULL, 0);
}

static void en50221_sl_handle_close_session_request(struct en50221_session_layer_private *private,
                                      uint8_t *data, uint32_t data_length, uint8_t slot_id, uint8_t connection_id)
{
    // check
    if (data_length < 3) {
        print(LOG_LEVEL, ERROR, 1, "Received data with invalid length from module on slot %02x\n", slot_id);
        return;
    }
    if (data[0] != 2) {
        print(LOG_LEVEL, ERROR, 1, "Received data with invalid length from module on slot %02x\n", slot_id);
        return;
    }

    // extract session number
    uint16_t session_number = (data[1] << 8) | data[2];

    // make up the response
    struct iovec iov[1];
    uint8_t hdr[5];
    hdr[0] = ST_CLOSE_SESSION_RES;
    hdr[1] = 3;
    hdr[2] = 0x00; // session closed
    hdr[3] = session_number << 8;
    hdr[4] = session_number;
    iov[0].iov_base = hdr;
    iov[0].iov_len = 5;

    // check session number is ok
    if (session_number >= private->max_sessions) {
        hdr[2] = 0xF0; // session close error
        print(LOG_LEVEL, ERROR, 1, "Received bad session id %i\n", slot_id);
    } else if (slot_id != private->sessions[session_number].slot_id) {
        hdr[2] = 0xF0; // session close error
        print(LOG_LEVEL, ERROR, 1, "Received unexpected session on invalid slot %i\n", slot_id);
    } else if (connection_id != private->sessions[session_number].connection_id) {
        hdr[2] = 0xF0; // session close error
        print(LOG_LEVEL, ERROR, 1, "Received unexpected session on invalid slot %i\n", slot_id);
    } else { // was ok!
        private->sessions[session_number].slot_id = -1;
        private->sessions[session_number].connection_id = -1;
    }

    // sendit
    if (en50221_tl_send_data(private->tl, slot_id, connection_id, iov, 1)) {
        print(LOG_LEVEL, ERROR, 1, "Transport layer reports error %i on slot %i\n",
              en50221_tl_get_error(private->tl), slot_id);
    }

    // callback to announce destruction
    if (private->sessions[session_number].callback)
        private->sessions[session_number].callback(private->sessions[session_number].callback_arg,
                                                   S_CALLBACK_REASON_CLOSE,
                                                   session_number,
                                                   private->sessions[session_number].resource_id,
                                                   NULL, 0);
}

static void en50221_sl_handle_session_package(struct en50221_session_layer_private *private,
                                              uint8_t *data, uint32_t data_length, uint8_t slot_id, uint8_t connection_id)
{
    // check
    if (data_length < 3) {
        print(LOG_LEVEL, ERROR, 1, "Received data with invalid length from module on slot %i\n", slot_id);
        return;
    }
    if (data[0] != 2) {
        print(LOG_LEVEL, ERROR, 1, "Received data with invalid length from module on slot %i\n", slot_id);
        return;
    }

    // get session number
    uint16_t session_number = (data[1] << 8) | data[2];
    if (session_number >= private->max_sessions) {
        print(LOG_LEVEL, ERROR, 1, "Received data with bad session_number from module on slot %i\n", slot_id);
        return;
    } else if (slot_id != private->sessions[session_number].slot_id) {
        print(LOG_LEVEL, ERROR, 1, "Received unexpected session on invalid slot %i\n", slot_id);
        return;
    } else if (connection_id != private->sessions[session_number].connection_id) {
        print(LOG_LEVEL, ERROR, 1, "Received unexpected session on invalid slot %i\n", slot_id);
        return;
    }

    // this resources callback is called
    // we carry the session_number to give
    // the resource the ability to send response-packages
    if (private->sessions[session_number].callback)
        private->sessions[session_number].callback(private->sessions[session_number].callback_arg,
                                                   S_CALLBACK_REASON_DATA,
                                                   session_number,
                                                   private->sessions[session_number].resource_id,
                                                   data + 3, data_length - 3);
}

static void en50221_sl_transport_callback(void *arg, int reason, uint8_t *data, uint32_t data_length,
                                          uint8_t slot_id, uint8_t connection_id)
{
    struct en50221_session_layer_private *private = (struct en50221_session_layer_private *) arg;
    int i;

    // deal with the reason for this callback
    switch(reason) {
    case T_CALLBACK_REASON_DATA:
        // fallthrough into rest of this function
        break;

    case T_CALLBACK_REASON_CONNECTIONCLOSE:
        for(i=0; i< private->max_sessions; i++) {
            if (private->sessions[i].connection_id == connection_id) {
                private->sessions[i].slot_id = -1;
                private->sessions[i].connection_id = -1;
                if (private->sessions[i].callback)
                    private->sessions[i].callback(private->sessions[i].callback_arg,
                                                  S_CALLBACK_REASON_CLOSE,
                                                  i,
                                                  private->sessions[i].resource_id,
                                                  NULL, 0);
            }
        }
        return;

    case T_CALLBACK_REASON_SLOTCLOSE:
        for(i=0; i< private->max_sessions; i++) {
            if (private->sessions[i].slot_id == slot_id) {
                private->sessions[i].slot_id = -1;
                private->sessions[i].connection_id = -1;
                if (private->sessions[i].callback)
                    private->sessions[i].callback(private->sessions[i].callback_arg,
                                                  S_CALLBACK_REASON_CLOSE,
                                                  i,
                                                  private->sessions[i].resource_id,
                                                  NULL, 0);
            }
        }
        return;
    }

    // sanity check data length
    if (data_length < 1) {
        print(LOG_LEVEL, ERROR, 1, "Received data with invalid length from module on slot %i\n", slot_id);
        return;
    }

    // deal with the data
    uint8_t spdu_tag = data[0];
    switch(spdu_tag)
    {
        case ST_OPEN_SESSION_REQ:
            en50221_sl_handle_open_session_request(private, data+1, data_length-1, slot_id, connection_id);
            break;

        case ST_CLOSE_SESSION_REQ:
            en50221_sl_handle_close_session_request(private, data+1, data_length-1, slot_id, connection_id);
            break;

        case ST_SESSION_NUMBER:
            en50221_sl_handle_session_package(private, data+1, data_length-1, slot_id, connection_id);
            break;

        case ST_CREATE_SESSION_RES:
            print(LOG_LEVEL, ERROR, 1, "Received ST_CREATE_SESSION_RES from module on slot %i\n", slot_id);
            break;

        case ST_CLOSE_SESSION_RES:
            print(LOG_LEVEL, ERROR, 1, "Received ST_CLOSE_SESSION_RES from module on slot %i", slot_id);
            break;

        default:
            print(LOG_LEVEL, ERROR, 1, "Received unknown session tag %02x from module on slot %i", spdu_tag, slot_id);
            break;
    }
}
