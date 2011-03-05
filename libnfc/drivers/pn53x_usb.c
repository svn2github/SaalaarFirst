/*-
 * Public platform independent Near Field Communication (NFC) library
 * 
 * Copyright (C) 2009, Roel Verdult
 * Copyright (C) 2010, Romain Tartière, Romuald Conty
 * Copyright (C) 2011, Romain Tartière, Romuald Conty
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
 * @file pn53x_usb.c
 * @brief Driver common routines for PN53x chips using USB
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif // HAVE_CONFIG_H

/*
Thanks to d18c7db and Okko for example code
*/

#include <stdio.h>
#include <stdlib.h>
#include <usb.h>
#include <string.h>

#include <nfc/nfc.h>

#include "nfc-internal.h"
#include "chips/pn53x.h"
#include "chips/pn53x-internal.h"
#include "drivers/pn53x_usb.h"

#define PN53X_USB_DRIVER_NAME "PN53x USB"
#define USB_TIMEOUT   0

#define CHIP_DATA(pnd) ((struct pn53x_data*)(pnd->chip_data))
#define DRIVER_DATA(pnd) ((struct pn53x_usb_data*)(pnd->driver_data))

typedef enum {
  UNKNOWN,
  NXP_PN531,
  NXP_PN533,
  ASK_LOGO,
  SCM_SCL3711
} pn53x_usb_model;

struct pn53x_usb_data {
  usb_dev_handle *pudh;
  pn53x_usb_model model;
  uint32_t uiEndPointIn;
  uint32_t uiEndPointOut;
  uint32_t uiMaxPacketSize;
};

const struct pn53x_io pn53x_usb_io;
bool pn53x_usb_get_usb_device_name (struct usb_device *dev, usb_dev_handle *udev, char *buffer, size_t len);

int
pn53x_usb_bulk_read (struct pn53x_usb_data *data, byte_t abtRx[], const size_t szRx)
{
  int ret = usb_bulk_read (data->pudh, data->uiEndPointIn, (char *) abtRx, szRx, USB_TIMEOUT);
  PRINT_HEX ("RX", abtRx, ret);
  return ret;
}

int
pn53x_usb_bulk_write (struct pn53x_usb_data *data, byte_t abtTx[], const size_t szTx)
{
  PRINT_HEX ("TX", abtTx, szTx);
  return usb_bulk_write (data->pudh, data->uiEndPointOut, (char *) abtTx, szTx, USB_TIMEOUT);
}

struct pn53x_usb_supported_device {
  uint16_t vendor_id;
  uint16_t product_id;
  pn53x_usb_model model;
  const char *name;
};

const struct pn53x_usb_supported_device pn53x_usb_supported_devices[] = {
  { 0x04CC, 0x0531, NXP_PN531,   "Philips / USB TAMA" },
  { 0x04CC, 0x2533, NXP_PN533,   "NXP / PN533" },
  { 0x04E6, 0x5591, SCM_SCL3711, "SCM Micro / SCL3711-NFC&RW" },
  { 0x054c, 0x0193, NXP_PN531,   "TOPPAN FORMS ???? / TN31CUD001SW ????" },
  { 0x1FD3, 0x0608, ASK_LOGO,    "ASK / LoGO" }
};

pn53x_usb_model
pn53x_usb_get_device_model (uint16_t vendor_id, uint16_t product_id)
{
  for (size_t n = 0; n < sizeof (pn53x_usb_supported_devices) / sizeof (struct pn53x_usb_supported_device); n++) {
    if ((vendor_id == pn53x_usb_supported_devices[n].vendor_id) &&
	(product_id == pn53x_usb_supported_devices[n].product_id))
      return pn53x_usb_supported_devices[n].model;
  }
  
  return UNKNOWN;
}

// TODO Move this HACK1 into an upper level in order to benefit to other devices that use PN53x
static const byte_t ack_frame[] = { 0x00, 0x00, 0xff, 0x00, 0xff, 0x00 };

int  pn53x_usb_ack (nfc_device_t * pnd);

// Find transfer endpoints for bulk transfers
void
pn53x_usb_get_end_points (struct usb_device *dev, struct pn53x_usb_data *data)
{
  uint32_t uiIndex;
  uint32_t uiEndPoint;
  struct usb_interface_descriptor *puid = dev->config->interface->altsetting;

  // 3 Endpoints maximum: Interrupt In, Bulk In, Bulk Out
  for (uiIndex = 0; uiIndex < puid->bNumEndpoints; uiIndex++) {
    // Only accept bulk transfer endpoints (ignore interrupt endpoints)
    if (puid->endpoint[uiIndex].bmAttributes != USB_ENDPOINT_TYPE_BULK)
      continue;

    // Copy the endpoint to a local var, makes it more readable code
    uiEndPoint = puid->endpoint[uiIndex].bEndpointAddress;

    // Test if we dealing with a bulk IN endpoint
    if ((uiEndPoint & USB_ENDPOINT_DIR_MASK) == USB_ENDPOINT_IN) {
      data->uiEndPointIn = uiEndPoint;
      data->uiMaxPacketSize = puid->endpoint[uiIndex].wMaxPacketSize;
    }
    // Test if we dealing with a bulk OUT endpoint
    if ((uiEndPoint & USB_ENDPOINT_DIR_MASK) == USB_ENDPOINT_OUT) {
      data->uiEndPointOut = uiEndPoint;
      data->uiMaxPacketSize = puid->endpoint[uiIndex].wMaxPacketSize;
    }
  }
}

bool
pn53x_usb_probe (nfc_device_desc_t pnddDevices[], size_t szDevices, size_t * pszDeviceFound)
{
  usb_init ();

  int res;
  // usb_find_busses will find all of the busses on the system. Returns the number of changes since previous call to this function (total of new busses and busses removed).
  if ((res = usb_find_busses () < 0))
    return false;
  // usb_find_devices will find all of the devices on each bus. This should be called after usb_find_busses. Returns the number of changes since the previous call to this function (total of new device and devices removed).
  if ((res = usb_find_devices () < 0))
    return false;

  *pszDeviceFound = 0;

  uint32_t uiBusIndex = 0;
  struct usb_bus *bus;
  for (bus = usb_get_busses (); bus; bus = bus->next) {
    struct usb_device *dev;
    for (dev = bus->devices; dev; dev = dev->next, uiBusIndex++) {
      for (size_t n = 0; n < sizeof (pn53x_usb_supported_devices) / sizeof (struct pn53x_usb_supported_device); n++) {
        // DBG("Checking device %04x:%04x (%04x:%04x)",dev->descriptor.idVendor,dev->descriptor.idProduct,candidates[i].idVendor,candidates[i].idProduct);
        if ((pn53x_usb_supported_devices[n].vendor_id == dev->descriptor.idVendor) &&
            (pn53x_usb_supported_devices[n].product_id == dev->descriptor.idProduct)) {
          // Make sure there are 2 endpoints available
          // with libusb-win32 we got some null pointers so be robust before looking at endpoints:
          if (dev->config == NULL || dev->config->interface == NULL || dev->config->interface->altsetting == NULL) {
            // Nope, we maybe want the next one, let's try to find another
            continue;
          }
          if (dev->config->interface->altsetting->bNumEndpoints < 2) {
            // Nope, we maybe want the next one, let's try to find another
            continue;
          }

          usb_dev_handle *udev = usb_open (dev);
          pn53x_usb_get_usb_device_name (dev, udev, pnddDevices[*pszDeviceFound].acDevice, sizeof (pnddDevices[*pszDeviceFound].acDevice));
          usb_close (udev);
          pnddDevices[*pszDeviceFound].pcDriver = PN53X_USB_DRIVER_NAME;
          pnddDevices[*pszDeviceFound].uiBusIndex = uiBusIndex;
          (*pszDeviceFound)++;
          // Test if we reach the maximum "wanted" devices
          if ((*pszDeviceFound) == szDevices) {
            return true;
          }
        }
      }
    }
  }
  if (*pszDeviceFound)
    return true;
  return false;
}

bool
pn53x_usb_get_usb_device_name (struct usb_device *dev, usb_dev_handle *udev, char *buffer, size_t len)
{
  *buffer = '\0';

  if (dev->descriptor.iManufacturer || dev->descriptor.iProduct) {
    if (udev) {
      usb_get_string_simple (udev, dev->descriptor.iManufacturer, buffer, len);
      if (strlen (buffer) > 0)
        strcpy (buffer + strlen (buffer), " / ");
      usb_get_string_simple (udev, dev->descriptor.iProduct, buffer + strlen (buffer), len - strlen (buffer));
    }
  }

  if (!*buffer) {
    for (size_t n = 0; n < sizeof (pn53x_usb_supported_devices) / sizeof (struct pn53x_usb_supported_device); n++) {
      if ((pn53x_usb_supported_devices[n].vendor_id == dev->descriptor.idVendor) &&
          (pn53x_usb_supported_devices[n].product_id == dev->descriptor.idProduct)) {
        strncpy (buffer, pn53x_usb_supported_devices[n].name, len);
        return true;
      }
    }
  }

  return false;
}

nfc_device_t *
pn53x_usb_connect (const nfc_device_desc_t *pndd)
{
  nfc_device_t *pnd = NULL;
  struct pn53x_usb_data data = {
    .pudh = NULL,
    .uiEndPointIn = 0,
    .uiEndPointOut = 0,
  };
  struct usb_bus *bus;
  struct usb_device *dev;
  uint32_t uiBusIndex;

  usb_init ();

  uiBusIndex = pndd->uiBusIndex;

  for (bus = usb_get_busses (); bus; bus = bus->next) {
    for (dev = bus->devices; dev; dev = dev->next, uiBusIndex--) {
      DBG ("Checking device %04x:%04x", dev->descriptor.idVendor, dev->descriptor.idProduct);
      if (uiBusIndex == 0) {
        // Open the USB device
        data.pudh = usb_open (dev);

        pn53x_usb_get_end_points (dev, &data);
        if (usb_set_configuration (data.pudh, 1) < 0) {
          ERR ("Unable to set USB configuration, please check USB permissions for device %04x:%04x", dev->descriptor.idVendor, dev->descriptor.idProduct);
          usb_close (data.pudh);
          // we failed to use the specified device
          return NULL;
        }

        if (usb_claim_interface (data.pudh, 0) < 0) {
          DBG ("%s", "Can't claim interface");
          usb_close (data.pudh);
          // we failed to use the specified device
          return NULL;
        }
        data.model = pn53x_usb_get_device_model (dev->descriptor.idVendor, dev->descriptor.idProduct);
        // Allocate memory for the device info and specification, fill it and return the info
        pnd = malloc (sizeof (nfc_device_t));
        pn53x_usb_get_usb_device_name (dev, data.pudh, pnd->acName, sizeof (pnd->acName));

        pnd->driver_data = malloc(sizeof(struct pn53x_usb_data));
        *DRIVER_DATA (pnd) = data;
        pnd->chip_data = malloc(sizeof(struct pn53x_data));

        CHIP_DATA (pnd)->state = NORMAL;
        CHIP_DATA (pnd)->io = &pn53x_usb_io;
        pnd->driver = &pn53x_usb_driver;

        // HACK1: Send first an ACK as Abort command, to reset chip before talking to it:
        pn53x_usb_ack (pnd);

        // HACK2: Then send a GetFirmware command to resync USB toggle bit between host & device
        // in case host used set_configuration and expects the device to have reset its toggle bit, which PN53x doesn't do
#if 1
        if (!pn53x_init (pnd)) {
          usb_close (data.pudh);
          goto error;
        }
#else
        byte_t  abtTx[] = { 0x00, 0x00, 0xff, 0x02, 0xfe, 0xd4, 0x02, 0x2a, 0x00 };
        byte_t  abtRx[BUFFER_LENGTH];
        int ret;

        ret = pn53x_usb_bulk_write (data, abtTx, sizeof(abtTx));
        if (ret < 0) {
          DBG ("usb_bulk_write failed with error %d", ret);
          usb_close (data.pudh);
          // we failed to use the specified device
          goto error;
        }
        ret = pn53x_usb_bulk_read (data, (char *) abtRx, s);
        if (ret < 0) {
          DBG ("usb_bulk_read failed with error %d", ret);
          usb_close (us.pudh);
          // we failed to use the specified device
          goto error;
        }
        if (ret == 6) { // we got the ACK/NACK properly
          if (!pn53x_check_ack_frame (pnd, abtRx, ret)) {
            DBG ("pn53x_check_ack_frame failed");
            usb_close (us.pudh);
            // we failed to use the specified device
            goto error;
          }
          ret = pn53x_usb_bulk_read (data, (char *) abtRx, BUFFER_LENGTH);
          if (ret < 0) {
            DBG ("usb_bulk_read failed with error %d", ret);
            usb_close (us.pudh);
            // we failed to use the specified device
            goto error;
          }
        }
#endif
        return pnd;
      }
    }
  }
  // We ran out of devices before the index required
  return NULL;

error:
  // Free allocated structure on error.
  free(pnd->driver_data);
  free(pnd->chip_data);
  free(pnd);
  return NULL;
}

void
pn53x_usb_disconnect (nfc_device_t * pnd)
{
  int     res;

  pn53x_usb_ack (pnd);

  if ((res = usb_release_interface (DRIVER_DATA (pnd)->pudh, 0)) < 0) {
    ERR ("usb_release_interface failed (%i)", res);
  }

  if ((res = usb_close (DRIVER_DATA (pnd)->pudh)) < 0) {
    ERR ("usb_close failed (%i)", res);
  }
  free(pnd->driver_data);
  free(pnd->chip_data);
  free(pnd);
}

#define PN53X_USB_BUFFER_LEN (PN53x_EXTENDED_FRAME__DATA_MAX_LEN + PN53x_EXTENDED_FRAME__OVERHEAD)

bool
pn53x_usb_send (nfc_device_t * pnd, const byte_t * pbtData, const size_t szData)
{
  byte_t  abtFrame[PN53X_USB_BUFFER_LEN] = { 0x00, 0x00, 0xff };  // Every packet must start with "00 00 ff"
  pnd->iLastCommand = pbtData[0];
  size_t szFrame = 0;

  pn53x_build_frame (abtFrame, &szFrame, pbtData, szData);

  int res = pn53x_usb_bulk_write (DRIVER_DATA (pnd), abtFrame, szFrame);
  // HACK This little hack is a well know problem of USB, see http://www.libusb.org/ticket/6 for more details
  if ((res % DRIVER_DATA (pnd)->uiMaxPacketSize) == 0) {
    usb_bulk_write (DRIVER_DATA (pnd)->pudh, DRIVER_DATA(pnd)->uiEndPointOut, "\0", 0, USB_TIMEOUT);
  }

  if (res < 0) {
    DBG ("usb_bulk_write failed with error %d", res);
    pnd->iLastError = DEIO;
    return false;
  }

  byte_t abtRxBuf[6];
  res = pn53x_usb_bulk_read (DRIVER_DATA (pnd), abtRxBuf, sizeof (abtRxBuf));
  if (res < 0) {
    DBG ("usb_bulk_read failed with error %d", res);
    pnd->iLastError = DEIO;
    // try to interrupt current device state
    pn53x_usb_ack(pnd);
    return false;
  }

  if (pn53x_check_ack_frame (pnd, abtRxBuf, res)) {
    ((struct pn53x_data*)(pnd->chip_data))->state = EXECUTE;
  } else {
    return false;
  }

  return true;
}

int
pn53x_usb_receive (nfc_device_t * pnd, byte_t * pbtData, const size_t szDataLen)
{
  size_t len;
  off_t offset = 0;
  int abort_fd = 0;

  switch (pnd->iLastCommand) {
  case InAutoPoll:
  case TgInitAsTarget:
  case TgGetData:
    abort_fd = pnd->iAbortFds[1];
    break;
  default:
    break;
  }

  byte_t  abtRxBuf[PN53X_USB_BUFFER_LEN];
  int res = pn53x_usb_bulk_read (DRIVER_DATA (pnd), abtRxBuf, sizeof (abtRxBuf));
  if (res < 0) {
    DBG ("usb_bulk_read failed with error %d", res);
    pnd->iLastError = DEIO;
    // try to interrupt current device state
    pn53x_usb_ack(pnd);
    return false;
  }

  pn53x_usb_ack (pnd);

  const byte_t pn53x_preamble[3] = { 0x00, 0x00, 0xff };
  if (0 != (memcmp (abtRxBuf, pn53x_preamble, 3))) {
    ERR ("%s", "Frame preamble+start code mismatch");
    pnd->iLastError = DEIO;
    return -1;
  }
  offset += 3;

  if ((0x01 == abtRxBuf[offset]) && (0xff == abtRxBuf[offset + 1])) {
    // Error frame
    ERR ("%s", "Application level error detected");
    pnd->iLastError = DEISERRFRAME;
    return -1;
  } else if ((0xff == abtRxBuf[offset]) && (0xff == abtRxBuf[offset + 1])) {
    // Extended frame
    // FIXME: Code this
    abort ();
    offset += 3;
  } else {
    // Normal frame
    if (256 != (abtRxBuf[offset] + abtRxBuf[offset + 1])) {
      // TODO: Retry
      ERR ("%s", "Length checksum mismatch");
      pnd->iLastError = DEIO;
      return -1;
    }

    // abtRxBuf[3] (LEN) include TFI + (CC+1)
    len = abtRxBuf[offset] - 2;
    offset += 2;
  }

  if (len > szDataLen) {
    ERR ("Unable to receive data: buffer too small. (szDataLen: %zu, len: %zu)", szDataLen, len);
    pnd->iLastError = DEIO;
    return -1;
  }

  // TFI + PD0 (CC+1)
  if (abtRxBuf[offset] != 0xD5) {
    ERR ("%s", "TFI Mismatch");
    pnd->iLastError = DEIO;
    return -1;
  }
  offset += 1;

  if (abtRxBuf[offset] != pnd->iLastCommand + 1) {
    ERR ("%s", "Command Code verification failed");
    pnd->iLastError = DEIO;
    return -1;
  }
  offset += 1;

  memcpy (pbtData, abtRxBuf + offset, len);
  offset += len;

  byte_t btDCS = (256 - 0xD5);
  btDCS -= pnd->iLastCommand + 1;
  for (size_t szPos = 0; szPos < len; szPos++) {
    btDCS -= pbtData[szPos];
  }

  if (btDCS != abtRxBuf[offset]) {
    ERR ("%s", "Data checksum mismatch");
    pnd->iLastError = DEIO;
    return -1;
  }
  offset += 1;

  if (0x00 != abtRxBuf[offset]) {
    ERR ("%s", "Frame postamble mismatch");
    pnd->iLastError = DEIO;
    return -1;
  }
  CHIP_DATA (pnd)->state = NORMAL;
  return len;
}

int
pn53x_usb_ack (nfc_device_t * pnd)
{
  return pn53x_usb_bulk_write (DRIVER_DATA (pnd), (byte_t *) ack_frame, sizeof (ack_frame));
}

bool
pn53x_usb_initiator_init (nfc_device_t *pnd)
{
  if (!pn53x_initiator_init (pnd))
    return false;

  if (ASK_LOGO == DRIVER_DATA (pnd)->model) {
    DBG ("ASK LoGO initialization.");
    pn53x_write_register (pnd, 0x6106, 0xFF, 0x1B);
    pn53x_write_register (pnd, 0x6306, 0xFF, 0x14);
    pn53x_write_register (pnd, 0xFFFD, 0xFF, 0x37);
    pn53x_write_register (pnd, 0xFFB0, 0xFF, 0x3B);
  }

  return true;
}

const struct pn53x_io pn53x_usb_io = {
  .send       = pn53x_usb_send,
  .receive    = pn53x_usb_receive,
};

const struct nfc_driver_t pn53x_usb_driver = {
  .name       = PN53X_USB_DRIVER_NAME,
  .probe      = pn53x_usb_probe,
  .connect    = pn53x_usb_connect,
  .disconnect = pn53x_usb_disconnect,
  .strerror   = pn53x_strerror,

  .initiator_init                   = pn53x_usb_initiator_init,
  .initiator_select_passive_target  = pn53x_initiator_select_passive_target,
  .initiator_poll_targets           = pn53x_initiator_poll_targets,
  .initiator_select_dep_target      = pn53x_initiator_select_dep_target,
  .initiator_deselect_target        = pn53x_initiator_deselect_target,
  .initiator_transceive_bytes       = pn53x_initiator_transceive_bytes,
  .initiator_transceive_bits        = pn53x_initiator_transceive_bits,

  .target_init           = pn53x_target_init,
  .target_send_bytes     = pn53x_target_send_bytes,
  .target_receive_bytes  = pn53x_target_receive_bytes,
  .target_send_bits      = pn53x_target_send_bits,
  .target_receive_bits   = pn53x_target_receive_bits,

  .configure  = pn53x_configure,
};
