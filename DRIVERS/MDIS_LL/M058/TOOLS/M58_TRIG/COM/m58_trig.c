/****************************************************************************
 ************                                                    ************
 ************                 M 5 8 _ T R I G                    ************
 ************                                                    ************
 ****************************************************************************
 *
 *       Author: see
 *        $Date: 2013/06/26 16:53:50 $
 *    $Revision: 1.5 $
 *
 *  Description: Wait for M58 trigger signals
 *
 *     Required: usr_oss.l usr_utl.l
 *     Switches: -
 *
 *-------------------------------[ History ]---------------------------------
 *
 * $Log: m58_trig.c,v $
 * Revision 1.5  2013/06/26 16:53:50  gv
 * R: 1: Porting to MDIS
 *    2: Mixing of Tabs & space characters for indentation
 * M: 1: Changed according to MDIS Porting Guide 0.9
 *    2: Cosmetics : replacement of all spaces by 1 hard Tab for indentation
 *
 * Revision 1.4  2003/06/06 13:59:50  kp
 * cosmetics
 *
 * Revision 1.3  2000/04/14 14:34:08  Schmidt
 * usage and SigHandler now static
 *
 * Revision 1.2  1998/10/05 18:08:53  Schmidt
 * SigHandler: UOS_SigMask() removed
 * main: unreferenced local variable 'value' and 'ret' removed
 *
 * Revision 1.1  1998/10/01 15:55:06  see
 * Added by mcvs
 *
 *---------------------------------------------------------------------------
 * (c) Copyright 1998 by MEN mikro elektronik GmbH, Nuernberg, Germany
 ****************************************************************************/

static const char RCSid[]="$Header: /dd2/CVSR/COM/DRIVERS/MDIS_LL/M058/TOOLS/M58_TRIG/COM/m58_trig.c,v 1.5 2013/06/26 16:53:50 gv Exp $";

#include <stdio.h>
#include <stdlib.h>

#include <MEN/men_typs.h>
#include <MEN/usr_oss.h>
#include <MEN/usr_utl.h>
#include <MEN/mdis_api.h>
#include <MEN/m58_drv.h>

/*--------------------------------------+
|   DEFINES                             |
+--------------------------------------*/
#define REV "V1.0"      /* program revision */

/*--------------------------------------+
|   PROTOTYPES                          |
+--------------------------------------*/
static void usage(void);
static void __MAPILIB SigHandler(u_int32 sigCode);

/********************************* SigHandler *******************************
 *
 *  Description: Signal handler
 *
 *---------------------------------------------------------------------------
 *  Input......: sigCode	signal code received
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/

static void __MAPILIB SigHandler(u_int32 sigCode)
{
	switch(sigCode) {
		case UOS_SIG_USR1:
			printf(">>> TRIGGER occured\n");
			break;
		default:
			printf(">>> signal=%d received\n",sigCode);
	}
}

/********************************* usage ************************************
 *
 *  Description: Print program usage
 *
 *---------------------------------------------------------------------------
 *  Input......: -
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/
static void usage(void)
{
	printf("Usage: m58_trig [<opts>] <device> [<opts>]\n");
	printf("Function: Wait for M58 trigger signals\n");
	printf("Options:\n");
	printf("    device       device name       [none]\n");
	printf("    -e=<edge>    trigger edge      [none]\n");
	printf("                 0 = falling\n");
	printf("                 1 = rising\n");
	printf("    -l           loop mode\n");
	printf("\n");
	printf("(c) 1998 by MEN mikro elektronik GmbH, %s\n\n",REV);
}

/********************************* main *************************************
 *
 *  Description: Program main function
 *
 *---------------------------------------------------------------------------
 *  Input......: argc,argv	argument counter, data ..
 *  Output.....: return	    success (0) or error (1)
 *  Globals....: -
 ****************************************************************************/
int main(int argc,char *argv[])
{
	MDIS_PATH path=0;
	int32 edge,loopmode,error,n;
	char *device,*str,*errstr,buf[40];

	/*--------------------+
	|  check arguments    |
	+--------------------*/
	if ((errstr = UTL_ILLIOPT("s=e=l?", buf))) {	/* check args */
		printf("*** %s\n", errstr);
		return(1);
	}

	if (UTL_TSTOPT("?")) {						/* help requested ? */
		usage();
		return(1);
	}

	/*--------------------+
	|  get arguments      |
	+--------------------*/
	for (device=NULL, n=1; n<argc; n++)
		if (*argv[n] != '-') {
			device = argv[n];
			break;
		}

	if (!device) {
		usage();
		return(1);
	}

	edge      = ((str = UTL_TSTOPT("e=")) ? atoi(str) : -1);
	loopmode  = (UTL_TSTOPT("l") ? 1 : 0);

	/*--------------------+
	|  install signal     |
	+--------------------*/
	/* install signal handler */
	if ((error = UOS_SigInit(SigHandler))) {
		printf("*** can't UOS_SigInit: error=0x%04x\n",error);
		return(1);
	}

	/* install signals */
	if ((error = UOS_SigInstall(UOS_SIG_USR1))) {
		printf("*** can't UOS_SigInstall: error=0x%04x\n",error);
		goto abort;
	}

	/*--------------------+
	|  open path          |
	|  set current chan   |
	+--------------------*/
	if ((path = M_open(device)) < 0) {
		printf("*** can't open path: %s\n",M_errstring(UOS_ErrnoGet()));
		return(1);
	}

	/*--------------------+
	|  config             |
	+--------------------*/
	/* trigger edge */
	if (edge != -1) {
		if ((M_setstat(path, M58_TRIG_EDGE, edge)) < 0) {
			printf("*** can't setstat M58_TRIG_EDGE: %s\n",
				   M_errstring(UOS_ErrnoGet()));
			goto abort;
		}
	}
	else {
		if ((M_getstat(path, M58_TRIG_EDGE, &edge)) < 0) {
			printf("*** can't getstat M58_TRIG_EDGE: %s\n",
				   M_errstring(UOS_ErrnoGet()));
			goto abort;
		}
	}

	/* enable interrupts */
	if ((M_setstat(path, M_MK_IRQ_ENABLE, 1)) < 0) {
		printf("*** can't setstat M_MK_IRQ_ENABLE: %s\n",
			   M_errstring(UOS_ErrnoGet()));
		goto abort;
	}

	/* enable trigger signal */
	if ((M_setstat(path, M58_TRIG_SIG_SET, UOS_SIG_USR1)) < 0) {
		printf("*** can't setstat M58_TRIG_SIG_SET: %s\n",
			   M_errstring(UOS_ErrnoGet()));
		goto abort;
	}

	/*--------------------+
	|  print info         |
	+--------------------*/
	printf("installed signal : %d\n",UOS_SIG_USR1);
	printf("trigger edge     : %s\n\n",(edge==0 ? "falling":"rising"));

	/*--------------------+
	|  wait loop ...      |
	+--------------------*/
	printf("wait for trigger signals ..\n");

	do {
		/* wait some time and check for key pressed */
		UOS_Delay(10);
	} while(loopmode && UOS_KeyPressed() == -1);

	/*--------------------+
	|  cleanup            |
	+--------------------*/
	abort:

	/* disable trigger signal */
	if ((M_setstat(path, M58_TRIG_SIG_CLR, 0)) < 0) {
		printf("*** can't setstat M58_TRIG_SIG_CLR: %s\n",
			   M_errstring(UOS_ErrnoGet()));
		goto abort;
	}

	/* close device */
	if (M_close(path) < 0)
		printf("*** can't close path: %s\n",M_errstring(UOS_ErrnoGet()));

	/* terminate signal handling */
	UOS_SigExit();

	return(0);
}

