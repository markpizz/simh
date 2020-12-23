/* s3_cd.c: IBM 1442 card reader/punch

   Copyright (c) 2001 Charles E. Owen
   Copyright (c) 1993-2001, Robert M. Supnik

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   ROBERT M SUPNIK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of Robert M Supnik shall not
   be used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Robert M Supnik.

   cdr      card reader
   cdp      card punch
   cdp2     card punch stacker 2

   Normally, cards are represented as ASCII text streams terminated by newlines.
   This allows cards to be created and edited as normal files.  Set the EBCDIC
   flag on the card unit allows cards to be read or punched in EBCDIC format,
   suitable for binary data.

*/

#include "s3_defs.h"

int32 s1sel, s2sel;
char rbuf[CBUFSIZE];               /* > CDR_WIDTH */
t_stat cdr_svc (UNIT *uptr);
t_stat cdr_boot (int32 unitno, DEVICE *dptr);
t_stat cdr_attach (UNIT *uptr, CONST char *cptr);
t_stat cd_reset (DEVICE *dptr);
const char *cd_description (DEVICE *dptr);

static t_stat read_card (int32 ilnt, int32 mod);
static t_stat punch_card (int32 ilnt, int32 mod);

int32 DAR;            /* Data address register */
int32 LCR;            /* Length Count Register */
int32 lastcard = 0;   /* Last card switch */
int32 carderr = 0;    /* Error switch */
int32 pcherror = 0;   /* Punch error */
int32 notready = 0;   /* Not ready error */
int32 cdr_ebcdic = 0; /* 1 = EBCDIC mode on reader */
int32 cdp_ebcdic = 1; /* 1 = EBCDIC mode on punch */

/* Card reader data structures

   cdr_dev   CDR descriptor
   cdr_unit   CDR unit descriptor
   cdr_reg   CDR register list
*/

UNIT cdr_unit = {
   UDATA (&cdr_svc, UNIT_SEQ+UNIT_ATTABLE, 0), 100 };

REG cdr_reg[] = {
   { FLDATA (LAST, lastcard, 0) },
   { FLDATA (ERR, carderr, 0) },
   { FLDATA (NOTRDY, notready, 0) },
   { HRDATA (DAR, DAR, 16) },
   { HRDATA (LCR, LCR, 16) },
   { FLDATA (EBCDIC, cdr_ebcdic, 0) },
   { FLDATA (S2, s2sel, 0) },
   { DRDATA (POS, cdr_unit.pos, T_ADDR_W), PV_LEFT },
   { DRDATA (TIME, cdr_unit.wait, 24), PV_LEFT },
   { BRDATA (BUF, rbuf, 8, 8, sizeof(rbuf)) },
   { NULL } };

DEVICE cdr_dev = {
   "CDR", &cdr_unit, cdr_reg, NULL,
   1, 10, 31, 1, 8, 7,
   NULL, NULL, &cd_reset,
   &cdr_boot, &cdr_attach, NULL, NULL,
   DEV_M10|DEV_DIS|DEV_DISABLE, 0, NULL, NULL, NULL, NULL, NULL, NULL, &cd_description };

/* CDP data structures

   cdp_dev   CDP device descriptor
   cdp_unit   CDP unit descriptor
   cdp_reg   CDP register list
*/

UNIT cdp_unit = {
   UDATA (NULL, UNIT_SEQ+UNIT_ATTABLE, 0) };

REG cdp_reg[] = {
   { FLDATA (ERR, pcherror, 0) },
   { FLDATA (EBCDIC, cdp_ebcdic, 0) },
   { FLDATA (S2, s2sel, 0) },
   { FLDATA (NOTRDY, notready, 0) },
   { HRDATA (DAR, DAR, 16) },
   { HRDATA (LCR, LCR, 16) },
   { DRDATA (POS, cdp_unit.pos, T_ADDR_W), PV_LEFT },
   { NULL } };

DEVICE cdp_dev = {
   "CDP", &cdp_unit, cdp_reg, NULL,
   1, 10, 31, 1, 8, 7,
   NULL, NULL, &cd_reset,
   NULL, NULL, NULL, NULL,
   DEV_M10|DEV_DIS, 0, NULL, NULL, NULL, NULL, NULL, NULL, &cd_description };

/* Stacker data structures

   stack_dev   STACK device descriptor
   stack_unit   STACK unit descriptors
   stack_reg   STACK register list
*/

UNIT stack_unit[] = {
   { UDATA (NULL, UNIT_SEQ+UNIT_ATTABLE, 0) } };

REG stack_reg[] = {
   { DRDATA (POS0, stack_unit[0].pos, 31), PV_LEFT },
   { NULL } };

DEVICE stack_dev = {
   "CDP2", stack_unit, stack_reg, NULL,
   1, 10, 31, 1, 8, 7,
   NULL, NULL, &cd_reset,
   NULL, NULL, NULL, NULL,
   DEV_M10|DEV_DIS, 0, NULL, NULL, NULL, NULL, NULL, NULL, &cd_description };


/* -------------------------------------------------------------------- */

/* 1442: master routine */

int32 crd (int32 op, int32 m, int32 n, int32 data)
{
   int32 iodata;
   switch (op) {
      case 0:     /* SIO 1442 */
         /* if (n == 1)
            return STOP_IBKPT; */
         switch (data) {               /* Select atacker */
            case 0x00:
               break;
            case 0x01:
               s2sel = 1;
               break;
            default:
               break;
         }
         switch (n) {
            case 0x00:   /* Feed */
               iodata = SCPE_OK;
               break;
            case 0x01:   /* Read only */
               if (cdr_ebcdic)
                  iodata = read_card(0, 1);
               else
                  iodata = read_card(0, 0);
               break;
            case 0x02:   /* Punch and feed */
               iodata = punch_card(0, 0);
               break;
            case 0x03:   /* Read Col Binary */
               iodata = read_card(0, 1);
               break;
            case 0x04:   /* Punch no feed */
               iodata = punch_card(0, 1);
               break;
            default:
               return STOP_INVDEV;
         }
         return iodata;

      case 1:     /* LIO 1442 */
         switch (n) {
            case 0x00:   /* Load LCR */
               LCR = data & 0xffff;
               break;
            case 0x04:
               DAR = data & 0xffff;
               break;
            default:
               return STOP_INVDEV;
         }
         return SCPE_OK;

      case 2:     /* TIO 1442 */
      case 4:     /* APL 1442 */
         iodata = 0;
         switch (n) {
            case 0x00:   /* Error */
               if (carderr || pcherror || notready)
                  iodata = 1;
               if ((cdr_unit.flags & UNIT_ATT) == 0)
                  iodata = 1;    /* attached? */
               break;
            case 0x02:   /* Busy */
               if (sim_is_active (&cdr_unit))
                  iodata = 1;
               break;
            default:
               return (STOP_INVDEV << 16);
         }
         return ((SCPE_OK << 16) | iodata);

      case 3:     /* SNS 1442 */
         iodata = 0;
         switch (n) {
            case 0x01:
               break;
            case 0x02:
               break;
            case 0x03:
               if (carderr)
                  iodata |= 0x80;
               if (lastcard)
                  iodata |= 0x40;
               if (pcherror)
                  iodata |= 0x20;
               if ((cdr_unit.flags & UNIT_ATT) == 0)
                  iodata |= 0x08;
               if (notready)
                  iodata |= 0x08;
               break;
            case 0x04:
               iodata = DAR;
               break;
            default:
               return (STOP_INVDEV << 16);
         }
         iodata |= ((SCPE_OK << 16) & 0xffff0000);
         return (iodata);

   }
   return SCPE_IERR;
}


/* Card read routine
   mod 0 = ASCII read
   mod 1 = EBCDIC read
*/

static t_stat read_card (int32 ilnt, int32 mod)
{
int32 i;
t_stat r;

if (sim_is_active (&cdr_unit)) {            /* busy ? */
   sim_cancel (&cdr_unit);                  /* cancel */
   if (r = cdr_svc (&cdr_unit)) return r;   /* process */
}

if (((cdp_unit.flags & UNIT_ATT) != 0 ||
   (stack_unit[0].flags & UNIT_ATT) != 0) && /* Punch is attached and */
   (cdr_unit.flags & UNIT_ATT) == 0) {      /* reader is not --- */
      for (i = 0; i < 80; i++) {            /* Assume blank cards in hopper */
         PutMem(DAR, 0, 0x40);
         DAR++;
      }
      sim_activate (&cdr_unit, cdr_unit.wait);      /* activate */
      return SCPE_OK;
}

if ((cdr_unit.flags & UNIT_ATT) == 0)               /* attached? */
   return SCPE_UNATT;

lastcard = carderr = notready = s1sel = s2sel = 0;  /* default stacker */

for (i = 0; i < CBUFSIZE; i++) rbuf[i] = 0x20;      /* clear buffer */
if (mod) {
   for (i = 0; i < 80; i++) {
      rbuf[i] = fgetc(cdr_unit.fileref);            /* Read EBCDIC */
   }
} else {
   if (fgets (rbuf, CBUFSIZE, cdr_unit.fileref)) {};/* read Ascii */
}
if (feof (cdr_unit.fileref)) {                      /* eof ? */
   notready = 1;
   return STOP_NOCD;
}
if (ferror (cdr_unit.fileref)) {         /* error? */
   sim_perror ("Card reader I/O error");
   clearerr (cdr_unit.fileref);
   carderr = 1;
   return SCPE_OK;
}
cdr_unit.pos = ftell (cdr_unit.fileref); /* update position */
i = getc (cdr_unit.fileref);             /* see if more */
if (feof (cdr_unit.fileref))             /* eof? set flag */
   lastcard = 1;
fseek (cdr_unit.fileref, cdr_unit.pos, SEEK_SET);
for (i = 0; i < 80; i++) {
   if (mod == 0) {                       /* If ASCII mode... */
      if (rbuf[i] == '\n' ||             /* remove ASCII CR/LF */
         rbuf[i] == '\r' ||
         rbuf[i] == 0x00)
            rbuf[i] = ' ';
      rbuf[i] = ascii_to_ebcdic[rbuf[i]];   /* convert to EBCDIC */
   }
   PutMem(DAR, 0, rbuf[i]);              /* Copy to main memory */
   DAR++;
}
sim_activate (&cdr_unit, cdr_unit.wait); /* activate */
return SCPE_OK;
}

/* Card reader service.  If a stacker select is active, copy to the
   selected stacker.  Otherwise, copy to the normal stacker.  If the
   unit is unattached, simply exit.
*/

t_stat cdr_svc (UNIT *uptr)
{
   int32 i;

   if (s2sel)                            /* stacker 1 ? */
      uptr = &stack_unit[0];
   else
      uptr = &stack_unit[0];             /* then default */
   if ((uptr -> flags & UNIT_ATT) == 0) return SCPE_OK; /* attached ? */
   for (i = 0; i < CDR_WIDTH; i++)
      rbuf[i] = ebcdic_to_ascii[rbuf[i]];
   for (i = CDR_WIDTH - 1; (i >= 0) && (rbuf[i] == ' '); i--) rbuf[i] = 0;
   rbuf[CDR_WIDTH] = 0;                  /* null at end */

   fputs (rbuf, uptr -> fileref);        /* write card */
   fputc ('\n', uptr -> fileref);        /* plus new line */
   if (ferror (uptr -> fileref)) {       /* error ? */
      sim_perror ("Card stacker I/O error");
      clearerr (uptr -> fileref);
}
   uptr -> pos = ftell(uptr -> fileref); /* update position */
   return SCPE_OK;
}

/* Card punch routine

   mod: not used
*/

static t_stat punch_card (int32 ilnt, int32 mod)
{
   int32 i, colcount;
   static char pbuf[CDP_WIDTH + 1];      /* + null */
   UNIT *uptr;

   if (s2sel)                            /* stack 2 ? */
      uptr = &stack_unit[0];
   else
      uptr = &cdp_unit;                  /* normal output */
   if ((uptr -> flags & UNIT_ATT) == 0) {   /* Attached? */
      notready = 1;
      return SCPE_OK;
   }
   pcherror = s1sel = notready = 0;      /* clear flags */

   colcount = 128 - LCR;
   for (i = 0; i < colcount; i++) {      /* Fetch data */
      if (cdp_ebcdic)
         pbuf[i] = GetMem(DAR, 0) & 0xff;
      else
         pbuf[i] = ebcdic_to_ascii[GetMem(DAR, 0)];
      DAR++;
   }
   for (i = CDP_WIDTH - 1; (i >= 0) && (pbuf[i] == ' '); i--) pbuf[i] = 0;

   pbuf[CDP_WIDTH] = 0;                  /* trailing null */
   if (!cdp_ebcdic) {
      fputs (pbuf, uptr -> fileref);     /* output card */
      fputc ('\n', uptr -> fileref);     /* plus new line */
   } else {
      for (i = 0; i < 80; i++) {
         fputc(pbuf[i], uptr -> fileref);
      }
   }
   if (ferror (uptr -> fileref)) {       /* error  */
      sim_perror ("Card punch I/O error");
      clearerr (uptr -> fileref);
      pcherror = 1;
   }
   uptr -> pos = ftell (uptr -> fileref);  /* update position */
   return SCPE_OK;
}

/* Select stack routine

   Modifiers have been checked by the caller
   Modifiers are 1, 2, for the respective stack
*/

t_stat select_stack (int32 ilnt, int32 mod)
{
   if (mod == 1)
      s1sel = 1;
   else
      if (mod == 2)
         s2sel = 1;
   return SCPE_OK;
}

static void cd_set_dev (const char *name, t_bool enable)
{
   DEVICE *dptr = find_dev (name);

   if (dptr != NULL) {
      if (enable) {
         dptr->flags &= ~DEV_DIS;
      }
      else {
         dptr->flags |= DEV_DIS;
         sim_cancel(dptr->units);
      }
   }
}

/*** Card reader/punch reset ***/

t_stat cd_reset (DEVICE *dptr)
{
   lastcard = carderr = 0;               /* clear indicators */
   notready = pcherror = 0;              /* clear indicators */
   s1sel = s2sel = 0;                    /* clear stacker sel */
   sim_cancel (&cdr_unit);               /* clear reader event */
   if (0 == strcmp("CDR", dptr->name)) {
      if (cdr_dev.flags & DEV_DIS) {
         if ((cdp_dev.flags & DEV_DIS) == 0) {
            cd_set_dev("CDP",  FALSE);
            cd_set_dev("CDP2", FALSE);
            cd_set_dev("PCH",  TRUE);
            cd_set_dev("SCH",  TRUE);
            cd_set_dev("STK1", TRUE);
            cd_set_dev("STK2", TRUE);
            cd_set_dev("STK3", TRUE);
            cd_set_dev("STK4", TRUE);
         }
      }
      else {
         if ((cdp_dev.flags & DEV_DIS) != 0) {
            cd_set_dev("CDP",  TRUE);
            cd_set_dev("CDP2", TRUE);
            cd_set_dev("PCH",  FALSE);
            cd_set_dev("SCH",  FALSE);
            cd_set_dev("STK1", FALSE);
            cd_set_dev("STK2", FALSE);
            cd_set_dev("STK3", FALSE);
            cd_set_dev("STK4", FALSE);
         }
       }
   }
   return SCPE_OK;
}

/*** Card reader attach ***/

t_stat cdr_attach (UNIT *uptr, CONST char *cptr)
{
   carderr = lastcard = notready = 0;    /* clear last card */
   return attach_unit (uptr, cptr);
}

/*** Bootstrap routine ***/

t_stat cdr_boot (int32 unitno, DEVICE *dptr)
{
   cdr_ebcdic = 1;
   cdp_ebcdic = 1;
   DAR = 0;
   LCR = 80;
   read_card(0, 1);
   return SCPE_OK;
}

const char *cd_description (DEVICE *dptr)
{
   if (0 == strcmp("CDR", dptr->name))
       return "IBM 1442 Card Reader";
   if (0 == strcmp("CDP", dptr->name))
       return "IBM 1442 Card Punch";
   return "IBM 1442 Card Stacker 2";
}
