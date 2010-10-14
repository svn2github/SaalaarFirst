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
 */

/**
 * @file pn53x.h
 * @brief PN531, PN532 and PN533 common functions
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif // HAVE_CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>

#include <nfc/nfc.h>
// FIXME: WTF are doing debug macros in this file?
#include <nfc/nfc-messages.h>

#include "pn53x.h"
#include "../mirror-subr.h"

#ifdef _WIN32
#  include <windows.h>

#  define strdup _strdup
#  define snprintf sprintf_s
#endif

#define MAX(a,b) (((a) > (b)) ? (a) : (b))

// PN53X configuration
const byte_t pncmd_get_firmware_version[2] = { 0xD4, 0x02 };
const byte_t pncmd_get_general_status[2] = { 0xD4, 0x04 };
const byte_t pncmd_get_register[4] = { 0xD4, 0x06 };
const byte_t pncmd_set_register[5] = { 0xD4, 0x08 };
const byte_t pncmd_set_parameters[3] = { 0xD4, 0x12 };
const byte_t pncmd_rf_configure[14] = { 0xD4, 0x32 };

// Reader
const byte_t pncmd_initiator_list_passive[264] = { 0xD4, 0x4A };
const byte_t pncmd_initiator_jump_for_dep[68] = { 0xD4, 0x56 };
const byte_t pncmd_initiator_select[3] = { 0xD4, 0x54 };
const byte_t pncmd_initiator_deselect[3] = { 0xD4, 0x44, 0x00 };
const byte_t pncmd_initiator_release[3] = { 0xD4, 0x52, 0x00 };
const byte_t pncmd_initiator_set_baud_rate[5] = { 0xD4, 0x4E };
const byte_t pncmd_initiator_exchange_data[265] = { 0xD4, 0x40 };
const byte_t pncmd_initiator_exchange_raw_data[266] = { 0xD4, 0x42 };
const byte_t pncmd_initiator_auto_poll[5] = { 0xD4, 0x60 };

// Target
const byte_t pncmd_target_get_data[2] = { 0xD4, 0x86 };
const byte_t pncmd_target_set_data[264] = { 0xD4, 0x8E };
const byte_t pncmd_target_init[39] = { 0xD4, 0x8C };
//Example of default values:
//const byte_t pncmd_target_init[39] = { 0xD4, 0x8C, 0x00, 0x08, 0x00, 0x12, 0x34, 0x56, 0x40, 0x01, 0xFE, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xFF, 0xFF, 0xAA, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00, 0x00 };
const byte_t pncmd_target_virtual_card[4] = { 0xD4, 0x14 };
const byte_t pncmd_target_receive[2] = { 0xD4, 0x88 };
const byte_t pncmd_target_send[264] = { 0xD4, 0x90 };
const byte_t pncmd_target_get_status[2] = { 0xD4, 0x8A };

static const byte_t pn53x_ack_frame[] = { 0x00, 0x00, 0xff, 0x00, 0xff, 0x00 };
static const byte_t pn53x_nack_frame[] = { 0x00, 0x00, 0xff, 0xff, 0x00, 0x00 };
static const byte_t pn53x_error_frame[] = { 0x00, 0x00, 0xff, 0x01, 0xff, 0x7f, 0x81, 0x00 };

/* prototypes */
const nfc_modulation_t pn53x_ptt_to_nm( const pn53x_target_type_t ptt );
const pn53x_modulation_t pn53x_nm_to_pm(const nfc_modulation_t nm);
const pn53x_target_type_t pn53x_nm_to_ptt(const nfc_modulation_t nm);

bool
pn53x_init(nfc_device_t * pnd)
{
  // CRC handling is enabled by default
  pnd->bCrc = true;
  // Parity handling is enabled by default
  pnd->bPar = true;

  // Reset the ending transmission bits register, it is unknown what the last tranmission used there
  pnd->ui8TxBits = 0;
  if (!pn53x_set_reg (pnd, REG_CIU_BIT_FRAMING, SYMBOL_TX_LAST_BITS, 0x00)) {
    return false;
  }

  // We can't read these parameters, so we set a default config by using the SetParameters wrapper
  // Note: pn53x_SetParameters() will save the sent value in pnd->ui8Parameters cache
  if(!pn53x_SetParameters(pnd, PARAM_AUTO_ATR_RES | PARAM_AUTO_RATS)) {
    return false;
  }

  return true;
}

// XXX: Is this function correctly named ?
bool
pn53x_transceive_check_ack_frame_callback (nfc_device_t * pnd, const byte_t * pbtRxFrame, const size_t szRxFrameLen)
{
  if (szRxFrameLen >= sizeof (pn53x_ack_frame)) {
    if (0 == memcmp (pbtRxFrame, pn53x_ack_frame, sizeof (pn53x_ack_frame))) {
      DBG ("%s", "PN53x ACKed");
      return true;
    } else if (0 == memcmp (pbtRxFrame, pn53x_nack_frame, sizeof (pn53x_nack_frame))) {
      DBG ("%s", "PN53x NACKed");
      // TODO: Try to recover
      // A counter could allow the command to be sent again (e.g. max 3 times)
      pnd->iLastError = DENACK;
      return false;
    }
  }
  pnd->iLastError = DEACKMISMATCH;
  ERR ("%s", "Unexpected PN53x reply!");
#if defined(DEBUG)
  // coredump so that we can have a backtrace about how this code was reached.
  abort ();
#endif
  return false;
}

bool
pn53x_transceive_check_error_frame_callback (nfc_device_t * pnd, const byte_t * pbtRxFrame, const size_t szRxFrameLen)
{
  if (szRxFrameLen >= sizeof (pn53x_error_frame)) {
    if (0 == memcmp (pbtRxFrame, pn53x_error_frame, sizeof (pn53x_error_frame))) {
      DBG ("%s", "PN53x sent an error frame");
      pnd->iLastError = DEISERRFRAME;
      return false;
    }
  }

  return true;
}

bool
pn53x_transceive (nfc_device_t * pnd, const byte_t * pbtTx, const size_t szTx, byte_t * pbtRx, size_t * pszRx)
{
  byte_t  abtRx[MAX_FRAME_LEN];
  size_t  szRx;

  // Check if receiving buffers are available, if not, replace them
  if (!pszRx || !pbtRx) {
    pbtRx = abtRx;
    pszRx = &szRx;
  }

  *pszRx = MAX_FRAME_LEN;
  // Call the tranceive callback function of the current device
  if (!pnd->pdc->transceive (pnd, pbtTx, szTx, pbtRx, pszRx))
    return false;
  // XXX Should we put all these numbers behind a human-readable #define ?
  switch (pbtTx[1]) {
  case 0x16:                   // PowerDown
  case 0x40:                   // InDataExchange
  case 0x42:                   // InCommunicateThru
  case 0x44:                   // InDeselect
  case 0x46:                   // InJumpForPSL
  case 0x4e:                   // InPSL
  case 0x50:                   // InATR
  case 0x52:                   // InRelease
  case 0x54:                   // InSelect
  case 0x56:                   // InJumpForDEP
  case 0x86:                   // TgGetData
  case 0x88:                   // TgGetInitiatorCommand
  case 0x8e:                   // TgSetData
  case 0x90:                   // TgResponseToInitiator
  case 0x92:                   // TgSetGeneralBytes
  case 0x94:                   // TgSetMetaData
    pnd->iLastError = pbtRx[0] & 0x3f;
    break;
  default:
    pnd->iLastError = 0;
  }

  return (0 == pnd->iLastError);
}

bool
pn53x_get_reg (nfc_device_t * pnd, uint16_t ui16Reg, uint8_t * ui8Value)
{
  size_t  szValueLen;
  byte_t  abtCmd[sizeof (pncmd_get_register)];
  memcpy (abtCmd, pncmd_get_register, sizeof (pncmd_get_register));

  abtCmd[2] = ui16Reg >> 8;
  abtCmd[3] = ui16Reg & 0xff;
  if (pn53x_transceive (pnd, abtCmd, sizeof (pncmd_get_register), ui8Value, &szValueLen)) {
    if (pnd->nc == NC_PN533) {
       // PN533 prepends its answer by a status byte
       if (ui8Value[0] == 0) {
         ui8Value[0] = ui8Value[1];
       } else {
         return false;
       }
    }
    return true;
  }
  return false;
}

bool
pn53x_set_reg (nfc_device_t * pnd, uint16_t ui16Reg, uint8_t ui8SymbolMask, uint8_t ui8Value)
{
  uint8_t ui8Current;
  byte_t  abtCmd[sizeof (pncmd_set_register)];
  memcpy (abtCmd, pncmd_set_register, sizeof (pncmd_set_register));

  abtCmd[2] = ui16Reg >> 8;
  abtCmd[3] = ui16Reg & 0xff;
  if (!pn53x_get_reg (pnd, ui16Reg, &ui8Current))
    return false;

  abtCmd[4] = ui8Value | (ui8Current & (~ui8SymbolMask));
  return (abtCmd[4] != ui8Current) ? pn53x_transceive (pnd, abtCmd, sizeof (pncmd_set_register), NULL, NULL) : true;
}

bool
pn53x_set_parameter (nfc_device_t * pnd, const uint8_t ui8Parameter, const bool bEnable)
{
  uint8_t ui8Value = (bEnable) ? (pnd->ui8Parameters | ui8Parameter) : (pnd->ui8Parameters & ~(ui8Parameter));
  if (ui8Value != pnd->ui8Parameters) {
    return pn53x_SetParameters(pnd, ui8Value);
  }
  return true;
}

bool
pn53x_SetParameters (nfc_device_t * pnd, const uint8_t ui8Value)
{
  byte_t  abtCmd[sizeof (pncmd_set_parameters)];
  memcpy (abtCmd, pncmd_set_parameters, sizeof (pncmd_set_parameters));

  abtCmd[2] = ui8Value;
  if(!pn53x_transceive (pnd, abtCmd, sizeof (pncmd_set_parameters), NULL, NULL)) {
    return false;
  }
  // We save last parameters in register cache
  pnd->ui8Parameters = ui8Value;
  return true;
}

bool
pn53x_set_tx_bits (nfc_device_t * pnd, const uint8_t ui8Bits)
{
  // Test if we need to update the transmission bits register setting
  if (pnd->ui8TxBits != ui8Bits) {
    // Set the amount of transmission bits in the PN53X chip register
    if (!pn53x_set_reg (pnd, REG_CIU_BIT_FRAMING, SYMBOL_TX_LAST_BITS, ui8Bits))
      return false;

    // Store the new setting
    ((nfc_device_t *) pnd)->ui8TxBits = ui8Bits;
  }
  return true;
}

bool
pn53x_wrap_frame (const byte_t * pbtTx, const size_t szTxBits, const byte_t * pbtTxPar, 
                  byte_t * pbtFrame, size_t * pszFrameBits)
{
  byte_t  btFrame;
  byte_t  btData;
  uint32_t uiBitPos;
  uint32_t uiDataPos = 0;
  size_t  szBitsLeft = szTxBits;

  // Make sure we should frame at least something
  if (szBitsLeft == 0)
    return false;

  // Handle a short response (1byte) as a special case
  if (szBitsLeft < 9) {
    *pbtFrame = *pbtTx;
    *pszFrameBits = szTxBits;
    return true;
  }
  // We start by calculating the frame length in bits
  *pszFrameBits = szTxBits + (szTxBits / 8);

  // Parse the data bytes and add the parity bits
  // This is really a sensitive process, mirror the frame bytes and append parity bits
  // buffer = mirror(frame-byte) + parity + mirror(frame-byte) + parity + ...
  // split "buffer" up in segments of 8 bits again and mirror them
  // air-bytes = mirror(buffer-byte) + mirror(buffer-byte) + mirror(buffer-byte) + ..
  while (true) {
    // Reset the temporary frame byte;
    btFrame = 0;

    for (uiBitPos = 0; uiBitPos < 8; uiBitPos++) {
      // Copy as much data that fits in the frame byte
      btData = mirror (pbtTx[uiDataPos]);
      btFrame |= (btData >> uiBitPos);
      // Save this frame byte
      *pbtFrame = mirror (btFrame);
      // Set the remaining bits of the date in the new frame byte and append the parity bit
      btFrame = (btData << (8 - uiBitPos));
      btFrame |= ((pbtTxPar[uiDataPos] & 0x01) << (7 - uiBitPos));
      // Backup the frame bits we have so far
      pbtFrame++;
      *pbtFrame = mirror (btFrame);
      // Increase the data (without parity bit) position
      uiDataPos++;
      // Test if we are done
      if (szBitsLeft < 9)
        return true;
      szBitsLeft -= 8;
    }
    // Every 8 data bytes we lose one frame byte to the parities
    pbtFrame++;
  }
}

bool
pn53x_unwrap_frame (const byte_t * pbtFrame, const size_t szFrameBits, byte_t * pbtRx, size_t * pszRxBits,
                    byte_t * pbtRxPar)
{
  byte_t  btFrame;
  byte_t  btData;
  uint8_t uiBitPos;
  uint32_t uiDataPos = 0;
  byte_t *pbtFramePos = (byte_t *) pbtFrame;
  size_t  szBitsLeft = szFrameBits;

  // Make sure we should frame at least something
  if (szBitsLeft == 0)
    return false;

  // Handle a short response (1byte) as a special case
  if (szBitsLeft < 9) {
    *pbtRx = *pbtFrame;
    *pszRxBits = szFrameBits;
    return true;
  }
  // Calculate the data length in bits
  *pszRxBits = szFrameBits - (szFrameBits / 9);

  // Parse the frame bytes, remove the parity bits and store them in the parity array
  // This process is the reverse of WrapFrame(), look there for more info
  while (true) {
    for (uiBitPos = 0; uiBitPos < 8; uiBitPos++) {
      btFrame = mirror (pbtFramePos[uiDataPos]);
      btData = (btFrame << uiBitPos);
      btFrame = mirror (pbtFramePos[uiDataPos + 1]);
      btData |= (btFrame >> (8 - uiBitPos));
      pbtRx[uiDataPos] = mirror (btData);
      if (pbtRxPar != NULL)
        pbtRxPar[uiDataPos] = ((btFrame >> (7 - uiBitPos)) & 0x01);
      // Increase the data (without parity bit) position
      uiDataPos++;
      // Test if we are done
      if (szBitsLeft < 9)
        return true;
      szBitsLeft -= 9;
    }
    // Every 8 data bytes we lose one frame byte to the parities
    pbtFramePos++;
  }
}

bool
pn53x_decode_target_data (const byte_t * pbtRawData, size_t szRawData, nfc_chip_t nc, nfc_modulation_type_t nmt,
                          nfc_target_info_t * pnti)
{
  uint8_t szAttribRes;

  switch (nmt) {
  case NMT_ISO14443A:
    // We skip the first byte: its the target number (Tg)
    pbtRawData++;

    // Somehow they switched the lower and upper ATQA bytes around for the PN531 chipset
    if (nc == NC_PN531) {
      pnti->nai.abtAtqa[1] = *(pbtRawData++);
      pnti->nai.abtAtqa[0] = *(pbtRawData++);
    } else {
      pnti->nai.abtAtqa[0] = *(pbtRawData++);
      pnti->nai.abtAtqa[1] = *(pbtRawData++);
    }
    pnti->nai.btSak = *(pbtRawData++);
    // Copy the NFCID1
    pnti->nai.szUidLen = *(pbtRawData++);
    memcpy (pnti->nai.abtUid, pbtRawData, pnti->nai.szUidLen);
    pbtRawData += pnti->nai.szUidLen;

    // Did we received an optional ATS (Smardcard ATR)
    if (szRawData > (pnti->nai.szUidLen + 5)) {
      pnti->nai.szAtsLen = ((*(pbtRawData++)) - 1);     // In pbtRawData, ATS Length byte is counted in ATS Frame.
      memcpy (pnti->nai.abtAts, pbtRawData, pnti->nai.szAtsLen);
    } else {
      pnti->nai.szAtsLen = 0;
    }

    // Strip CT (Cascade Tag) to retrieve and store the _real_ UID
    // (e.g. 0x8801020304050607 is in fact 0x01020304050607)
    if ((pnti->nai.szUidLen == 8) && (pnti->nai.abtUid[0] == 0x88)) {
      pnti->nai.szUidLen = 7;
      memmove (pnti->nai.abtUid, pnti->nai.abtUid + 1, 7);
    } else if ((pnti->nai.szUidLen == 12) && (pnti->nai.abtUid[0] == 0x88) && (pnti->nai.abtUid[4] == 0x88)) {
      pnti->nai.szUidLen = 10;
      memmove (pnti->nai.abtUid, pnti->nai.abtUid + 1, 3);
      memmove (pnti->nai.abtUid + 3, pnti->nai.abtUid + 5, 7);
    }
    break;

  case NMT_ISO14443B:
    // We skip the first byte: its the target number (Tg)
    pbtRawData++;

    // Now we are in ATQB, we skip the first ATQB byte always equal to 0x50
    pbtRawData++;
    
    // Store the PUPI (Pseudo-Unique PICC Identifier)
    memcpy (pnti->nbi.abtPupi, pbtRawData, 4);
    pbtRawData += 4;

    // Store the Application Data
    memcpy (pnti->nbi.abtApplicationData, pbtRawData, 4);
    pbtRawData += 4;

    // Store the Protocol Info
    memcpy (pnti->nbi.abtProtocolInfo, pbtRawData, 3);
    pbtRawData += 3;

    // We leave the ATQB field, we now enter in Card IDentifier
    szAttribRes = *(pbtRawData++);
    if (szAttribRes) {
      pnti->nbi.ui8CardIdentifier = *(pbtRawData++);
    }
    break;

  case NMT_FELICA:
    // We skip the first byte: its the target number (Tg)
    pbtRawData++;

    // Store the mandatory info
    pnti->nfi.szLen = *(pbtRawData++);
    pnti->nfi.btResCode = *(pbtRawData++);
    // Copy the NFCID2t
    memcpy (pnti->nfi.abtId, pbtRawData, 8);
    pbtRawData += 8;
    // Copy the felica padding
    memcpy (pnti->nfi.abtPad, pbtRawData, 8);
    pbtRawData += 8;
    // Test if the System code (SYST_CODE) is available
    if (pnti->nfi.szLen > 18) {
      memcpy (pnti->nfi.abtSysCode, pbtRawData, 2);
    }
    break;
  case NMT_JEWEL:
    // We skip the first byte: its the target number (Tg)
    pbtRawData++;

    // Store the mandatory info
    memcpy (pnti->nji.btSensRes, pbtRawData, 2);
    pbtRawData += 2;
    memcpy (pnti->nji.btId, pbtRawData, 4);
    break;
  default:
    return false;
    break;
  }
  return true;
}

bool
pn53x_initiator_select_passive_target (nfc_device_t * pnd,
                                       const nfc_modulation_t nm,
                                       const byte_t * pbtInitData, const size_t szInitData,
                                       nfc_target_t * pnt)
{
  size_t  szTargetsData;
  byte_t  abtTargetsData[MAX_FRAME_LEN];

  pn53x_modulation_t pm = pn53x_nm_to_pm(nm);
  if (!pn53x_InListPassiveTarget (pnd, pm, 1, pbtInitData, szInitData, abtTargetsData, &szTargetsData))
    return false;

  // Make sure one tag has been found, the PN53X returns 0x00 if none was available
  if (abtTargetsData[0] == 0)
    return false;

  // Is a tag info struct available
  if (pnt) {
    pnt->nm = nm;
    // Fill the tag info struct with the values corresponding to this init modulation
    if (!pn53x_decode_target_data (abtTargetsData + 1, szTargetsData - 1, pnd->nc, nm.nmt, &(pnt->nti))) {
      return false;
    }
  }
  return true;
}

bool
pn53x_initiator_poll_targets (nfc_device_t * pnd,
                              const nfc_modulation_t * pnmModulations, const size_t szModulations,
                              const byte_t btPollNr, const byte_t btPeriod,
                              nfc_target_t * pntTargets, size_t * pszTargetFound)
{
  size_t szTargetTypes = 0;
  pn53x_target_type_t apttTargetTypes[32];
  for (size_t n=0; n<szModulations; n++) {
    apttTargetTypes[szTargetTypes] = pn53x_nm_to_ptt(pnmModulations[n]);
    if( apttTargetTypes[szTargetTypes] == PTT_MIFARE ) { // Hack to have ATR
      apttTargetTypes[szTargetTypes] = PTT_ISO14443_4A_106;
      szTargetTypes++;
      apttTargetTypes[szTargetTypes] = PTT_MIFARE;
    }
    szTargetTypes++;
  }

  return pn53x_InAutoPoll (pnd, apttTargetTypes, szTargetTypes, btPollNr, btPeriod, pntTargets, pszTargetFound);
}


/**
 * @brief C wrapper to InListPassiveTarget command
 * @return true if command is successfully sent
 *
 * @param pnd nfc_device_t struct pointer that represent currently used device
 * @param pmInitModulation Desired modulation
 * @param pbtInitiatorData Optional initiator data used for Felica, ISO14443B, Topaz Polling or for ISO14443A selecting a specific UID
 * @param szInitiatorData Length of initiator data \a pbtInitiatorData
 * @param pbtTargetsData pointer on a pre-allocated byte array to receive TargetData[n] as described in pn53x user manual
 * @param pszTargetsData size_t pointer where size of \a pbtTargetsData will be written
 *
 * @note Selected targets count can be found in \a pbtTargetsData[0] if available (i.e. \a pszTargetsData content is more than 0)
 * @note To decode theses TargetData[n], there is @fn pn53x_decode_target_data
 */
bool
pn53x_InListPassiveTarget (nfc_device_t * pnd,
                           const pn53x_modulation_t pmInitModulation, const byte_t szMaxTargets,
                           const byte_t * pbtInitiatorData, const size_t szInitiatorData,
                           byte_t * pbtTargetsData, size_t * pszTargetsData)
{
  size_t  szRx;
  byte_t  abtCmd[sizeof (pncmd_initiator_list_passive)];
  memcpy (abtCmd, pncmd_initiator_list_passive, sizeof (pncmd_initiator_list_passive));

  abtCmd[2] = szMaxTargets;     // MaxTg

  // XXX Is there is a better way to do this ?
  switch(pmInitModulation) {
    case PM_ISO14443A_106:
    case PM_FELICA_212:
    case PM_FELICA_424:
      // all gone fine.
      break;
    case PM_ISO14443B_106:
      // FIXME Some PN532 doesn't support type B !
    case PM_JEWEL_106:
      if(pnd->nc == NC_PN531) {
        // These modulations are not supported by pn531
        pnd->iLastError = DENOTSUP;
        return false;
      }
      break;
    case PM_ISO14443B_212:
    case PM_ISO14443B_424:
    case PM_ISO14443B_847:
      if(pnd->nc != NC_PN533) {
        // These modulations are not supported by pn531 neither pn532
        pnd->iLastError = DENOTSUP;
        return false;
      }
      break;
    default:
      pnd->iLastError = DENOTSUP;
      return false;
  }
  abtCmd[3] = pmInitModulation; // BrTy, the type of init modulation used for polling a passive tag

  // Set the optional initiator data (used for Felica, ISO14443B, Topaz Polling or for ISO14443A selecting a specific UID).
  if (pbtInitiatorData)
    memcpy (abtCmd + 4, pbtInitiatorData, szInitiatorData);

  // Try to find a tag, call the tranceive callback function of the current device
  szRx = MAX_FRAME_LEN;
  if (pn53x_transceive (pnd, abtCmd, 4 + szInitiatorData, pbtTargetsData, &szRx)) {
    *pszTargetsData = szRx;
    return true;
  } else {
    return false;
  }
}

bool
pn53x_InDeselect (nfc_device_t * pnd, const uint8_t ui8Target)
{
  byte_t  abtCmd[sizeof (pncmd_initiator_deselect)];
  memcpy (abtCmd, pncmd_initiator_deselect, sizeof (pncmd_initiator_deselect));
  abtCmd[2] = ui8Target;

  return (pn53x_transceive (pnd, abtCmd, sizeof (abtCmd), NULL, NULL));
}

bool
pn53x_InRelease (nfc_device_t * pnd, const uint8_t ui8Target)
{
  byte_t  abtCmd[sizeof (pncmd_initiator_release)];
  memcpy (abtCmd, pncmd_initiator_release, sizeof (pncmd_initiator_release));
  abtCmd[2] = ui8Target;

  return (pn53x_transceive (pnd, abtCmd, sizeof (abtCmd), NULL, NULL));
}

bool
pn53x_InAutoPoll (nfc_device_t * pnd,
                  const pn53x_target_type_t * ppttTargetTypes, const size_t szTargetTypes,
                  const byte_t btPollNr, const byte_t btPeriod, nfc_target_t * pntTargets, size_t * pszTargetFound)
{
  size_t  szTxInAutoPoll,
          n,
          szRx;
  byte_t  abtRx[MAX_FRAME_LEN];
  bool    res;
  byte_t *pbtTxInAutoPoll;

  if (pnd->nc != NC_PN532) {
    // This function is not supported by pn531 neither pn533
    pnd->iLastError = DENOTSUP;
    return false;
  }
  // InAutoPoll frame looks like this { 0xd4, 0x60, 0x0f, 0x01, 0x00 } => { direction, command, pollnr, period, types... }
  szTxInAutoPoll = 4 + szTargetTypes;
  pbtTxInAutoPoll = malloc (szTxInAutoPoll);
  pbtTxInAutoPoll[0] = 0xd4;
  pbtTxInAutoPoll[1] = 0x60;
  pbtTxInAutoPoll[2] = btPollNr;
  pbtTxInAutoPoll[3] = btPeriod;
  for (n = 0; n < szTargetTypes; n++) {
    pbtTxInAutoPoll[4 + n] = ppttTargetTypes[n];
  }

  szRx = MAX_FRAME_LEN;
  res = pnd->pdc->transceive (pnd, pbtTxInAutoPoll, szTxInAutoPoll, abtRx, &szRx);

  if ((szRx == 0) || (res == false)) {
    return false;
  } else {
    *pszTargetFound = abtRx[0];
    if (*pszTargetFound) {
      uint8_t ln;
      byte_t *pbt = abtRx + 1;
      /* 1st target */
      // Target type
      pn53x_target_type_t ptt = *(pbt++);
      pntTargets[0].nm = pn53x_ptt_to_nm(ptt);
      // AutoPollTargetData length
      ln = *(pbt++);
      pn53x_decode_target_data (pbt, ln, pnd->nc, pntTargets[0].nm.nmt, &(pntTargets[0].nti));
      pbt += ln;

      if (abtRx[0] > 1) {
        /* 2nd target */
        // Target type
        ptt = *(pbt++);
        pntTargets[1].nm = pn53x_ptt_to_nm(ptt);
        // AutoPollTargetData length
        ln = *(pbt++);
        pn53x_decode_target_data (pbt, ln, pnd->nc, pntTargets[1].nm.nmt, &(pntTargets[1].nti));
      }
    }
  }
  return true;
}

static struct sErrorMessage {
  int     iErrorCode;
  const char *pcErrorMsg;
} sErrorMessages[] = {
  /* Chip-level errors */
  {
  0x00, "Success"}, {
  0x01, "Timeout"}, {
  0x02, "CRC Error"}, {
  0x03, "Parity Error"}, {
  0x04, "Erroneous Bit Count"}, {
  0x05, "Framing Error"}, {
  0x06, "Bit-collision"}, {
  0x07, "Buffer Too Small"}, {
  0x09, "Buffer Overflow"}, {
  0x0a, "Timeout"}, {
  0x0b, "Protocol Error"}, {
  0x0d, "Overheating"}, {
  0x0e, "Internal Buffer overflow."}, {
  0x10, "Invalid Parameter"},
    /* DEP Errors */
  {
  0x12, "Unknown DEP Command"}, {
  0x13, "Invalid Parameter"},
    /* MIFARE */
  {
  0x14, "Authentication Error"},
    /*  */
  {
  0x23, "Wrong ISO/IEC14443-3 Check Byte"}, {
  0x25, "Invalid State"}, {
  0x26, "Operation Not Allowed"}, {
  0x27, "Command Not Acceptable"}, {
  0x29, "Target Released"}, {
  0x2a, "Card ID Mismatch"}, {
  0x2B, "Card Discarded"}, {
  0x2C, "NFCID3 Mismatch"}, {
  0x2D, "Over Current"}, {
  0x2E, "NAD Missing in DEP Frame"},
    /* Driver-level error */
  {
  DENACK, "Received NACK"}, {
  DEACKMISMATCH, "Expected ACK/NACK"}, {
  DEISERRFRAME, "Received an error frame"},
    /* TODO: Move me in more generic code for libnfc 1.6 */
  {
  DEINVAL, "Invalid argument"}, {
  DEIO, "Input/output error"}, {
  DETIMEOUT, "Operation timed-out"}, {
  DENOTSUP, "Operation not supported"}
};

const char *
pn53x_strerror (const nfc_device_t * pnd)
{
  const char *pcRes = "Unknown error";
  size_t  i;

  for (i = 0; i < (sizeof (sErrorMessages) / sizeof (struct sErrorMessage)); i++) {
    if (sErrorMessages[i].iErrorCode == pnd->iLastError) {
      pcRes = sErrorMessages[i].pcErrorMsg;
      break;
    }
  }

  return pcRes;
}

bool
pn53x_get_firmware_version (nfc_device_t * pnd)
{
  byte_t  abtFw[4];
  size_t  szFwLen = sizeof (abtFw);
  char   *pcName;
  // TODO Read more info here: there are modulation capabilities info to know if ISO14443B is supported
  if (!pn53x_transceive (pnd, pncmd_get_firmware_version, 2, abtFw, &szFwLen)) {
    // Failed to get firmware revision??, whatever...let's disconnect and clean up and return err
    DBG ("Failed to get firmware revision for: %s", pnd->acName);
    pnd->pdc->disconnect (pnd);
    return false;
  }
  // Add the firmware revision to the device name, PN531 gives 2 bytes info, but PN532 and PN533 gives 4
  pcName = strdup (pnd->acName);
  switch (pnd->nc) {
  case NC_PN531:
    snprintf (pnd->acName, DEVICE_NAME_LENGTH - 1, "%s - PN531 v%d.%d", pcName, abtFw[0], abtFw[1]);
    break;
  case NC_PN532:
    snprintf (pnd->acName, DEVICE_NAME_LENGTH - 1, "%s - PN532 v%d.%d (0x%02x)", pcName, abtFw[1], abtFw[2], abtFw[3]);
    break;
  case NC_PN533:
    snprintf (pnd->acName, DEVICE_NAME_LENGTH - 1, "%s - PN533 v%d.%d (0x%02x)", pcName, abtFw[1], abtFw[2], abtFw[3]);
    break;
  }
  free (pcName);
  return true;
}

bool
pn53x_configure (nfc_device_t * pnd, const nfc_device_option_t ndo, const bool bEnable)
{
  byte_t  btValue;
  byte_t  abtCmd[sizeof (pncmd_rf_configure)];

  memcpy (abtCmd, pncmd_rf_configure, sizeof (pncmd_rf_configure));

  // Make sure we are dealing with a active device
  if (!pnd->bActive)
    return false;

  switch (ndo) {
  case NDO_HANDLE_CRC:
    // Enable or disable automatic receiving/sending of CRC bytes
    // TX and RX are both represented by the symbol 0x80
    btValue = (bEnable) ? 0x80 : 0x00;
    if (!pn53x_set_reg (pnd, REG_CIU_TX_MODE, SYMBOL_TX_CRC_ENABLE, btValue))
      return false;
    if (!pn53x_set_reg (pnd, REG_CIU_RX_MODE, SYMBOL_RX_CRC_ENABLE, btValue))
      return false;
    pnd->bCrc = bEnable;
    break;

  case NDO_HANDLE_PARITY:
    // Handle parity bit by PN53X chip or parse it as data bit
    btValue = (bEnable) ? 0x00 : SYMBOL_PARITY_DISABLE;
    if (!pn53x_set_reg (pnd, REG_CIU_MANUAL_RCV, SYMBOL_PARITY_DISABLE, btValue))
      return false;
    pnd->bPar = bEnable;
    break;

  case NDO_EASY_FRAMING:
    pnd->bEasyFraming = bEnable;
    break;

  case NDO_ACTIVATE_FIELD:
    abtCmd[2] = RFCI_FIELD;
    abtCmd[3] = (bEnable) ? 1 : 0;
    if (!pn53x_transceive (pnd, abtCmd, 4, NULL, NULL))
      return false;
    break;

  case NDO_ACTIVATE_CRYPTO1:
    btValue = (bEnable) ? SYMBOL_MF_CRYPTO1_ON : 0x00;
    if (!pn53x_set_reg (pnd, REG_CIU_STATUS2, SYMBOL_MF_CRYPTO1_ON, btValue))
      return false;
    break;

  case NDO_INFINITE_SELECT:
    // Retry format: 0x00 means only 1 try, 0xff means infinite
    abtCmd[2] = RFCI_RETRY_SELECT;
    abtCmd[3] = (bEnable) ? 0xff : 0x00;        // MxRtyATR, default: active = 0xff, passive = 0x02
    abtCmd[4] = (bEnable) ? 0xff : 0x00;        // MxRtyPSL, default: 0x01
    abtCmd[5] = (bEnable) ? 0xff : 0x00;        // MxRtyPassiveActivation, default: 0xff
    if (!pn53x_transceive (pnd, abtCmd, 6, NULL, NULL))
      return false;
    break;

  case NDO_ACCEPT_INVALID_FRAMES:
    btValue = (bEnable) ? SYMBOL_RX_NO_ERROR : 0x00;
    if (!pn53x_set_reg (pnd, REG_CIU_RX_MODE, SYMBOL_RX_NO_ERROR, btValue))
      return false;
    break;

  case NDO_ACCEPT_MULTIPLE_FRAMES:
    btValue = (bEnable) ? SYMBOL_RX_MULTIPLE : 0x00;
    if (!pn53x_set_reg (pnd, REG_CIU_RX_MODE, SYMBOL_RX_MULTIPLE, btValue))
      return false;
    return true;
    break;

  case NDO_AUTO_ISO14443_4:
    return pn53x_set_parameter(pnd, PARAM_AUTO_RATS, bEnable);
    break;

  case NDO_FORCE_ISO14443_A:
    if(!bEnable) {
      // Nothing to do
      return true;
    }
    // Force pn53x to be in ISO14443-A mode
    if (!pn53x_set_reg (pnd, REG_CIU_TX_MODE, SYMBOL_TX_FRAMING, 0x00)) {
      return false;
    }
    if (!pn53x_set_reg (pnd, REG_CIU_RX_MODE, SYMBOL_RX_FRAMING, 0x00)) {
      return false;
    }
    return true;
    break;
  }

  // When we reach this, the configuration is completed and succesful
  return true;
}

bool
pn53x_initiator_select_dep_target(nfc_device_t * pnd, const nfc_dep_mode_t ndm,
                                  const nfc_dep_info_t * pndiInitiator,
                                  nfc_target_t * pnt)
{
  if (pndiInitiator) {
    return pn53x_InJumpForDEP (pnd, ndm, NULL, 0, pndiInitiator->abtNFCID3, pndiInitiator->abtGB, pndiInitiator->szGB, pnt);
  } else {
    return pn53x_InJumpForDEP (pnd, ndm, NULL, 0, NULL, NULL, 0, pnt);
  }
}

/**
 * @brief Wrapper for InJumpForDEP command
 * @param pmInitModulation desired initial modulation
 * @param pbtPassiveInitiatorData NFCID1 at 106kbps (see NFCIP-1: 11.2.1.26) or Polling Request Frame's payload at 212/424kbps (see NFCIP-1: 11.2.2.5)
 * @param szPassiveInitiatorData size of pbtPassiveInitiatorData content
 * @param pbtNFCID3i NFCID3 of the initiator
 * @param pbtGB General Bytes
 * @param szGB count of General Bytes
 * @param[out] pnt \a nfc_target_t which will be filled by this function
 */
bool
pn53x_InJumpForDEP (nfc_device_t * pnd,
                    const nfc_dep_mode_t ndm,
                    const byte_t * pbtPassiveInitiatorData, const size_t szPassiveInitiatorData, 
                    const byte_t * pbtNFCID3i,
                    const byte_t * pbtGB, const size_t szGB,
                    nfc_target_t * pnt)
{
  byte_t  abtRx[MAX_FRAME_LEN];
  size_t  szRx;
  size_t  offset;
  byte_t  abtCmd[sizeof (pncmd_initiator_jump_for_dep)];

  memcpy (abtCmd, pncmd_initiator_jump_for_dep, sizeof (pncmd_initiator_jump_for_dep));

  abtCmd[2] = (ndm == NDM_ACTIVE) ? 0x01 : 0x00;

  // FIXME Baud rate in D.E.P. mode is hard-wired as 106kbps
  abtCmd[3] = 0x00;             /* baud rate = 106kbps */

  offset = 5;
  if (pbtPassiveInitiatorData && (ndm == NDM_PASSIVE)) {        /* can't have passive initiator data when using active mode */
    abtCmd[4] |= 0x01;
    memcpy (abtCmd + offset, pbtPassiveInitiatorData, szPassiveInitiatorData);
    offset += szPassiveInitiatorData;
  }

  if (pbtNFCID3i) {
    abtCmd[4] |= 0x02;
    memcpy (abtCmd + offset, pbtNFCID3i, 10);
    offset += 10;
  }

  if (szGB && pbtGB) {
    abtCmd[4] |= 0x04;
    memcpy (abtCmd + offset, pbtGB, szGB);
    offset += szGB;
  }
  // Try to find a target, call the transceive callback function of the current device
  if (!pn53x_transceive (pnd, abtCmd, 5 + szPassiveInitiatorData + 10 + szGB, abtRx, &szRx))
    return false;

  // Make sure one target has been found, the PN53X returns 0x00 if none was available
  if (abtRx[1] != 1)
    return false;

  // Is a target struct available
  if (pnt) {
    pnt->nm.nmt = NMT_DEP;
    pnt->nm.nmt = NBR_UNDEFINED;
    memcpy (pnt->nti.ndi.abtNFCID3, abtRx + 2, 10);
    pnt->nti.ndi.btDID = abtRx[12];
    pnt->nti.ndi.btBS = abtRx[13];
    pnt->nti.ndi.btBR = abtRx[14];
    pnt->nti.ndi.btTO = abtRx[15];
    pnt->nti.ndi.btPP = abtRx[16];
    if(szRx > 17) {
      pnt->nti.ndi.szGB = szRx - 17;
      memcpy (pnt->nti.ndi.abtGB, abtRx + 17, pnt->nti.ndi.szGB);
    } else {
      pnt->nti.ndi.szGB = 0;
    }
  }
  return true;
}

bool
pn53x_initiator_transceive_bits (nfc_device_t * pnd, const byte_t * pbtTx, const size_t szTxBits,
                                 const byte_t * pbtTxPar, byte_t * pbtRx, size_t * pszRxBits, byte_t * pbtRxPar)
{
  byte_t  abtRx[MAX_FRAME_LEN];
  size_t  szRx;
  size_t  szFrameBits = 0;
  size_t  szFrameBytes = 0;
  uint8_t ui8rcc;
  uint8_t ui8Bits = 0;
  byte_t  abtCmd[sizeof (pncmd_initiator_exchange_raw_data)];

  memcpy (abtCmd, pncmd_initiator_exchange_raw_data, sizeof (pncmd_initiator_exchange_raw_data));

  // Check if we should prepare the parity bits ourself
  if (!pnd->bPar) {
    // Convert data with parity to a frame
    pn53x_wrap_frame (pbtTx, szTxBits, pbtTxPar, abtCmd + 2, &szFrameBits);
  } else {
    szFrameBits = szTxBits;
  }

  // Retrieve the leading bits
  ui8Bits = szFrameBits % 8;

  // Get the amount of frame bytes + optional (1 byte if there are leading bits) 
  szFrameBytes = (szFrameBits / 8) + ((ui8Bits == 0) ? 0 : 1);

  // When the parity is handled before us, we just copy the data
  if (pnd->bPar)
    memcpy (abtCmd + 2, pbtTx, szFrameBytes);

  // Set the amount of transmission bits in the PN53X chip register
  if (!pn53x_set_tx_bits (pnd, ui8Bits))
    return false;

  // Send the frame to the PN53X chip and get the answer
  // We have to give the amount of bytes + (the two command bytes 0xD4, 0x42)
  if (!pn53x_transceive (pnd, abtCmd, szFrameBytes + 2, abtRx, &szRx))
    return false;

  // Get the last bit-count that is stored in the received byte 
  if (!pn53x_get_reg (pnd, REG_CIU_CONTROL, &ui8rcc))
    return false;
  ui8Bits = ui8rcc & SYMBOL_RX_LAST_BITS;

  // Recover the real frame length in bits
  szFrameBits = ((szRx - 1 - ((ui8Bits == 0) ? 0 : 1)) * 8) + ui8Bits;

  // Ignore the status byte from the PN53X here, it was checked earlier in pn53x_transceive()
  // Check if we should recover the parity bits ourself
  if (!pnd->bPar) {
    // Unwrap the response frame
    pn53x_unwrap_frame (abtRx + 1, szFrameBits, pbtRx, pszRxBits, pbtRxPar);
  } else {
    // Save the received bits
    *pszRxBits = szFrameBits;
    // Copy the received bytes
    memcpy (pbtRx, abtRx + 1, szRx - 1);
  }

  // Everything went successful
  return true;
}

bool
pn53x_initiator_transceive_bytes (nfc_device_t * pnd, const byte_t * pbtTx, const size_t szTx, byte_t * pbtRx,
                                  size_t * pszRx)
{
  byte_t  abtRx[MAX_FRAME_LEN];
  size_t  szExtraTxLen,
          szRx;
  byte_t  abtCmd[sizeof (pncmd_initiator_exchange_raw_data)];

  // We can not just send bytes without parity if while the PN53X expects we handled them
  if (!pnd->bPar)
    return false;

  // Copy the data into the command frame
  if (pnd->bEasyFraming) {
    memcpy (abtCmd, pncmd_initiator_exchange_data, sizeof (pncmd_initiator_exchange_data));
    abtCmd[2] = 1;              /* target number */
    memcpy (abtCmd + 3, pbtTx, szTx);
    szExtraTxLen = 3;
  } else {
    memcpy (abtCmd, pncmd_initiator_exchange_raw_data, sizeof (pncmd_initiator_exchange_raw_data));
    memcpy (abtCmd + 2, pbtTx, szTx);
    szExtraTxLen = 2;
  }

  // To transfer command frames bytes we can not have any leading bits, reset this to zero
  if (!pn53x_set_tx_bits (pnd, 0))
    return false;

  // Send the frame to the PN53X chip and get the answer
  // We have to give the amount of bytes + (the two command bytes 0xD4, 0x42)
  if (!pn53x_transceive (pnd, abtCmd, szTx + szExtraTxLen, abtRx, &szRx))
    return false;

  // Save the received byte count
  *pszRx = szRx - 1;

  // Copy the received bytes
  memcpy (pbtRx, abtRx + 1, *pszRx);

  // Everything went successful
  return true;
}

bool
pn53x_target_init (nfc_device_t * pnd, const nfc_target_mode_t ntm, const nfc_target_t nt, byte_t * pbtRx, size_t * pszRx)
{
  // Save the current configuration settings
  bool    bCrc = pnd->bCrc;
  bool    bPar = pnd->bPar;

  // Configure the target corresponding to the requested mode
  // XXX I (Romuald) don't think that a good thing to select NDO_EASY_FRAMING here, that's a user choice...
  switch(ntm)
  {
    case NTM_PASSIVE_ONLY:
      pn53x_set_parameter(pnd, PARAM_AUTO_ATR_RES, false);
      pn53x_configure(pnd, NDO_EASY_FRAMING, false);
    break;

    case NTM_DEP_ONLY:
      pn53x_set_parameter(pnd, PARAM_AUTO_ATR_RES, true);
      pn53x_configure(pnd, NDO_EASY_FRAMING, true);
    break;

    case NTM_ISO14443_4_PICC_ONLY:
      if(pnd->nc != NC_PN532) {
        // This mode is not supported by pn531 neither pn533
        pnd->iLastError = DENOTSUP;
        return false;
      }
      pn53x_set_parameter(pnd, PARAM_14443_4_PICC, true);
      pn53x_configure(pnd, NDO_EASY_FRAMING, true);
    break;

    default:
      // Unknown mode
      return false;
  }

  // Make sure the CRC & parity are handled by the device, this is needed for target_init to work properly
  if (!bCrc)
    pn53x_configure ((nfc_device_t *) pnd, NDO_HANDLE_CRC, true);
  if (!bPar)
    pn53x_configure ((nfc_device_t *) pnd, NDO_HANDLE_PARITY, true);

  // Let the PN53X be activated by the RF level detector from power down mode
  if (!pn53x_set_reg (pnd, REG_CIU_TX_AUTO, SYMBOL_INITIAL_RF_ON, 0x04))
    return false;

  byte_t abtMifareParams[6];
  byte_t abtFeliCaParams[18];
  byte_t * pbtMifareParams = NULL;
  byte_t * pbtFeliCaParams = NULL;

  const byte_t * pbtNFCID3t = NULL;
  const byte_t * pbtGB = NULL;
  size_t szGB = 0;

  switch(nt.nm.nmt) {
    case NMT_ISO14443A: {
      // Set ATQA (SENS_RES)
      abtMifareParams[0] = nt.nti.nai.abtAtqa[1];
      abtMifareParams[1] = nt.nti.nai.abtAtqa[0];
      // Set UID 
      // Note: in this mode we can only emulate a single size (4 bytes) UID where the first is fixed by PN53x as 0x08
      abtMifareParams[2] = nt.nti.nai.abtUid[1];
      abtMifareParams[3] = nt.nti.nai.abtUid[2];
      abtMifareParams[4] = nt.nti.nai.abtUid[3];
      // Set SAK (SEL_RES)
      abtMifareParams[5] = nt.nti.nai.btSak;

      pbtMifareParams = abtMifareParams;
    }
    break;

    case NMT_FELICA:
      // Set NFCID2t 
      memcpy(abtFeliCaParams, nt.nti.nfi.abtId, 8);
      // Set PAD
      memcpy(abtFeliCaParams+8, nt.nti.nfi.abtPad, 8);
      // Set SystemCode
      memcpy(abtFeliCaParams+16, nt.nti.nfi.abtSysCode, 2);
      pbtFeliCaParams = abtFeliCaParams;
    break;

    case NMT_DEP:
      pbtNFCID3t = nt.nti.ndi.abtNFCID3;
      szGB = nt.nti.ndi.szGB;
      if (szGB) pbtGB = nt.nti.ndi.abtGB;
    break;
    case NMT_ISO14443B:
    case NMT_JEWEL:
      pnd->iLastError = DENOTSUP;
      return false;
    break;
  }

  if(!pn53x_TgInitAsTarget(pnd, ntm, pbtMifareParams, pbtFeliCaParams, pbtNFCID3t, pbtGB, szGB, pbtRx, pszRx, NULL)) {
    return false;
  }

  // Restore the CRC & parity setting to the original value (if needed)
  if (!bCrc)
    pn53x_configure ((nfc_device_t *) pnd, NDO_HANDLE_CRC, false);
  if (!bPar)
    pn53x_configure ((nfc_device_t *) pnd, NDO_HANDLE_PARITY, false);

  return true;
}

bool
pn53x_TgInitAsTarget (nfc_device_t * pnd, nfc_target_mode_t ntm, 
                      const byte_t * pbtMifareParams,
                      const byte_t * pbtFeliCaParams,
                      const byte_t * pbtNFCID3t, const byte_t * pbtGB, const size_t szGB,
                      byte_t * pbtRx, size_t * pszRx, byte_t * pbtModeByte)
{
  byte_t  abtRx[MAX_FRAME_LEN];
  size_t  szRx;
  byte_t  abtCmd[sizeof (pncmd_target_init) + 48]; // 47 bytes max. for General Bytes and 1 for GB lenght
  size_t  szOptionalBytes = 0;

  memcpy (abtCmd, pncmd_target_init, sizeof (pncmd_target_init));

  // Clear the target init struct, reset to all zeros
  memset (abtCmd + 2, 0x00, sizeof (pncmd_target_init)-2);

  // Store the target mode in the initialization params
  abtCmd[2] = ntm;

  // MIFARE part
  if (pbtMifareParams) {
    memcpy (abtCmd+3, pbtMifareParams, 6);
  }
  // FeliCa part
  if (pbtFeliCaParams) {
    memcpy (abtCmd+9, pbtFeliCaParams, 18);
  }
  // DEP part
  if (pbtNFCID3t) {
    memcpy(abtCmd+27, pbtNFCID3t, 10);
  }
  if (szGB) {
    if( pnd->nc == NC_PN531 ) {
      memcpy (abtCmd+37, pbtGB, szGB);
      szOptionalBytes = szGB;
    } else {
      abtCmd[37] = (byte_t)(szGB);
      memcpy (abtCmd+38, pbtGB, szGB);
      szOptionalBytes = szGB + 1;
    }
  }
  // TODO Handle Tk (Historical bytes) length (only available on PN532, PN533)

  // Request the initialization as a target
  szRx = MAX_FRAME_LEN;
  if (!pn53x_transceive (pnd, abtCmd, sizeof (pncmd_target_init) + szOptionalBytes, abtRx, &szRx))
    return false;

  // Note: the first byte is skip: 
  //       its the "mode" byte which contains baudrate, DEP and Framing type (Mifare, active or FeliCa) datas.
  if(pbtModeByte) {
    *pbtModeByte = pbtRx[0];
  }

  // Save the received byte count
  *pszRx = szRx - 1;
  // Copy the received bytes
  memcpy (pbtRx, abtRx + 1, *pszRx);

  return true;
}

bool
pn53x_target_receive_bits (nfc_device_t * pnd, byte_t * pbtRx, size_t * pszRxBits, byte_t * pbtRxPar)
{
  byte_t  abtRx[MAX_FRAME_LEN];
  size_t  szRx;
  size_t  szFrameBits;
  uint8_t ui8rcc;
  uint8_t ui8Bits;

  // Try to gather a received frame from the reader
  if (!pn53x_transceive (pnd, pncmd_target_receive, 2, abtRx, &szRx))
    return false;

  // Get the last bit-count that is stored in the received byte 
  if (!pn53x_get_reg (pnd, REG_CIU_CONTROL, &ui8rcc))
    return false;
  ui8Bits = ui8rcc & SYMBOL_RX_LAST_BITS;

  // Recover the real frame length in bits
  szFrameBits = ((szRx - 1 - ((ui8Bits == 0) ? 0 : 1)) * 8) + ui8Bits;

  // Ignore the status byte from the PN53X here, it was checked earlier in pn53x_transceive()
  // Check if we should recover the parity bits ourself
  if (!pnd->bPar) {
    // Unwrap the response frame
    pn53x_unwrap_frame (abtRx + 1, szFrameBits, pbtRx, pszRxBits, pbtRxPar);
  } else {
    // Save the received bits
    *pszRxBits = szFrameBits;
    // Copy the received bytes
    memcpy (pbtRx, abtRx + 1, szRx - 1);
  }
  // Everyting seems ok, return true
  return true;
}

bool
pn53x_target_receive_bytes (nfc_device_t * pnd, byte_t * pbtRx, size_t * pszRx)
{
  byte_t const *pbtTx;
  byte_t  abtRx[MAX_FRAME_LEN];
  size_t  szRx;

  if (pnd->bEasyFraming) {
    pbtTx = pncmd_target_get_data;
  } else {
    pbtTx = pncmd_target_receive;
  }

  // Try to gather a received frame from the reader
  if (!pn53x_transceive (pnd, pbtTx, 2, abtRx, &szRx))
    return false;

  // Save the received byte count
  *pszRx = szRx - 1;

  // Copy the received bytes
  memcpy (pbtRx, abtRx + 1, *pszRx);

  // Everyting seems ok, return true
  return true;
}

bool
pn53x_target_send_bits (nfc_device_t * pnd, const byte_t * pbtTx, const size_t szTxBits, const byte_t * pbtTxPar)
{
  size_t  szFrameBits = 0;
  size_t  szFrameBytes = 0;
  uint8_t ui8Bits = 0;
  byte_t  abtCmd[sizeof (pncmd_target_send)];

  memcpy (abtCmd, pncmd_target_send, sizeof (pncmd_target_send));

  // Check if we should prepare the parity bits ourself
  if (!pnd->bPar) {
    // Convert data with parity to a frame
    pn53x_wrap_frame (pbtTx, szTxBits, pbtTxPar, abtCmd + 2, &szFrameBits);
  } else {
    szFrameBits = szTxBits;
  }

  // Retrieve the leading bits
  ui8Bits = szFrameBits % 8;

  // Get the amount of frame bytes + optional (1 byte if there are leading bits) 
  szFrameBytes = (szFrameBits / 8) + ((ui8Bits == 0) ? 0 : 1);

  // When the parity is handled before us, we just copy the data
  if (pnd->bPar)
    memcpy (abtCmd + 2, pbtTx, szFrameBytes);

  // Set the amount of transmission bits in the PN53X chip register
  if (!pn53x_set_tx_bits (pnd, ui8Bits))
    return false;

  // Try to send the bits to the reader
  if (!pn53x_transceive (pnd, abtCmd, szFrameBytes + 2, NULL, NULL))
    return false;

  // Everyting seems ok, return true
  return true;
}

bool
pn53x_target_send_bytes (nfc_device_t * pnd, const byte_t * pbtTx, const size_t szTx)
{
  byte_t  abtCmd[MAX (sizeof (pncmd_target_send), sizeof (pncmd_target_set_data))];


  // We can not just send bytes without parity if while the PN53X expects we handled them
  if (!pnd->bPar)
    return false;

  if (pnd->bEasyFraming) {
    memcpy (abtCmd, pncmd_target_set_data, sizeof (pncmd_target_set_data));
  } else {
    memcpy (abtCmd, pncmd_target_send, sizeof (pncmd_target_send));
  }

  // Copy the data into the command frame
  memcpy (abtCmd + 2, pbtTx, szTx);

  // Try to send the bits to the reader
  if (!pn53x_transceive (pnd, abtCmd, szTx + 2, NULL, NULL))
    return false;

  // Everyting seems ok, return true
  return true;
}

// FIXME How to handle corner case ?
const pn53x_modulation_t
pn53x_nm_to_pm(const nfc_modulation_t nm)
{
  switch(nm.nmt) {
    case NMT_ISO14443A:
      return PM_ISO14443A_106;
    break;

    case NMT_ISO14443B:
      switch(nm.nbr) {
        case NBR_106:
          return PM_ISO14443B_106;
        break;
        case NBR_212:
          return PM_ISO14443B_212;
        break;
        case NBR_424:
          return PM_ISO14443B_424;
        break;
        case NBR_UNDEFINED:
          // XXX What to do ?
        break;
      }
    break;

    case NMT_JEWEL:
      return PM_JEWEL_106;
    break;

    case NMT_FELICA:
      switch(nm.nbr) {
        case NBR_212:
          return PM_FELICA_212;
        break;
        case NBR_424:
          return PM_FELICA_424;
        break;
        case NBR_106:
        case NBR_UNDEFINED:
          // XXX What to do ?
        break;
      }
    break;
  }
}

// FIXME How to handle corner case ?
const nfc_modulation_t
pn53x_ptt_to_nm( const pn53x_target_type_t ptt )
{
  switch (ptt) {
    case PTT_GENERIC_PASSIVE_106:
    case PTT_GENERIC_PASSIVE_212:
    case PTT_GENERIC_PASSIVE_424:
      // XXX This should not happend, how handle it cleanly ?
    break;

    case PTT_MIFARE:
    case PTT_ISO14443_4A_106:
      return (const nfc_modulation_t){ .nmt = NMT_ISO14443A, .nbr = NBR_106 };
    break;

    case PTT_ISO14443_4B_106:
    case PTT_ISO14443_4B_TCL_106:
      return (const nfc_modulation_t){ .nmt = NMT_ISO14443B, .nbr = NBR_106 };
    break;

    case PTT_JEWEL_106:
      return (const nfc_modulation_t){ .nmt = NMT_JEWEL, .nbr = NBR_106 };
    break;

    case PTT_FELICA_212:
      return (const nfc_modulation_t){ .nmt = NMT_FELICA, .nbr = NBR_212 };
    break;
    case PTT_FELICA_424:
      return (const nfc_modulation_t){ .nmt = NMT_FELICA, .nbr = NBR_424 };
    break;

    case PTT_DEP_PASSIVE_106:
    case PTT_DEP_ACTIVE_106:
      return (const nfc_modulation_t){ .nmt = NMT_DEP, .nbr = NBR_106 };
    break;
    case PTT_DEP_PASSIVE_212:
    case PTT_DEP_ACTIVE_212:
      return (const nfc_modulation_t){ .nmt = NMT_DEP, .nbr = NBR_212 };
    break;
    case PTT_DEP_PASSIVE_424:
    case PTT_DEP_ACTIVE_424:
      return (const nfc_modulation_t){ .nmt = NMT_DEP, .nbr = NBR_424 };
    break;
  }
}

// FIXME How to handle corner case ?
const pn53x_target_type_t
pn53x_nm_to_ptt(const nfc_modulation_t nm)
{
  switch(nm.nmt) {
    case NMT_ISO14443A:
      return PTT_MIFARE;
      // return PTT_ISO14443_4A_106;
    break;

    case NMT_ISO14443B:
      switch(nm.nbr) {
        case NBR_106:
          return PTT_ISO14443_4B_106;
        break;
      }
    break;

    case NMT_JEWEL:
      return PTT_JEWEL_106;
    break;

    case NMT_FELICA:
      switch(nm.nbr) {
        case NBR_212:
          return PTT_FELICA_212;
        break;
        case NBR_424:
          return PTT_FELICA_424;
        break;
      }
    break;
  }
}

