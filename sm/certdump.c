/* certdump.c - Dump a certificate for debugging
 *	Copyright (C) 2001 Free Software Foundation, Inc.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h> 
#include <time.h>
#include <assert.h>

#include <gcrypt.h>
#include <ksba.h>

#include "gpgsm.h"
#include "keydb.h"

static void
print_sexp (KsbaConstSexp p)
{
  unsigned long n;
  KsbaConstSexp endp;

  if (!p)
    log_printf ("none");
  else
    {
      n = strtoul (p, (char**)&endp, 10);
      p = endp;
      if (*p!=':')
        log_printf ("ERROR - invalid value");
      else
        {
          for (p++; n; n--, p++)
            log_printf ("%02X", *p);
        }
    }
}



static void
print_time (time_t t)
{

  if (!t)
    log_printf ("none");
  else if ( t == (time_t)(-1) )
    log_printf ("error");
  else
    {
      struct tm *tp;

      tp = gmtime (&t);
      log_printf ("%04d-%02d-%02d %02d:%02d:%02d",
                  1900+tp->tm_year, tp->tm_mon+1, tp->tm_mday,
                  tp->tm_hour, tp->tm_min, tp->tm_sec);
      assert (!tp->tm_isdst);
    }
}

static void
print_dn (char *p)
{
  if (!p)
    log_printf ("error");
  else
    log_printf ("`%s'", p);
}


void 
gpgsm_dump_cert (const char *text, KsbaCert cert)
{
  KsbaSexp sexp;
  unsigned char *p;
  char *dn;
  time_t t;

  log_debug ("BEGIN Certificate `%s':\n", text? text:"");
  if (cert)
    {
      sexp = ksba_cert_get_serial (cert);
      log_debug ("  serial: ");
      print_sexp (sexp);
      ksba_free (sexp);
      log_printf ("\n");

      t = ksba_cert_get_validity (cert, 0);
      log_debug ("  notBefore: ");
      print_time (t);
      log_printf ("\n");
      t = ksba_cert_get_validity (cert, 1);
      log_debug ("  notAfter: ");
      print_time (t);
      log_printf ("\n");

      dn = ksba_cert_get_issuer (cert, 0);
      log_debug ("  issuer: ");
      print_dn (dn);
      ksba_free (dn);
      log_printf ("\n");
    
      dn = ksba_cert_get_subject (cert, 0);
      log_debug ("  subject: ");
      print_dn (dn);
      ksba_free (dn);
      log_printf ("\n");

      log_debug ("  hash algo: %s\n", ksba_cert_get_digest_algo (cert));

      p = gpgsm_get_fingerprint_string (cert, 0);
      log_debug ("  SHA1 Fingerprint: %s\n", p);
      xfree (p);
    }
  log_debug ("END Certificate\n");
}
