/*-
 * Public platform independent Near Field Communication (NFC) library
 *
 * Copyright (C) 2009 Roel Verdult
 * Copyright (C) 2011 Romain Tartière
 * Copyright (C) 2011, 2012 Romuald Conty
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
 * @file acr122_usb.h
 * @brief Driver for ACR122 devices using direct USB connection (without PCSC)
 */

#ifndef __NFC_DRIVER_ACR122_USB_H__
#define __NFC_DRIVER_ACR122_USB_H__

#include <nfc/nfc-types.h>

extern const struct nfc_driver acr122_usb_driver;

#endif // ! __NFC_DRIVER_ACR122_USB_H__
