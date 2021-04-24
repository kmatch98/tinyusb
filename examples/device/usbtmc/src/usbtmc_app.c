/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Nathan Conrad
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <strings.h>
#include <stdlib.h>     /* atoi */
#include "tusb.h"
#include "bsp/board.h"
#include "main.h"

#include "scpi/parser.h"
#include "scpi-def.h"

#if (CFG_TUD_USBTMC_ENABLE_488)
static usbtmc_response_capabilities_488_t const
#else
static usbtmc_response_capabilities_t const
#endif
tud_usbtmc_app_capabilities  =
{
    .USBTMC_status = USBTMC_STATUS_SUCCESS,
    .bcdUSBTMC = USBTMC_VERSION,
    .bmIntfcCapabilities =
    {
        .listenOnly = 0,
        .talkOnly = 0,
        .supportsIndicatorPulse = 1
    },
    .bmDevCapabilities = {
        .canEndBulkInOnTermChar = 0
    },

#if (CFG_TUD_USBTMC_ENABLE_488)
    .bcdUSB488 = USBTMC_488_VERSION,
    .bmIntfcCapabilities488 =
    {
        .supportsTrigger = 1,
        .supportsREN_GTL_LLO = 0,
        .is488_2 = 1
    },
    .bmDevCapabilities488 =
    {
      .SCPI = 1,
      .SR1 = 0,
      .RL1 = 0,
      .DT1 =0,
    }
#endif
};

#define IEEE4882_STB_QUESTIONABLE (0x08u)
#define IEEE4882_STB_MAV          (0x10u)
#define IEEE4882_STB_SER          (0x20u)
#define IEEE4882_STB_SRQ          (0x40u)

//static const char idn[] = "AdafruitIndustries,TinyLogicFriend-ItsyBitsyM4,SN0001,FirmwareVer123456\r\n";
//static const char idn[] = "TinyUSB,ModelNumber,SerialNumber,FirmwareVer and a bunch of other text to make it longer than a packet, perhaps? lets make it three transfers...\n";
static const char luvu2[] = "LUVU2\r\n";
static volatile uint8_t status;

// 0=not query, 1=queried, 2=delay,set(MAV), 3=delay 4=ready?
// (to simulate delay)
static volatile uint16_t queryState = 0;
static volatile uint32_t queryDelayStart;
static volatile uint32_t bulkInStarted;
static volatile uint32_t idnQuery;
static volatile uint32_t luvQuery;
static volatile uint32_t parseQuery = 0;

//static uint32_t resp_delay = 125u; // Adjustable delay, to allow for better testing
static uint32_t resp_delay = 1u; // Adjustable delay, to allow for better testing
static size_t buffer_outlen = 0;
static size_t buffer_inlen  = 0;
static size_t buffer_tx_ix; // for transmitting using multiple transfers
static uint8_t buffer_out[225]; // Receive packets: A few packets long should be enough.
//static uint8_t buffer_in[225]; // Return packets: A few packets long should be enough.
static uint8_t buffer_in[4096]; // Return packets: A few packets long should be enough.




static usbtmc_msg_dev_dep_msg_in_header_t rspMsg = {
    .bmTransferAttributes =
    {
      .EOM = 1,
      .UsingTermChar = 0
    }
};

void tud_usbtmc_open_cb(uint8_t interface_id)
{
  (void)interface_id;
  tud_usbtmc_start_bus_read();
}

#if (CFG_TUD_USBTMC_ENABLE_488)
usbtmc_response_capabilities_488_t const *
#else
usbtmc_response_capabilities_t const *
#endif
tud_usbtmc_get_capabilities_cb()
{
  return &tud_usbtmc_app_capabilities;
}


bool tud_usbtmc_msg_trigger_cb(usbtmc_msg_generic_t* msg) {
  (void)msg;
  // Let trigger set the SRQ
  status |= IEEE4882_STB_SRQ;
  return true;
}

bool tud_usbtmc_msgBulkOut_start_cb(usbtmc_msg_request_dev_dep_out const * msgHeader)
{
  (void)msgHeader;
  buffer_outlen = 0;
  if(msgHeader->TransferSize > sizeof(buffer_out))
  {

    return false;
  }
  return true;
}

void set_parse_query(uint32_t setting){
  // used to trigger a BulkIn send when a message response is complete
  parseQuery=setting;
}

bool copy_to_buffer(const char *data, size_t len)
{

  //* todo * check for buffer overrun

  memcpy(&(buffer_in[buffer_inlen]), data, len);
  buffer_inlen += len;

  //board_led_write(0);
  queryState=4; // trial

  return true;

}

bool tud_usbtmc_msg_data_cb(void *data, size_t len, bool transfer_complete)
{
  //(void) transfer_complete;

  // If transfer isn't finished, we just ignore it (for now)

  if(len + buffer_outlen < sizeof(buffer_out))
  {
    memcpy(&(buffer_out[buffer_outlen]), data, len);
    buffer_outlen += len;

    SCPI_Input(&scpi_context, data, len);
  }
  else
  {
    return false; // buffer overflow!
  }
  queryState = transfer_complete;
  idnQuery = 0;
  luvQuery = 0;

  if(transfer_complete && (len >=4) && !strncasecmp("*idn?",data,4)) //** change to handle by scpi_parser
  {
    idnQuery = 1;
    // board_led_write(0);
  }
  else if(transfer_complete && !strncasecmp("delay ",data,5))
  {
    queryState = 0;
    int d = atoi((char*)data + 5);
    if(d > 10000)
      d = 10000;
    if(d<0)
      d=0;
    resp_delay = (uint32_t)d;
  }
  else if(transfer_complete && !strncasecmp("LUVU",data,3))
  {

    luvQuery = 1;
  }
  else if(transfer_complete)
  {
    // Updated to use scpi_parser 8 April 2021 KMatocha
    // send data to the SCPI handler
    //board_led_write(0);

    // if transfer is complete, send an empty string to trigger SCPI_Input to act
    //SCPI_Input(&scpi_context, NULL, 0);

  }
  tud_usbtmc_start_bus_read();
  return true;
}

bool tud_usbtmc_msgBulkIn_complete_cb()
{
  if((buffer_tx_ix == buffer_outlen) || idnQuery || luvQuery) // done
  {
    status &= (uint8_t)~(IEEE4882_STB_MAV); // clear MAV
    queryState = 0;
    bulkInStarted = 0;
    buffer_tx_ix = 0;
  }
  tud_usbtmc_start_bus_read();

  return true;
}

static unsigned int msgReqLen;

bool tud_usbtmc_msgBulkIn_request_cb(usbtmc_msg_request_dev_dep_in const * request)
{
  rspMsg.header.MsgID = request->header.MsgID,
  rspMsg.header.bTag = request->header.bTag,
  rspMsg.header.bTagInverse = request->header.bTagInverse;
  msgReqLen = request->TransferSize;

#ifdef xDEBUG
  uart_tx_str_sync("MSG_IN_DATA: Requested!\r\n");
#endif
  if(queryState == 0 || (buffer_tx_ix == 0))
  {
    TU_ASSERT(bulkInStarted == 0);
    bulkInStarted = 1;

    // > If a USBTMC interface receives a Bulk-IN request prior to receiving a USBTMC command message
    //   that expects a response, the device must NAK the request (*not stall*)
  }
  else
  {
    size_t txlen = tu_min32(buffer_outlen-buffer_tx_ix,msgReqLen);
    tud_usbtmc_transmit_dev_msg_data(&buffer_out[buffer_tx_ix], txlen,
        (buffer_tx_ix+txlen) == buffer_outlen, false);
    buffer_tx_ix += txlen;
  }
  // Always return true indicating not to stall the EP.
  return true;
}

void usbtmc_app_task_iter(void) {
  switch(queryState) {
  case 0:
    break;
  case 1:
    queryDelayStart = board_millis();
    queryState = 2;
    break;
  case 2:
    if( (board_millis() - queryDelayStart) > resp_delay) {
      queryDelayStart = board_millis();
      queryState=3;
      status |= 0x10u; // MAV
      status |= 0x40u; // SRQ
    }
    break;
  case 3:
    if( (board_millis() - queryDelayStart) > resp_delay) {
      queryState = 4;
    }
    break;
  case 4: // time to transmit;
    if(bulkInStarted && (buffer_tx_ix == 0)) {
      // if(idnQuery)
      // {
      //   tud_usbtmc_transmit_dev_msg_data(idn, tu_min32(sizeof(idn)-1,msgReqLen),true,false);
      //   queryState = 0;
      //   bulkInStarted = 0;
      // }
      // else if(luvQuery){
      if(luvQuery){
        tud_usbtmc_transmit_dev_msg_data(luvu2, tu_min32(sizeof(luvu2)-1,msgReqLen),true,false);
        queryState = 0;
        bulkInStarted = 0;
        //board_led_write(1);
      }
      else if(parseQuery && (buffer_inlen > 0) ){
        //board_led_write(0);

        tud_usbtmc_transmit_dev_msg_data(buffer_in, buffer_inlen, true, false);
        //tud_usbtmc_transmit_dev_msg_data("buffer", 7, true, false);
        queryState = 0;
        bulkInStarted = 0;
        parseQuery = 0;
        buffer_inlen = 0;
      }
      else
      {
        // echo the command back to the host
        // buffer_tx_ix = tu_min32(buffer_outlen,msgReqLen);
        // tud_usbtmc_transmit_dev_msg_data(buffer_out, buffer_tx_ix, buffer_tx_ix == buffer_outlen, false);
        // queryState = 0;
        // bulkInStarted = 0;
      }
      // MAV is cleared in the transfer complete callback.
    }
    break;
  default:
    TU_ASSERT(false,);
    return;
  }
}

bool tud_usbtmc_initiate_clear_cb(uint8_t *tmcResult)
{
  *tmcResult = USBTMC_STATUS_SUCCESS;
  queryState = 0;
  bulkInStarted = false;
  status = 0;
  return true;
}

bool tud_usbtmc_check_clear_cb(usbtmc_get_clear_status_rsp_t *rsp)
{
  queryState = 0;
  bulkInStarted = false;
  status = 0;
  buffer_tx_ix = 0u;
  buffer_outlen = 0u;
  buffer_inlen = 0u;
  rsp->USBTMC_status = USBTMC_STATUS_SUCCESS;
  rsp->bmClear.BulkInFifoBytes = 0u;
  return true;
}
bool tud_usbtmc_initiate_abort_bulk_in_cb(uint8_t *tmcResult)
{
  bulkInStarted = 0;
  *tmcResult = USBTMC_STATUS_SUCCESS;
  return true;
}
bool tud_usbtmc_check_abort_bulk_in_cb(usbtmc_check_abort_bulk_rsp_t *rsp)
{
  (void)rsp;
  tud_usbtmc_start_bus_read();
  return true;
}

bool tud_usbtmc_initiate_abort_bulk_out_cb(uint8_t *tmcResult)
{
  *tmcResult = USBTMC_STATUS_SUCCESS;
  return true;

}
bool tud_usbtmc_check_abort_bulk_out_cb(usbtmc_check_abort_bulk_rsp_t *rsp)
{
  (void)rsp;
  tud_usbtmc_start_bus_read();
  return true;
}

void tud_usbtmc_bulkIn_clearFeature_cb(void)
{
}
void tud_usbtmc_bulkOut_clearFeature_cb(void)
{
  tud_usbtmc_start_bus_read();
}

// Return status byte, but put the transfer result status code in the rspResult argument.
uint8_t tud_usbtmc_get_stb_cb(uint8_t *tmcResult)
{
  uint8_t old_status = status;
  status = (uint8_t)(status & ~(IEEE4882_STB_SRQ)); // clear SRQ

  *tmcResult = USBTMC_STATUS_SUCCESS;
  // Increment status so that we see different results on each read...

  return old_status;
}

bool tud_usbtmc_indicator_pulse_cb(tusb_control_request_t const * msg, uint8_t *tmcResult)
{
  (void)msg;
  led_indicator_pulse();
  *tmcResult = USBTMC_STATUS_SUCCESS;
  return true;
}
