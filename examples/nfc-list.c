/*-
 * Public platform independent Near Field Communication (NFC) library
 * 
 * Copyright (C) 2009, 2O1O, Roel Verdult, Romuald Conty
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
 */

/**
 * @file nfc-list.c
 * @brief Lists the first target present of each founded device
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif // HAVE_CONFIG_H

#ifdef HAVE_LIBUSB
#  ifdef DEBUG
#    include <sys/param.h>
#    include <usb.h>
#  endif
#endif

#include <err.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <nfc/nfc.h>
#include <nfc/nfc-messages.h>
#include "nfc-utils.h"

#define MAX_DEVICE_COUNT 16
#define MAX_TARGET_COUNT 16

static nfc_device_t *pnd;

int
main (int argc, const char *argv[])
{
  const char *acLibnfcVersion;
  size_t  szDeviceFound;
  size_t  szTargetFound;
  size_t  i;
  bool verbose = false;
  nfc_device_desc_t *pnddDevices;

  // Display libnfc version
  acLibnfcVersion = nfc_version ();
  printf ("%s use libnfc %s\n", argv[0], acLibnfcVersion);

  pnddDevices = parse_args (argc, argv, &szDeviceFound, &verbose);
#ifdef HAVE_LIBUSB
#  ifdef DEBUG
  usb_set_debug (4);
#  endif
#endif

  /* Lazy way to open an NFC device */
#if 0
  pnd = nfc_connect (NULL);
#endif

  /* If specific device is wanted, i.e. an ARYGON device on /dev/ttyUSB0 */
#if 0
  nfc_device_desc_t ndd;
  ndd.pcDriver = "ARYGON";
  ndd.pcPort = "/dev/ttyUSB0";
  ndd.uiSpeed = 115200;
  pnd = nfc_connect (&ndd);
#endif

  /* If specific device is wanted, i.e. a SCL3711 on USB */
#if 0
  nfc_device_desc_t ndd;
  ndd.pcDriver = "PN533_USB";
  strcpy(ndd.acDevice, "SCM Micro / SCL3711-NFC&RW");
  pnd = nfc_connect (&ndd);
#endif

  if (szDeviceFound == 0) {
    if (!(pnddDevices = malloc (MAX_DEVICE_COUNT * sizeof (*pnddDevices)))) {
      fprintf (stderr, "malloc() failed\n");
      return EXIT_FAILURE;
    }

    nfc_list_devices (pnddDevices, MAX_DEVICE_COUNT, &szDeviceFound);
  }

  if (szDeviceFound == 0) {
    printf ("No NFC device found.\n");
  }

  for (i = 0; i < szDeviceFound; i++) {
    nfc_target_t ant[MAX_TARGET_COUNT];
    pnd = nfc_connect (&(pnddDevices[i]));

    if (pnd == NULL) {
      ERR ("%s", "Unable to connect to NFC device.");
      return EXIT_FAILURE;
    }
    nfc_initiator_init (pnd);

    printf ("Connected to NFC device: %s\n", pnd->acName);

    // List ISO14443A targets
    nfc_modulation_t nm = {
      .nmt = NMT_ISO14443A,
      .nbr = NBR_106,
    };
    if (nfc_initiator_list_passive_targets (pnd, nm, ant, MAX_TARGET_COUNT, &szTargetFound)) {
      size_t  n;
      if (verbose || (szTargetFound > 0)) {
        printf ("%d ISO14443A passive target(s) was found%s\n", (int) szTargetFound, (szTargetFound == 0) ? ".\n" : ":");
      }
      for (n = 0; n < szTargetFound; n++) {
        print_nfc_iso14443a_info (ant[n].nti.nai, verbose);
        printf ("\n");
      }
    }

    nm.nmt = NMT_FELICA;
    nm.nbr = NBR_212;
    // List Felica tags
    if (nfc_initiator_list_passive_targets (pnd, nm, ant, MAX_TARGET_COUNT, &szTargetFound)) {
      size_t  n;
      if (verbose || (szTargetFound > 0)) {
        printf ("%d Felica (212 kbps) passive target(s) was found%s\n", (int) szTargetFound,
                (szTargetFound == 0) ? ".\n" : ":");
      }
      for (n = 0; n < szTargetFound; n++) {
        print_nfc_felica_info (ant[n].nti.nfi, verbose);
        printf ("\n");
      }
    }

    nm.nbr = NBR_424;
    if (nfc_initiator_list_passive_targets (pnd, nm, ant, MAX_TARGET_COUNT, &szTargetFound)) {
      size_t  n;
      if (verbose || (szTargetFound > 0)) {
        printf ("%d Felica (424 kbps) passive target(s) was found%s\n", (int) szTargetFound,
                (szTargetFound == 0) ? ".\n" : ":");
      }
      for (n = 0; n < szTargetFound; n++) {
        print_nfc_felica_info (ant[n].nti.nfi, verbose);
        printf ("\n");
      }
    }

    nm.nmt = NMT_ISO14443B;
    nm.nbr = NBR_106;
    // List ISO14443B targets
    if (nfc_initiator_list_passive_targets (pnd, nm, ant, MAX_TARGET_COUNT, &szTargetFound)) {
      size_t  n;
      if (verbose || (szTargetFound > 0)) {
        printf ("%d ISO14443B passive target(s) was found%s\n", (int) szTargetFound, (szTargetFound == 0) ? ".\n" : ":");
      }
      for (n = 0; n < szTargetFound; n++) {
        print_nfc_iso14443b_info (ant[n].nti.nbi, verbose);
        printf ("\n");
      }
    }

    nm.nmt = NMT_JEWEL;
    nm.nbr = NBR_106;
    // List Jewel targets
    if (nfc_initiator_list_passive_targets(pnd, nm, ant, MAX_TARGET_COUNT, &szTargetFound )) {
      size_t n;
      if (verbose || (szTargetFound > 0)) {
        printf("%d Jewel passive target(s) was found%s\n", (int)szTargetFound, (szTargetFound==0)?".\n":":");
      }
      for(n=0; n<szTargetFound; n++) {
        print_nfc_jewel_info (ant[n].nti.nji, verbose);
        printf("\n");
      }
    }
    nfc_disconnect (pnd);
  }

  free (pnddDevices);
  return 0;
}
