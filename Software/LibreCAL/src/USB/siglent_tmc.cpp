/*
 * Emulation for a Siglent SEM5000A eCal attached over USBTMC.
 *
 * Author: Joshua Wise <joshua@accelerated.tech>
 * Copyright (c) 2025 Accelerated Tech, Inc.
 *
 * The MIT License (MIT)
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

#include <string.h>
#include <stdlib.h>     /* atoi */
#include "tusb.h"
#include "bsp/board_api.h"
#include "serial.h"
#include "ff.h"
#include "Switch.hpp"
#include <ctype.h>
#include <cstring>

#define LOG_LEVEL	LOG_LEVEL_DEBUG
#define LOG_MODULE	"USB TMC"
#include "Log.h"

static usbtmc_response_capabilities_t const tud_usbtmc_app_capabilities = {
    .USBTMC_status = USBTMC_STATUS_SUCCESS,
    .bcdUSBTMC = USBTMC_VERSION,
    .bmIntfcCapabilities = {
        .listenOnly = 0,
        .talkOnly = 0,
        .supportsIndicatorPulse = 1
    },
    .bmDevCapabilities = {
        .canEndBulkInOnTermChar = 0
    },
};

static size_t ibuf_len;
static uint8_t ibuf[256];

static size_t obuf_pos = 0;
static size_t obuf_len = 0;
static uint8_t obuf[1024];

static FIL fl_file;
static bool fl_file_open = false;

extern "C" void tud_usbtmc_open_cb(uint8_t interface_id) {
  (void)interface_id;
  tud_usbtmc_start_bus_read();
}

extern "C"  usbtmc_response_capabilities_t const * tud_usbtmc_get_capabilities_cb() {
  return &tud_usbtmc_app_capabilities;
}


extern "C" bool tud_usbtmc_msg_trigger_cb(usbtmc_msg_generic_t* msg) {
  (void)msg;
  return true;
}

extern "C" bool tud_usbtmc_msgBulkOut_start_cb(usbtmc_msg_request_dev_dep_out const * msgHeader) {
  ibuf_len = 0;
  obuf_pos = 0;
  obuf_len = 0;
  if (msgHeader->TransferSize > sizeof(ibuf)) {
    return false;
  }
  return true;
}

extern "C" bool tud_usbtmc_msg_data_cb(void *data, size_t len, bool transfer_complete) {
  if (len + ibuf_len < sizeof(ibuf)) {
    memcpy(&(ibuf[ibuf_len]), data, len);
    ibuf_len += len;
  } else {
    return false; // buffer overflow!
  }
  
  if (!transfer_complete) {
    tud_usbtmc_start_bus_read();
    return true;
  }
  
  /* Ok, we're good, and we've received a message; go parse it. */
  ibuf[ibuf_len] = 0;
  LOG_DEBUG("USB TMC: %s", ibuf);
  
  obuf_len = 0;
  obuf_pos = 0;

  /* This SCPI interface supports a very different command set than the main
   * SCPI interface does, so we don't even share a parser.  */
  
  if (!strcasecmp((char *)ibuf, "*IDN?\n")) {
      obuf_pos = 0;
      obuf_len = snprintf((char *)obuf, sizeof(obuf), "LibreVNA,LibreCAL,%s,%d.%d.%d\n", getSerial(), FW_MAJOR, FW_MINOR, FW_PATCH);
  } else if (!strcasecmp((char *)ibuf, "FL:DATA:READ:START\n")) {
    /* The FL:DATA: commands convert to filesystem reads and writes.  In a
     * more serious USB device, we would kick these onto another thread, but
     * for this application it's ok to block the USB thread for the small
     * reads.
     */
    fl_file_open = f_open(&fl_file, "0:siglent/info.dat", FA_OPEN_EXISTING | FA_READ) == FR_OK;
  } else if (!strncasecmp((char *)ibuf, "FL:DATA:INDEX ", 14)) {
    int idx = atoi((char *)ibuf + 14);
    char name[32];
    snprintf(name, sizeof(name), "0:siglent/data%d.zip", idx);
    fl_file_open = f_open(&fl_file, name, FA_OPEN_EXISTING | FA_READ) == FR_OK;
  } else if (!strncasecmp((char *)ibuf, "FL:DATA:READ? ", 14)) {
    size_t req = atoi((char *)ibuf + 14);
    
    size_t len = tu_min32(sizeof(obuf), req);
    size_t rv;
    
    if (!fl_file_open) {
      goto done;
    }
    
    if (f_read(&fl_file, obuf, len, &rv) != FR_OK) {
      goto done;
    }

    obuf_len = rv;
  } else if (!strncasecmp((char *)ibuf, "SL ", 3)) {
    /* This is not a very robust parser. */
    char *buf = (char *)ibuf + 3;
    const char *cmd = strtok(buf, ",");
    if (!cmd) {
      goto done;
    }
    const char *srcport_s = strtok(NULL, ",");
    if (!srcport_s) {
      goto done;
    }
    int srcport = atoi(srcport_s) - 1;
    if (!strcasecmp(cmd, "OPEN")) {
      Switch::SetStandard(srcport, Switch::Standard::Open);
    } else if (!strcasecmp(cmd, "SHORT")) {
      Switch::SetStandard(srcport, Switch::Standard::Short);
    } else if (!strcasecmp(cmd, "LOAD")) {
      Switch::SetStandard(srcport, Switch::Standard::Load);
    } else if (!strcasecmp(cmd, "THRU") || !strcasecmp(cmd, "ATT")) {
      // Since we don't have an attenuator, we fake "SL ATT,n,m" with "THRU"
      // by providing the through coefficients for CF_*.
      const char *dstport_s = strtok(NULL, ",\n");
      if (!dstport_s) {
        goto done;
      }
      int dstport = atoi(dstport_s) - 1;
      Switch::SetThrough(srcport, dstport);
    } else {
    	goto done;
    }

  } else if(!strncasecmp((char*) ibuf, "SET:PORT", 8)) {
	  // SNA5000A sends these commands. There is one mandatory argument
	  // which is either OPEN, SHORT, LOAD, THRU or ATT. This is then
	  // followed by a list of ports (by their letter, e.g. 'A' or 'D').
	  // All arguments are comma separated. Example command:
	  // SET:PORT LOAD,A,B

	  // assemble the port list
	  uint8_t ports[4] = {0,0,0,0};
	  uint8_t port_cnt = 0;
	  char *comma = (char*) ibuf;
	  while(comma = strchr(comma, ',')) {
		  ports[port_cnt++] = *++comma - 'A';
	  }

	  // figure out the argument
	  if(!strncasecmp((char*) &ibuf[9], "THRU", 4) || !strncasecmp((char*) &ibuf[9], "ATT", 4)) {
		  // special case, this must always have two ports
		  if(port_cnt != 2) {
			  LOG_ERR("%d ports given for THRU", port_cnt);
			  goto done;
		  }
		  Switch::SetThrough(ports[0], ports[1]);
		  goto done;
	  }
	  // handle the other standards
	  auto s = Switch::Standard::None;
	  if(!strncasecmp((char*) &ibuf[9], "OPEN", 4)) {
		  s = Switch::Standard::Open;
	  } else if(!strncasecmp((char*) &ibuf[9], "SHORT", 5)) {
		  s = Switch::Standard::Short;
	  } else if(!strncasecmp((char*) &ibuf[9], "LOAD", 4)) {
		  s = Switch::Standard::Load;
	  } else {
		  LOG_ERR("Unknown port standard: %s", &ibuf[9]);
		  goto done;
	  }
	  for(uint8_t i=0;i<port_cnt;i++) {
		  Switch::SetStandard(ports[i], s);
	  }
  } else {
	  LOG_ERR("Unknown command: %s", ibuf);
	  goto done;
  }

done:
  tud_usbtmc_start_bus_read();
  return true;
}

extern "C" bool tud_usbtmc_msgBulkIn_complete_cb() {
  tud_usbtmc_start_bus_read();

  return true;
}

extern "C" bool tud_usbtmc_msgBulkIn_request_cb(usbtmc_msg_request_dev_dep_in const * request) {
  size_t txlen = tu_min32(obuf_len - obuf_pos, request->TransferSize);
  if (txlen == 0) {
    return true;
  }
  
  tud_usbtmc_transmit_dev_msg_data(obuf + obuf_pos, txlen, (obuf_pos + txlen) == obuf_len, false);
  obuf_pos += txlen;

  return true;
}

extern "C" void usbtmc_app_task_iter(void) {
}

extern "C" bool tud_usbtmc_initiate_clear_cb(uint8_t *tmcResult) {
  *tmcResult = USBTMC_STATUS_SUCCESS;
  return true;
}

extern "C" bool tud_usbtmc_check_clear_cb(usbtmc_get_clear_status_rsp_t *rsp) {
  ibuf_len = 0;
  obuf_len = 0;
  obuf_pos = 0;
  rsp->USBTMC_status = USBTMC_STATUS_SUCCESS;
  rsp->bmClear.BulkInFifoBytes = 0u;
  return true;
}

extern "C" bool tud_usbtmc_initiate_abort_bulk_in_cb(uint8_t *tmcResult) {
  *tmcResult = USBTMC_STATUS_SUCCESS;
  return true;
}

extern "C" bool tud_usbtmc_check_abort_bulk_in_cb(usbtmc_check_abort_bulk_rsp_t *rsp) {
  (void)rsp;
  tud_usbtmc_start_bus_read();
  return true;
}

extern "C" bool tud_usbtmc_initiate_abort_bulk_out_cb(uint8_t *tmcResult) {
  *tmcResult = USBTMC_STATUS_SUCCESS;
  return true;
}

extern "C" bool tud_usbtmc_check_abort_bulk_out_cb(usbtmc_check_abort_bulk_rsp_t *rsp) {
  (void)rsp;
  tud_usbtmc_start_bus_read();
  return true;
}

extern "C" void tud_usbtmc_bulkIn_clearFeature_cb(void) {
}

extern "C" void tud_usbtmc_bulkOut_clearFeature_cb(void) {
  tud_usbtmc_start_bus_read();
}

extern "C" bool tud_usbtmc_indicator_pulse_cb(tusb_control_request_t const * msg, uint8_t *tmcResult) {
  (void)msg;
  // led_indicator_pulse();
  *tmcResult = USBTMC_STATUS_SUCCESS;
  return true;
}
