/* errors.c  -	error strings
 *	Copyright (C) 1998 Free Software Foundation, Inc.
 *
 * This file is part of GNUPG.
 *
 * GNUPG is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GNUPG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "errors.h"

#ifndef HAVE_STRERROR
char *
strerror( int n )
{
    extern char *sys_errlist[];
    extern int sys_nerr;
    static char buf[15];

    if( n >= 0 && n < sys_nerr )
	return sys_errlist[n];
    strcpy( buf, "Unknown error" );
    return buf;
}
#endif /* !HAVE_STRERROR */

const char *
g10_errstr( int err )
{
    static char buf[50];
    const char *p;

  #define X(n,s) case G10ERR_##n : p = s; break;
    switch( err ) {
      case -1:		p = "eof"; break;
      case 0:		p = "okay"; break;
      X(GENERAL,	"General error")
      X(UNKNOWN_PACKET, "Unknown packet type")
      X(UNKNOWN_VERSION,"Unknown version")
      X(PUBKEY_ALGO    ,"Unknown pubkey algorithm")
      X(DIGEST_ALGO    ,"Unknown digest algorithm")
      X(BAD_PUBKEY     ,"Bad public key")
      X(BAD_SECKEY     ,"Bad secret key")
      X(BAD_SIGN       ,"Bad signature")
      X(CHECKSUM   ,	"Checksum error")
      X(BAD_PASS     ,	"Bad passphrase")
      X(NO_PUBKEY      ,"Public key not found")
      X(CIPHER_ALGO    ,"Unknown cipher algorithm")
      X(KEYRING_OPEN   ,"Can't open the keyring")
      X(INVALID_PACKET ,"Invalid packet")
      X(INVALID_ARMOR  ,"Invalid armor")
      X(NO_USER_ID     ,"No such user id")
      X(NO_SECKEY      ,"Secret key not available")
      X(WRONG_SECKEY   ,"Wrong secret key used")
      X(UNSUPPORTED    ,"Not supported")
      X(BAD_KEY        ,"Bad key")
      X(READ_FILE      ,"File read error")
      X(WRITE_FILE     ,"File write error")
      X(COMPR_ALGO     ,"Unknown compress algorithm")
      X(OPEN_FILE      ,"File open error")
      X(CREATE_FILE    ,"File create error")
      X(PASSPHRASE     ,"Invalid passphrase")
      X(NI_PUBKEY      ,"Unimplemented pubkey algorithm")
      X(NI_CIPHER      ,"Unimplemented cipher algorithm")
      X(SIG_CLASS      ,"Unknown signature class")
      X(TRUSTDB        ,"Trust database error")
      X(BAD_MPI        ,"Bad MPI")
      X(RESOURCE_LIMIT ,"Resource limit")
      X(INV_KEYRING    ,"Invalid keyring")
      X(BAD_CERT       ,"Bad certificate")
      X(INV_USER_ID    ,"Malformed user id")
      X(CLOSE_FILE     ,"File close error")
      X(RENAME_FILE    ,"File rename error")
      X(DELETE_FILE    ,"File delete error")
      X(UNEXPECTED     ,"Unexpected data")
      X(TIME_CONFLICT  ,"Timestamp conflict")
      X(WR_PUBKEY_ALGO ,"Unusable pubkey algorithm")
      X(FILE_EXISTS    ,"File exists")
      X(WEAK_KEY       ,"Weak key")
      default: p = buf; sprintf(buf, "g10err=%d", err); break;
    }
  #undef X
    return p;
}

