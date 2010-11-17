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
 * @file uart_posix.c
 * @brief POSIX UART driver
 */

#  include <sys/select.h>
#  include <termios.h>
typedef struct termios term_info;
typedef struct {
  int     fd;                   // Serial port file descriptor
  term_info tiOld;              // Terminal info before using the port
  term_info tiNew;              // Terminal info during the transaction
} serial_port_unix;

// timeval struct that define timeout delay for serial port
static struct timeval default_timeout = {
  .tv_sec = 0,                  // 0 second
  .tv_usec = 60000              // 60 ms
};

// Work-around to claim uart interface using the c_iflag (software input processing) from the termios struct
#  define CCLAIMED 0x80000000

serial_port
uart_open (const char *pcPortName)
{
  serial_port_unix *sp = malloc (sizeof (serial_port_unix));

  if (sp == 0)
    return INVALID_SERIAL_PORT;

  sp->fd = open (pcPortName, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (sp->fd == -1) {
    uart_close (sp);
    return INVALID_SERIAL_PORT;
  }

  if (tcgetattr (sp->fd, &sp->tiOld) == -1) {
    uart_close (sp);
    return INVALID_SERIAL_PORT;
  }
  // Make sure the port is not claimed already
  if (sp->tiOld.c_iflag & CCLAIMED) {
    uart_close (sp);
    return CLAIMED_SERIAL_PORT;
  }
  // Copy the old terminal info struct
  sp->tiNew = sp->tiOld;

  sp->tiNew.c_cflag = CS8 | CLOCAL | CREAD;
  sp->tiNew.c_iflag = CCLAIMED | IGNPAR;
  sp->tiNew.c_oflag = 0;
  sp->tiNew.c_lflag = 0;

  sp->tiNew.c_cc[VMIN] = 0;     // block until n bytes are received
  sp->tiNew.c_cc[VTIME] = 0;    // block until a timer expires (n * 100 mSec.)

  if (tcsetattr (sp->fd, TCSANOW, &sp->tiNew) == -1) {
    uart_close (sp);
    return INVALID_SERIAL_PORT;
  }

  tcflush (sp->fd, TCIFLUSH);
  return sp;
}

// TODO Remove PN53x related timeout
#define UART_TIMEOUT(X) ((X * 7) + 15000)		// where X is 1-byte duration (µs): X * 6+1 bytes (PN53x's ACK + 1 other chance) + 15 ms (PN532 Tmax to reply ACK)
// UART_SPEED_T0_TIME(X) convert baud rate to interval between 2 bytes (in us)
#define UART_SPEED_T0_TIME(X) ((1000000 * 9)/ X)	// 8,n,1 => 9 bits => data rate ~= bauds/9 (bytes/s); ex: 8N1@9600 ~= 1066 bytes/s
void
uart_set_speed (serial_port sp, const uint32_t uiPortSpeed)
{
  long int iTimeout = UART_TIMEOUT(UART_SPEED_T0_TIME(uiPortSpeed)); 
  DBG ("Serial port speed requested to be set to %d bauds (%ld µs).", uiPortSpeed, iTimeout);
  const serial_port_unix *spu = (serial_port_unix *) sp;

  // Portability note: on some systems, B9600 != 9600 so we have to do
  // uint32_t <=> speed_t associations by hand.
  speed_t stPortSpeed = B9600;
  switch (uiPortSpeed) {
  case 9600:
    stPortSpeed = B9600;
    break;
  case 19200:
    stPortSpeed = B19200;
    break;
  case 38400:
    stPortSpeed = B38400;
    break;
#  ifdef B57600
  case 57600:
    stPortSpeed = B57600;
    break;
#  endif
#  ifdef B115200
  case 115200:
    stPortSpeed = B115200;
    break;
#  endif
#  ifdef B230400
  case 230400:
    stPortSpeed = B230400;
    break;
#  endif
#  ifdef B460800
  case 460800:
    stPortSpeed = B460800;
    break;
#  endif
  default:
    ERR ("Unable to set serial port speed to %d bauds. Speed value must be one of those defined in termios(3).",
         uiPortSpeed);
    return;
  };
  // Set default timeout
  default_timeout.tv_usec = iTimeout;

  // Set port speed (Input and Output)
  cfsetispeed ((struct termios *) &(spu->tiNew), stPortSpeed);
  cfsetospeed ((struct termios *) &(spu->tiNew), stPortSpeed);
  if (tcsetattr (spu->fd, TCSADRAIN, &(spu->tiNew)) == -1) {
    ERR ("%s", "Unable to apply new speed settings.");
  }
}

uint32_t
uart_get_speed (serial_port sp)
{
  uint32_t uiPortSpeed = 0;
  const serial_port_unix *spu = (serial_port_unix *) sp;
  switch (cfgetispeed (&spu->tiNew)) {
  case B9600:
    uiPortSpeed = 9600;
    break;
  case B19200:
    uiPortSpeed = 19200;
    break;
  case B38400:
    uiPortSpeed = 38400;
    break;
#  ifdef B57600
  case B57600:
    uiPortSpeed = 57600;
    break;
#  endif
#  ifdef B115200
  case B115200:
    uiPortSpeed = 115200;
    break;
#  endif
#  ifdef B230400
  case B230400:
    uiPortSpeed = 230400;
    break;
#  endif
#  ifdef B460800
  case B460800:
    uiPortSpeed = 460800;
    break;
#  endif
  }

  return uiPortSpeed;
}

void
uart_close (const serial_port sp)
{
  if (((serial_port_unix *) sp)->fd >= 0) {
    tcsetattr (((serial_port_unix *) sp)->fd, TCSANOW, &((serial_port_unix *) sp)->tiOld);
    close (((serial_port_unix *) sp)->fd);
  }
  free (sp);
}

/**
 * @brief Receive data from UART and copy data to \a pbtRx
 *
 * @return 0 on success, otherwise driver error code
 */
int
uart_receive (serial_port sp, byte_t * pbtRx, size_t * pszRx)
{
  int     res;
  int     byteCount;
  fd_set  rfds;
  struct timeval tv;

  // Reset the output count  
  *pszRx = 0;

  do {
    // Reset file descriptor
    FD_ZERO (&rfds);
    FD_SET (((serial_port_unix *) sp)->fd, &rfds);
    tv = default_timeout;
    res = select (((serial_port_unix *) sp)->fd + 1, &rfds, NULL, NULL, &tv);

    // Read error
    if (res < 0) {
      DBG ("%s", "RX error.");
      return DEIO;
    }
    // Read time-out
    if (res == 0) {
      if (*pszRx == 0) {
        // Error, we received no data
        DBG ("RX time-out (%ld µs), buffer empty.", default_timeout.tv_usec);
        return DETIMEOUT;
      } else {
        // We received some data, but nothing more is available
        return 0;
      }
    }
    // Retrieve the count of the incoming bytes
    res = ioctl (((serial_port_unix *) sp)->fd, FIONREAD, &byteCount);
    if (res < 0) {
      return DEIO;
    }
    // There is something available, read the data
    res = read (((serial_port_unix *) sp)->fd, pbtRx + (*pszRx), byteCount);

    // Stop if the OS has some troubles reading the data
    if (res <= 0) {
      return DEIO;
    }

    *pszRx += res;

  } while (byteCount);

  return 0;
}

/**
 * @brief Send \a pbtTx content to UART
 *
 * @return 0 on success, otherwise a driver error is returned
 */
int
uart_send (serial_port sp, const byte_t * pbtTx, const size_t szTx)
{
  int32_t res;
  size_t  szPos = 0;
  fd_set  rfds;
  struct timeval tv;

  while (szPos < szTx) {
    // Reset file descriptor
    FD_ZERO (&rfds);
    FD_SET (((serial_port_unix *) sp)->fd, &rfds);
    tv = default_timeout;
    res = select (((serial_port_unix *) sp)->fd + 1, NULL, &rfds, NULL, &tv);

    // Write error
    if (res < 0) {
      DBG ("%s", "TX error.");
      return DEIO;
    }
    // Write time-out
    if (res == 0) {
      DBG ("%s", "TX time-out.");
      return DETIMEOUT;
    }
    // Send away the bytes
    res = write (((serial_port_unix *) sp)->fd, pbtTx + szPos, szTx - szPos);

    // Stop if the OS has some troubles sending the data
    if (res <= 0) {
      return DEIO;
    }

    szPos += res;
  }
  return 0;
}

