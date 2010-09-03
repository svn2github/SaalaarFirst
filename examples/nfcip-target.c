/*-
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
 */

/**
 * @file nfcip-target.c
 * @brief
 */

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif // HAVE_CONFIG_H

#include <err.h>
#include <stdio.h>
#include <nfc/nfc.h>

#define MAX_FRAME_LEN 264

int main(int argc, const char *argv[])
{
  byte_t abtRecv[MAX_FRAME_LEN];
  size_t szRecvBits;
  byte_t send[] = "Hello Mars!";
  nfc_device_t *pnd = nfc_connect(NULL);

  if (argc > 1) {
    errx (1, "usage: %s", argv[0]);
  }

  if (!pnd || !nfc_target_init(pnd, abtRecv, &szRecvBits)) {
    printf("unable to connect or initialize\n");
    return 1;
  }

  if (!nfc_target_receive_bytes(pnd, abtRecv, &szRecvBits)) {
    printf("unable to receive data\n");
    return 1;
  }
  abtRecv[szRecvBits] = 0;
  printf("Received: %s\n", abtRecv);
  printf("Sending : %s\n", send);

  if (!nfc_target_send_bytes(pnd, send, 11)) {
    printf("unable to send data\n");
    return 1;
  }

  nfc_disconnect(pnd);
  return 0;
}
