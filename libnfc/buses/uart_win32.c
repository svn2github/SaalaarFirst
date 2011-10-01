/*-
 * Public platform independent Near Field Communication (NFC) library
 * 
 * Copyright (C) 2009, 2010, Roel Verdult, Romuald Conty
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
 */

/**
 * @file uart_win32.c
 * @brief Windows UART driver
 */

#include "log.h"

#define LOG_CATEGORY "libnfc.bus.uart_win32"

// Handle platform specific includes
#include "contrib/windows.h"
#define delay_ms( X ) Sleep( X )

typedef struct {
  HANDLE  hPort;                // Serial port handle
  DCB     dcb;                  // Device control settings
  COMMTIMEOUTS ct;              // Serial port time-out configuration
} serial_port_windows;

serial_port
uart_open (const char *pcPortName)
{
  char    acPortName[255];
  serial_port_windows *sp = malloc (sizeof (serial_port_windows));

  // Copy the input "com?" to "\\.\COM?" format
  sprintf (acPortName, "\\\\.\\%s", pcPortName);
  _strupr (acPortName);

  // Try to open the serial port
  sp->hPort = CreateFileA (acPortName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
  if (sp->hPort == INVALID_HANDLE_VALUE) {
    uart_close (sp);
    return INVALID_SERIAL_PORT;
  }
  // Prepare the device control
  memset (&sp->dcb, 0, sizeof (DCB));
  sp->dcb.DCBlength = sizeof (DCB);
  if (!BuildCommDCBA ("baud=9600 data=8 parity=N stop=1", &sp->dcb)) {
    uart_close (sp);
    return INVALID_SERIAL_PORT;
  }
  // Update the active serial port
  if (!SetCommState (sp->hPort, &sp->dcb)) {
    uart_close (sp);
    return INVALID_SERIAL_PORT;
  }

  sp->ct.ReadIntervalTimeout = 30;
  sp->ct.ReadTotalTimeoutMultiplier = 0;
  sp->ct.ReadTotalTimeoutConstant = 30;
  sp->ct.WriteTotalTimeoutMultiplier = 30;
  sp->ct.WriteTotalTimeoutConstant = 0;

  if (!SetCommTimeouts (sp->hPort, &sp->ct)) {
    uart_close (sp);
    return INVALID_SERIAL_PORT;
  }

  PurgeComm (sp->hPort, PURGE_RXABORT | PURGE_RXCLEAR);

  return sp;
}

void
uart_close (const serial_port sp)
{
  if (((serial_port_windows *) sp)->hPort != INVALID_HANDLE_VALUE) {
    CloseHandle (((serial_port_windows *) sp)->hPort);
  }
  free (sp);
}

void
uart_flush_input (const serial_port sp)
{
  PurgeComm(((serial_port_windows *) sp)->hPort, PURGE_RXABORT | PURGE_RXCLEAR);
}

void
uart_set_speed (serial_port sp, const uint32_t uiPortSpeed)
{
  serial_port_windows *spw;

  log_put (LOG_CATEGORY, NFC_PRIORITY_TRACE, "Serial port speed requested to be set to %d bauds.", uiPortSpeed);
  // Set port speed (Input and Output)
  switch (uiPortSpeed) {
  case 9600:
  case 19200:
  case 38400:
  case 57600:
  case 115200:
  case 230400:
  case 460800:
    break;
  default:
    log_put (LOG_CATEGORY, NFC_PRIORITY_ERROR, "Unable to set serial port speed to %d bauds. Speed value must be one of these constants: 9600 (default), 19200, 38400, 57600, 115200, 230400 or 460800.", uiPortSpeed);
    return;
  };
  spw = (serial_port_windows *) sp;

  // Set baud rate
  spw->dcb.BaudRate = uiPortSpeed;
  if (!SetCommState (spw->hPort, &spw->dcb)) {
    log_put (LOG_CATEGORY, NFC_PRIORITY_ERROR, "%s", "Unable to apply new speed settings.");
    return;
  }
  PurgeComm (spw->hPort, PURGE_RXABORT | PURGE_RXCLEAR);
}

uint32_t
uart_get_speed (const serial_port sp)
{
  const serial_port_windows *spw = (serial_port_windows *) sp;
  if (!GetCommState (spw->hPort, (serial_port) & spw->dcb))
    return spw->dcb.BaudRate;

  return 0;
}

int
uart_receive (serial_port sp, byte_t * pbtRx, const size_t szRx, void * abort_p, struct timeval *timeout)
{
  DWORD dwBytesToGet = (DWORD)szRx;
  DWORD dwBytesReceived = 0;
  DWORD dwTotalBytesReceived = 0;
  BOOL res;

  // XXX Put this part into uart_win32_timeouts () ?
  COMMTIMEOUTS timeouts;
  timeouts.ReadIntervalTimeout = 0;
  timeouts.ReadTotalTimeoutMultiplier = 0;
  timeouts.ReadTotalTimeoutConstant = timeout ? ((timeout->tv_sec * 1000) + (timeout->tv_usec / 1000)) : 0;
  timeouts.WriteTotalTimeoutMultiplier = 0;
  timeouts.WriteTotalTimeoutConstant = timeout ? ((timeout->tv_sec * 1000) + (timeout->tv_usec / 1000)) : 0;

  if (!SetCommTimeouts (((serial_port_windows *) sp)->hPort, &timeouts)) {
    log_put (LOG_CATEGORY, NFC_PRIORITY_ERROR, "Unable to apply new timeout settings.");
    return ECOMIO;
  }

  // TODO Enhance the reception method
  // - According to MSDN, it could be better to implement nfc_abort_command() mecanism using Cancello()
  // - ECOMTIMEOUT should be return is case of timeout (as of uart_posix implementation)
  volatile bool * abort_flag_p = (volatile bool *)abort_p;
  do {
    res = ReadFile (((serial_port_windows *) sp)->hPort, pbtRx + dwTotalBytesReceived,
      dwBytesToGet, 
      &dwBytesReceived, NULL);

    dwTotalBytesReceived += dwBytesReceived;

    if (!res) {
      DWORD err = GetLastError();
      log_put (LOG_CATEGORY, NFC_PRIORITY_ERROR, "ReadFile error: %u", err);
      return ECOMIO;
    }
    if (((DWORD)szRx) > dwTotalBytesReceived) {
      dwBytesToGet -= dwBytesReceived;
    }

    if (abort_flag_p != NULL && (*abort_flag_p) && dwTotalBytesReceived == 0) {
      return EOPABORT;
    }
  } while (((DWORD)szRx) > dwTotalBytesReceived);

  return (dwTotalBytesReceived == (DWORD) szRx) ? 0 : ECOMIO;
}

int
uart_send (serial_port sp, const byte_t * pbtTx, const size_t szTx, struct timeval *timeout)
{
  DWORD   dwTxLen = 0;

  COMMTIMEOUTS timeouts;
  timeouts.ReadIntervalTimeout = 0;
  timeouts.ReadTotalTimeoutMultiplier = 0;
  timeouts.ReadTotalTimeoutConstant = timeout ? ((timeout->tv_sec * 1000) + (timeout->tv_usec / 1000)) : 0;
  timeouts.WriteTotalTimeoutMultiplier = 0;
  timeouts.WriteTotalTimeoutConstant = timeout ? ((timeout->tv_sec * 1000) + (timeout->tv_usec / 1000)) : 0;

  if (!SetCommTimeouts (((serial_port_windows *) sp)->hPort, &timeouts)) {
    log_put (LOG_CATEGORY, NFC_PRIORITY_ERROR, "Unable to apply new timeout settings.");
    return ECOMIO;
  }

  if (!WriteFile (((serial_port_windows *) sp)->hPort, pbtTx, szTx, &dwTxLen, NULL)) {
    return ECOMIO;
  }
  if (!dwTxLen)
    return ECOMIO;
  return 0;
}

BOOL is_port_available(int nPort)
{
  TCHAR szPort[15];
  COMMCONFIG cc;
  DWORD dwCCSize;

  sprintf(szPort, "COM%d", nPort);

  // Check if this port is available
  dwCCSize = sizeof(cc);
  return GetDefaultCommConfig(szPort, &cc, &dwCCSize);
}

// Path to the serial port is OS-dependant.
// Try to guess what we should use.
#define MAX_SERIAL_PORT_WIN 255
char **
uart_list_ports (void)
{
  char ** availablePorts = malloc((1 + MAX_SERIAL_PORT_WIN) * sizeof(char*));
  int curIndex = 0;
  int i;
  for (i = 1; i <= MAX_SERIAL_PORT_WIN; i++) {
    if (is_port_available(i)) {
      availablePorts[curIndex] = (char*)malloc(10);
      sprintf(availablePorts[curIndex], "COM%d", i);
      // printf("found candidate port: %s\n", availablePorts[curIndex]);
      curIndex++;
    }
  }
  availablePorts[curIndex] = NULL;

  return availablePorts;
}
