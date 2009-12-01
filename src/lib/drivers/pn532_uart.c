/**
 * Public platform independent Near Field Communication (NFC) library
 * 
 * Copyright (C) 2009, Roel Verdult
 * 
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 * 
 * 
 * @file pn532_uart.c
 * @brief
 */
#define _XOPEN_SOURCE 500
#include <stdio.h>

#include "pn532_uart.h"

#include <nfc/nfc-messages.h>

#include "../drivers.h"
#include "../bitutils.h"

// Bus
#include "uart.h"

#ifdef _WIN32
  #define SERIAL_STRING "COM"
  #define delay_ms( X ) Sleep( X )
#else
  // unistd.h is needed for usleep() fct.
  #include <unistd.h>
  #define delay_ms( X ) usleep( X * 1000 )

  #ifdef __APPLE__
    // MacOS
    #define SERIAL_STRING "/dev/tty.SLAB_USBtoUART"
  #else
    // *BSD, Linux and others POSIX systems
    #define SERIAL_STRING "/dev/ttyUSB"
  #endif
#endif

#define BUFFER_LENGTH 256

#define SERIAL_DEFAULT_PORT_SPEED 115200

nfc_device_t* pn532_uart_connect(const nfc_device_desc_t* pndd)
{
  uint32_t uiDevNr;
  serial_port sp;
  char acConnect[BUFFER_LENGTH];
  nfc_device_t* pnd = NULL;

  if( pndd == NULL ) {
#ifdef DISABLE_SERIAL_AUTOPROBE
    INFO("Sorry, serial auto-probing have been disabled at compile time.");
    return NULL;
#else
    DBG("Trying to find ARYGON device on serial port: %s# at %d bauds.",SERIAL_STRING, SERIAL_DEFAULT_PORT_SPEED);
    // I have no idea how MAC OS X deals with multiple devices, so a quick workaround
    for (uiDevNr=0; uiDevNr<DRIVERS_MAX_DEVICES; uiDevNr++)
    {
#ifdef __APPLE__
      strcpy(acConnect,SERIAL_STRING);
#else
      sprintf(acConnect,"%s%d",SERIAL_STRING,uiDevNr);
#endif /* __APPLE__ */

      sp = uart_open(acConnect);
      if ((sp != INVALID_SERIAL_PORT) && (sp != CLAIMED_SERIAL_PORT))
      {
        uart_set_speed(sp, SERIAL_DEFAULT_PORT_SPEED);
        break;
      }
#ifdef DEBUG
      if (sp == INVALID_SERIAL_PORT) DBG("Invalid serial port: %s",acConnect);
      if (sp == CLAIMED_SERIAL_PORT) DBG("Serial port already claimed: %s",acConnect);
#endif /* DEBUG */
    }
#endif
    // Test if we have found a device
    if (uiDevNr == DRIVERS_MAX_DEVICES) return NULL;
  } else {
    DBG("Connecting to: %s at %d bauds.",pndd->pcPort, pndd->uiSpeed);
    strcpy(acConnect,pndd->pcPort);
    sp = uart_open(acConnect);
    if (sp == INVALID_SERIAL_PORT) ERR("Invalid serial port: %s",acConnect);
    if (sp == CLAIMED_SERIAL_PORT) ERR("Serial port already claimed: %s",acConnect);
    if ((sp == CLAIMED_SERIAL_PORT) || (sp == INVALID_SERIAL_PORT)) return NULL;

    uart_set_speed(sp, pndd->uiSpeed);
  }
  /** @info PN532C106 wakeup. */
  /** @todo Put this command in pn53x init process */
  byte_t abtRxBuf[BUFFER_LENGTH];
  size_t szRxBufLen;
  const byte_t pncmd_pn532c106_wakeup[] = { 0x55,0x55,0x00,0x00,0x00,0x00,0x00,0xFF,0x03,0xFD,0xD4,0x14,0x01,0x17,0x00 };

  uart_send(sp, pncmd_pn532c106_wakeup, sizeof(pncmd_pn532c106_wakeup));
  delay_ms(10);

  if (!uart_receive(sp,abtRxBuf,&szRxBufLen)) {
    ERR("%s", "Unable to receive data. (RX)");
    return NULL;
  }
#ifdef DEBUG
  printf(" RX: ");
  print_hex(abtRxBuf,szRxBufLen);
#endif

  DBG("Successfully connected to: %s",acConnect);

  // We have a connection
  pnd = malloc(sizeof(nfc_device_t));
  strcpy(pnd->acName,"PN532_UART");
  pnd->nc = NC_PN532;
  pnd->nds = (nfc_device_spec_t)sp;
  pnd->bActive = true;
  pnd->bCrc = true;
  pnd->bPar = true;
  pnd->ui8TxBits = 0;
  return pnd;
}

void pn532_uart_disconnect(nfc_device_t* pnd)
{
  uart_close((serial_port)pnd->nds);
  free(pnd);
}

bool pn532_uart_transceive(const nfc_device_spec_t nds, const byte_t* pbtTx, const size_t szTxLen, byte_t* pbtRx, size_t* pszRxLen)
{
  byte_t abtTxBuf[BUFFER_LENGTH] = { 0x00, 0x00, 0xff }; // Every packet must start with "00 00 ff"
  byte_t abtRxBuf[BUFFER_LENGTH];
  size_t szRxBufLen = BUFFER_LENGTH;
  size_t szPos;

  // Packet length = data length (len) + checksum (1) + end of stream marker (1)
  abtTxBuf[3] = szTxLen;
  // Packet length checksum
  abtTxBuf[4] = BUFFER_LENGTH - abtTxBuf[3];
  // Copy the PN53X command into the packet buffer
  memmove(abtTxBuf+5,pbtTx,szTxLen);

  // Calculate data payload checksum
  abtTxBuf[szTxLen+5] = 0;
  for(szPos=0; szPos < szTxLen; szPos++) 
  {
    abtTxBuf[szTxLen+5] -= abtTxBuf[szPos+5];
  }

  // End of stream marker
  abtTxBuf[szTxLen+6] = 0;

#ifdef DEBUG
  printf(" TX: ");
  print_hex(abtTxBuf,szTxLen+7);
#endif
  if (!uart_send((serial_port)nds,abtTxBuf,szTxLen+7)) {
    ERR("%s", "Unable to transmit data. (TX)");
    return false;
  }

  /** @note PN532 (at 115200 bauds) need 20ms between sending and receiving frame.
   * It seems to be a required delay to able to send from host to device, plus the device computation then device respond transmission 
   */
  delay_ms(20);

  /** @note PN532 (at 115200 bauds) need 30ms more to be stable (report correctly present tag, at each try: 20ms seems to be enought for one shot...)
   * PN532 seems to work correctly with 50ms at 115200 bauds.
   */
  delay_ms(30);

  if (!uart_receive((serial_port)nds,abtRxBuf,&szRxBufLen)) {
    ERR("%s", "Unable to receive data. (RX)");
    return false;
  }

#ifdef DEBUG
  printf(" RX: ");
  print_hex(abtRxBuf,szRxBufLen);
#endif

  // When the answer should be ignored, just return a successful result
  if(pbtRx == NULL || pszRxLen == NULL) return true;

  // Only succeed when the result is at least 00 00 ff 00 ff 00 00 00 FF xx Fx Dx xx .. .. .. xx 00 (x = variable)
  if(szRxBufLen < 15) return false;

  // Remove the preceding and appending bytes 00 00 ff 00 ff 00 00 00 FF xx Fx .. .. .. xx 00 (x = variable)
  *pszRxLen = szRxBufLen - 15;
  memcpy(pbtRx, abtRxBuf+13, *pszRxLen);

  return true;
}
