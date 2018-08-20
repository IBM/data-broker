/*
 * Copyright © 2018 IBM Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef CRC16_H_
#define CRC16_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "errno.h"

#include "logutil.h"
#include "definitions.h"

#define POLY 0x1021
#define MASK 0x80

static inline
int crcremainder( const char * message, const uint16_t size ) {
    char * paddedmsg;
    char pad[2] = {'\0','\0'};
    unsigned int remain = 0x0000;
    unsigned int  mask;
    unsigned char byte;
    int len = size;

    if(( message == NULL ) || ( size == 0 ))
      return -EINVAL;

    if( len > DBBE_REDIS_MAX_KEY_LEN )
      return -ENAMETOOLONG;

    paddedmsg = malloc(len + 2);
    memcpy(paddedmsg, message, len);
    memcpy(&paddedmsg[len], pad, 2);
    len += 2;

    LOG( DBG_TRACE, stderr, "len=%d\n", len);
    int i;
    for ( i=0; i<len; i++)
      LOG( DBG_TRACE, stderr,"c=%x\n", paddedmsg[i]);

    for ( i=0; i<len; i++) {
        // get next byte
        byte =  paddedmsg[i];
        LOG( DBG_TRACE, stderr, "byte = %d or %x\n", byte, byte);
        mask = MASK;
        while (mask > 0) {
            remain <<= 1;
            // looking for next 1 bit
            if (byte & mask) remain += 1;
            mask >>= 1;

            // crc16, only want 16bits
            if (remain > 0xffff) {
                // remove any overflow
                remain &= 0xffff;

                // xor last remain with the POLY to get next remain
                remain ^= POLY;
            }
        }
    }
    free( paddedmsg );
    return remain;
}

#endif // CRC16_H_
