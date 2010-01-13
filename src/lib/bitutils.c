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
 * @file bitutils.c
 * @brief
 */

#include <stdio.h>

#include "bitutils.h"

const static byte_t OddParity[256] = {
  1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
  0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
  0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
  1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
  0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
  1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
  1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
  0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
  0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
  1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
  1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
  0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
  1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
  0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
  0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
  1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1
};

const static byte_t ByteMirror[256] = {
  0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 0x10, 0x90, 0x50, 0xd0, 0x30, 
  0xb0, 0x70, 0xf0, 0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8, 0x18, 0x98, 
  0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8, 0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 
  0xe4, 0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4, 0x0c, 0x8c, 0x4c, 0xcc, 
  0x2c, 0xac, 0x6c, 0xec, 0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc, 0x02, 
  0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2, 0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 
  0x72, 0xf2, 0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea, 0x1a, 0x9a, 0x5a, 
  0xda, 0x3a, 0xba, 0x7a, 0xfa, 0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 
  0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6, 0x0e, 0x8e, 0x4e, 0xce, 0x2e, 
  0xae, 0x6e, 0xee, 0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe, 0x01, 0x81, 
  0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1, 0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 
  0xf1, 0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 0x19, 0x99, 0x59, 0xd9, 
  0x39, 0xb9, 0x79, 0xf9, 0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5, 0x15, 
  0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5, 0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 
  0x6d, 0xed, 0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd, 0x03, 0x83, 0x43, 
  0xc3, 0x23, 0xa3, 0x63, 0xe3, 0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3, 
  0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb, 0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 
  0xbb, 0x7b, 0xfb, 0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7, 0x17, 0x97, 
  0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7, 0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 
  0xef, 0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

byte_t oddparity(const byte_t bt)
{
  return OddParity[bt];
}

void oddparity_bytes(const byte_t* pbtData, const size_t szLen, byte_t* pbtPar)
{
  size_t szByteNr;

  // Calculate the parity bits for the command
  for (szByteNr=0; szByteNr<szLen; szByteNr++)
  {
    pbtPar[szByteNr] = OddParity[pbtData[szByteNr]];
  }
}

byte_t mirror(byte_t bt)
{
  return ByteMirror[bt];
}

void mirror_bytes(byte_t *pbts, size_t szLen)
{
  size_t szByteNr;

  for (szByteNr=0; szByteNr<szLen; szByteNr++)
  {
    *pbts = ByteMirror[*pbts];
    pbts++;
  }
}

uint32_t mirror32(uint32_t ui32Bits)
{
  mirror_bytes((byte_t*)&ui32Bits,4);
  return ui32Bits;
}

uint64_t mirror64(uint64_t ui64Bits)
{
  mirror_bytes((byte_t*)&ui64Bits,8);
  return ui64Bits;
}

uint32_t swap_endian32(const void* pui32)
{
  uint32_t ui32N = *((uint32_t*)pui32);
  return (((ui32N&0xFF)<<24)+((ui32N&0xFF00)<<8)+((ui32N&0xFF0000)>>8)+((ui32N&0xFF000000)>>24));
}

uint64_t swap_endian64(const void* pui64)
{
  uint64_t ui64N = *((uint64_t *)pui64);
  return (((ui64N&0xFF)<<56)+((ui64N&0xFF00)<<40)+((ui64N&0xFF0000)<<24)+((ui64N&0xFF000000)<<8)+((ui64N&0xFF00000000ull)>>8)+((ui64N&0xFF0000000000ull)>>24)+((ui64N&0xFF000000000000ull)>>40)+((ui64N&0xFF00000000000000ull)>>56));
}

void append_iso14443a_crc(byte_t* pbtData, size_t szLen)
{
  byte_t bt;
  uint32_t wCrc = 0x6363;

  do {
    bt = *pbtData++;
    bt = (bt^(byte_t)(wCrc & 0x00FF));
    bt = (bt^(bt<<4));
    wCrc = (wCrc >> 8)^((uint32_t)bt << 8)^((uint32_t)bt<<3)^((uint32_t)bt>>4);
  } while (--szLen);

  *pbtData++ = (byte_t) (wCrc & 0xFF);
  *pbtData = (byte_t) ((wCrc >> 8) & 0xFF);
}

void print_hex(const byte_t* pbtData, const size_t szBytes)
{
  size_t szPos;

  for (szPos=0; szPos < szBytes; szPos++)
  {
    printf("%02x  ",pbtData[szPos]);
  }
  printf("\n");
}

void print_hex_bits(const byte_t* pbtData, const size_t szBits)
{
  uint8_t uRemainder;
  size_t szPos;
  size_t szBytes = szBits/8;

  for (szPos=0; szPos < szBytes; szPos++)
  {
    printf("%02x  ",pbtData[szPos]);
  }

  uRemainder = szBits % 8;
  // Print the rest bits
  if (uRemainder != 0)
  {
    if (uRemainder < 5)
      printf("%01x (%d bits)",pbtData[szBytes], uRemainder);
    else
      printf("%02x (%d bits)",pbtData[szBytes], uRemainder);
  }
  printf("\n");
}

void print_hex_par(const byte_t* pbtData, const size_t szBits, const byte_t* pbtDataPar)
{
  uint8_t uRemainder;
  size_t szPos;
  size_t szBytes = szBits/8;

  for (szPos=0; szPos < szBytes; szPos++)
  {
    printf("%02x",pbtData[szPos]);
    if (OddParity[pbtData[szPos]] != pbtDataPar[szPos])
    {
      printf("! ");
    } else {
      printf("  ");
    }
  }

  uRemainder = szBits % 8;
  // Print the rest bits, these cannot have parity bit
  if (uRemainder != 0)
  {
    if (uRemainder < 5)
      printf("%01x (%d bits)",pbtData[szBytes], uRemainder);
    else
      printf("%02x (%d bits)",pbtData[szBytes], uRemainder);
  }
  printf("\n");
}

