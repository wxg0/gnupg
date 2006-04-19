/* encr-data.c -  process an encrypted data packet
 * Copyright (C) 1998, 1999, 2000, 2001, 2005,
 *               2006 Free Software Foundation, Inc.
 *
 * This file is part of GnuPG.
 *
 * GnuPG is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GnuPG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 * USA.
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "gpg.h"
#include "util.h"
#include "packet.h"
#include "cipher.h"
#include "options.h"
#include "i18n.h"


static int mdc_decode_filter( void *opaque, int control, IOBUF a,
					      byte *buf, size_t *ret_len);
static int decode_filter( void *opaque, int control, IOBUF a,
					byte *buf, size_t *ret_len);

typedef struct 
{
  gcry_cipher_hd_t cipher_hd;
  gcry_md_hd_t mdc_hash;
  char defer[20];
  int  defer_filled;
  int  eof_seen;
} decode_filter_ctx_t;


/****************
 * Decrypt the data, specified by ED with the key DEK.
 */
int
decrypt_data( void *procctx, PKT_encrypted *ed, DEK *dek )
{
    decode_filter_ctx_t dfx;
    byte *p;
    int rc=0, c, i;
    byte temp[32];
    unsigned blocksize;
    unsigned nprefix;

    memset( &dfx, 0, sizeof dfx );
    if( opt.verbose && !dek->algo_info_printed ) {
	const char *s = gcry_cipher_algo_name (dek->algo);
	if (s && *s)
	    log_info(_("%s encrypted data\n"), s );
	else
	    log_info(_("encrypted with unknown algorithm %d\n"), dek->algo );
        dek->algo_info_printed = 1;
    }
    rc = openpgp_cipher_test_algo (dek->algo);
    if (rc)
	goto leave;
    blocksize = gcry_cipher_get_algo_blklen (dek->algo);
    if( !blocksize || blocksize > 16 )
	log_fatal("unsupported blocksize %u\n", blocksize );
    nprefix = blocksize;
    if( ed->len && ed->len < (nprefix+2) )
	BUG();

    if( ed->mdc_method ) {
        if (gcry_md_open (&dfx.mdc_hash, ed->mdc_method, 0 ))
            BUG ();
	if ( DBG_HASHING )
	    gcry_md_start_debug (dfx.mdc_hash, "checkmdc");
    }

    rc = gcry_cipher_open (&dfx.cipher_hd, dek->algo,
                           GCRY_CIPHER_MODE_CFB,
                           (GCRY_CIPHER_SECURE
                            | ((ed->mdc_method || dek->algo >= 100)?
                               0 : GCRY_CIPHER_ENABLE_SYNC)));
    if (rc)
      {
        /* We should never get an error here cause we already checked
         * that the algorithm is available.  */
        BUG();
      }


    /* log_hexdump( "thekey", dek->key, dek->keylen );*/
    rc = gcry_cipher_setkey (dfx.cipher_hd, dek->key, dek->keylen);
    if ( gpg_err_code (rc) == GPG_ERR_WEAK_KEY )
      {
	log_info(_("WARNING: message was encrypted with"
		   " a weak key in the symmetric cipher.\n"));
	rc=0;
      }
    else if( rc )
      {
	log_error("key setup failed: %s\n", g10_errstr(rc) );
	goto leave;
      
      }
    if (!ed->buf) {
        log_error(_("problem handling encrypted packet\n"));
        goto leave;
    }

    gcry_cipher_setiv (dfx.cipher_hd, NULL, 0);

    if( ed->len ) {
	for(i=0; i < (nprefix+2) && ed->len; i++, ed->len-- ) {
	    if( (c=iobuf_get(ed->buf)) == -1 )
		break;
	    else
		temp[i] = c;
	}
    }
    else {
	for(i=0; i < (nprefix+2); i++ )
	    if( (c=iobuf_get(ed->buf)) == -1 )
		break;
	    else
		temp[i] = c;
    }

    gcry_cipher_decrypt (dfx.cipher_hd, temp, nprefix+2, NULL, 0);
    gcry_cipher_sync (dfx.cipher_hd);
    p = temp;
/* log_hexdump( "prefix", temp, nprefix+2 ); */
    if(dek->symmetric
       && (p[nprefix-2] != p[nprefix] || p[nprefix-1] != p[nprefix+1]) )
      {
	rc = GPG_ERR_BAD_KEY;
	goto leave;
      }

    if( dfx.mdc_hash )
	gcry_md_write (dfx.mdc_hash, temp, nprefix+2);

    if( ed->mdc_method )
	iobuf_push_filter( ed->buf, mdc_decode_filter, &dfx );
    else
	iobuf_push_filter( ed->buf, decode_filter, &dfx );

    proc_packets( procctx, ed->buf );
    ed->buf = NULL;
    if( ed->mdc_method && dfx.eof_seen == 2 )
	rc = gpg_error (GPG_ERR_INV_PACKET);
    else if( ed->mdc_method ) { /* check the mdc */
	int datalen = gcry_md_get_algo_dlen (ed->mdc_method);

	gcry_cipher_decrypt (dfx.cipher_hd, dfx.defer, 20, NULL, 0);
	gcry_md_final (dfx.mdc_hash);
	if (datalen != 20
	    || memcmp (gcry_md_read( dfx.mdc_hash, 0 ), dfx.defer, datalen) )
          rc = gpg_error (GPG_ERR_BAD_SIGNATURE);
	/*log_hexdump("MDC calculated:", md_read( dfx.mdc_hash, 0), datalen);*/
	/*log_hexdump("MDC message   :", dfx.defer, 20);*/
    }
    

  leave:
    gcry_cipher_close (dfx.cipher_hd);
    gcry_md_close (dfx.mdc_hash);
    return rc;
}



/* I think we should merge this with cipher_filter */
static int
mdc_decode_filter( void *opaque, int control, IOBUF a,
					      byte *buf, size_t *ret_len)
{
    decode_filter_ctx_t *dfx = opaque;
    size_t n, size = *ret_len;
    int rc = 0;
    int c;

    if( control == IOBUFCTRL_UNDERFLOW && dfx->eof_seen ) {
	*ret_len = 0;
	rc = -1;
    }
    else if( control == IOBUFCTRL_UNDERFLOW ) {
	assert(a);
	assert( size > 40 );

	/* get at least 20 bytes and put it somewhere ahead in the buffer */
	for(n=20; n < 40 ; n++ ) {
	    if( (c = iobuf_get(a)) == -1 )
		break;
	    buf[n] = c;
	}
	if( n == 40 ) {
	    /* we have enough stuff - flush the deferred stuff */
	    /* (we have asserted that the buffer is large enough) */
	    if( !dfx->defer_filled ) { /* the first time */
		memcpy(buf, buf+20, 20 );
		n = 20;
	    }
	    else {
		memcpy(buf, dfx->defer, 20 );
	    }
	    /* now fill up */
	    for(; n < size; n++ ) {
		if( (c = iobuf_get(a)) == -1 )
		    break;
		buf[n] = c;
	    }
	    /* move the last 20 bytes back to the defer buffer */
	    /* (okay, we are wasting 20 bytes of supplied buffer) */
	    n -= 20;
	    memcpy( dfx->defer, buf+n, 20 );
	    dfx->defer_filled = 1;
	}
	else if( !dfx->defer_filled ) { /* eof seen buf empty defer */
	    /* this is bad because there is an incomplete hash */
	    n -= 20;
	    memcpy(buf, buf+20, n );
	    dfx->eof_seen = 2; /* eof with incomplete hash */
	}
	else { /* eof seen */
	    memcpy(buf, dfx->defer, 20 );
	    n -= 20;
	    memcpy( dfx->defer, buf+n, 20 );
	    dfx->eof_seen = 1; /* normal eof */
	}

	if( n ) {
	    gcry_cipher_decrypt (dfx->cipher_hd, buf, n, NULL, 0);
	    gcry_md_write (dfx->mdc_hash, buf, n);
	}
	else {
	    assert( dfx->eof_seen );
	    rc = -1; /* eof */
	}
	*ret_len = n;
    }
    else if( control == IOBUFCTRL_DESC ) {
	*(char**)buf = "mdc_decode_filter";
    }
    return rc;
}

static int
decode_filter( void *opaque, int control, IOBUF a, byte *buf, size_t *ret_len)
{
    decode_filter_ctx_t *fc = opaque;
    size_t n, size = *ret_len;
    int rc = 0;

    if( control == IOBUFCTRL_UNDERFLOW ) {
	assert(a);
	n = iobuf_read( a, buf, size );
	if( n == -1 ) n = 0;
	if( n )
	    gcry_cipher_decrypt (fc->cipher_hd, buf, n, NULL, 0);
	else
	    rc = -1; /* eof */
	*ret_len = n;
    }
    else if( control == IOBUFCTRL_DESC ) {
	*(char**)buf = "decode_filter";
    }
    return rc;
}

