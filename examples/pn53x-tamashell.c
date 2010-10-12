/*
 
Public platform independent Near Field Communication (NFC) library
Copyright (C) 2010, Yobibe
 
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif // HAVE_CONFIG_H

#if defined(HAVE_READLINE)
#  include <stdio.h>
#  include <readline/readline.h>
#  include <readline/history.h>
#else
#  define _GNU_SOURCE // for getline on system with glibc < 2.10
#  define _POSIX_C_SOURCE 200809L // for getline on system with glibc >= 2.10
#  include <stdio.h>
   extern FILE* stdin;
#endif //HAVE_READLINE

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include <nfc/nfc.h>
#include <nfc/nfc-messages.h>

#include "nfc-utils.h"

#include "chips/pn53x.h"

#define MAX_FRAME_LEN 264

int main(int argc, const char* argv[])
{
  nfc_device_t* pnd;
  byte_t abtRx[MAX_FRAME_LEN];
  byte_t abtTx[MAX_FRAME_LEN] = { 0xD4 };
  size_t szRx;
  size_t szTx;

  // Try to open the NFC reader
  pnd = nfc_connect(NULL);

  if (pnd == NULL) {
    ERR ("%s", "Unable to connect to NFC device.");
    return EXIT_FAILURE;
  }

  printf ("Connected to NFC reader: %s\n", pnd->acName);

  nfc_initiator_init(pnd);

  char * cmd;
  char * prompt="> ";
  while(1) {
    int offset=0;
#if defined(HAVE_READLINE)
    cmd=readline(prompt);
    // NULL if ctrl-d
    if (cmd==NULL) {
      printf("Bye!\n");
      break;
    }
    add_history(cmd);
#else
    cmd = NULL;
    printf("%s", prompt);
    fflush(0);
    size_t n;
    extern FILE* stdin;
    int s = getline(&cmd, &n, stdin);
    if (s <= 0) {
      printf("Bye!\n");
      free(cmd);
      break;
    }
    // FIXME print only if read from redirected stdin (i.e. script)
    printf("%s", cmd);
#endif //HAVE_READLINE
    if (cmd[0]=='q') {
      printf("Bye!\n");
      free(cmd);
      break;
    }
    szTx = 0;
    for(int i = 0; i<MAX_FRAME_LEN-10; i++) {
      int size;
      byte_t byte;
      while (isspace(cmd[offset])) {
        offset++;
      }
      size = sscanf(cmd+offset, "%2x", (unsigned int*)&byte);
      if (size<1) {
        break;
      }
      abtTx[i+1] = byte;
      szTx++;
      if (cmd[offset+1] == 0) { // if last hex was only 1 symbol
        break;
      }
      offset += 2;
    }

    if ((int)szTx < 1) {
      free(cmd);
      continue;
    }
    szTx++;
    printf("Tx: ");
    print_hex((byte_t*)abtTx+1,szTx-1);

    if (!pn53x_transceive (pnd, abtTx, szTx, abtRx, &szRx)) {
      free(cmd);
      nfc_perror (pnd, "Rx");
      continue;
    }

    printf("Rx: ");
    print_hex(abtRx, szRx);
    free(cmd);
  }

  nfc_disconnect(pnd);
  return 1;
}
