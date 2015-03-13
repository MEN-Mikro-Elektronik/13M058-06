/****************************************************************************
 ************                                                    ************
 ************                 M 5 8 _ R E A D                    ************
 ************                                                    ************
 ****************************************************************************
 *
 *       Author: see
 *        $Date: 2013/06/26 17:00:04 $
 *    $Revision: 1.5 $
 *
 *  Description: Configure and read M58 input ports
 *
 *     Required: usr_oss.l usr_utl.l
 *     Switches: -
 *
 *-------------------------------[ History ]---------------------------------
 *
 * $Log: m58_read.c,v $
 * Revision 1.5  2013/06/26 17:00:04  gv
 * R: 1: Porting to MDIS
 *    2: Mixing of Tabs & space characters for indentation
 * M: 1: Changed according to MDIS Porting Guide 0.9
 *    2: Cosmetics : replacement of all spaces by 1 hard Tab for indentation
 *
 * Revision 1.4  2003/06/06 13:59:41  kp
 * cosmetics
 *
 * Revision 1.3  2000/04/14 14:33:58  Schmidt
 * usage function now static
 *
 * Revision 1.2  2000/04/14 12:15:08  kp
 * cosmetics
 *
 * Revision 1.1  1998/10/01 15:54:45  see
 * Added by mcvs
 *
 *---------------------------------------------------------------------------
 * (c) Copyright 1998 by MEN mikro elektronik GmbH, Nuernberg, Germany
 ****************************************************************************/

static const char RCSid[]="$Header: /dd2/CVSR/COM/DRIVERS/MDIS_LL/M058/TOOLS/M58_READ/COM/m58_read.c,v 1.5 2013/06/26 17:00:04 gv Exp $";

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
	printf("Usage: m58_read [<opts>] <device> [<opts>]\n");
	printf("Function: Configure and read M58 channel\n");
	printf("Options:\n");
	printf("    device       device name                     [none]\n");
	printf("    -c=<chan>    channel number (0..3)           [none]\n");
	printf("    -m=<mode>    data storage mode               [none]\n");
	printf("                 0..7 = (refer to SW-Doc.)\n");
	printf("    -t=<term>    termination of channel          [none]\n");
	printf("                 0 = active\n");
	printf("                 1 = passive\n");
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
int main(int argc, char *argv[])
{
	MDIS_PATH path=0;
	int32 chan,term,mode,loopmode,value,ret,n;
	char *device,*str,*errstr,buf[40];

	/*--------------------+
	|  check arguments    |
	+--------------------*/
	if ((errstr = UTL_ILLIOPT("c=t=m=l?", buf))) {	/* check args */
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

	chan      = ((str = UTL_TSTOPT("c=")) ? atoi(str) : -1);
	term      = ((str = UTL_TSTOPT("t=")) ? atoi(str) : -1);
	mode      = ((str = UTL_TSTOPT("m=")) ? atoi(str) : -1);
	loopmode  = (UTL_TSTOPT("l") ? 1 : 0);

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
	/* channel number */
	if (chan != -1) {
		if ((M_setstat(path, M_MK_CH_CURRENT, chan)) < 0) {
			printf("*** can't setstat M_MK_CH_CURRENT: %s\n",
				   M_errstring(UOS_ErrnoGet()));
			goto abort;
		}
	}
	else {
		if ((M_getstat(path, M_MK_CH_CURRENT, &chan)) < 0) {
			printf("*** can't getstat M_MK_CH_CURRENT: %s\n",
				   M_errstring(UOS_ErrnoGet()));
			goto abort;
		}
	}

	/* channel direction */
	if ((M_setstat(path, M_LL_CH_DIR, M_CH_IN)) < 0) {
		printf("*** can't setstat M_LL_CH_DIR: %s\n",
			   M_errstring(UOS_ErrnoGet()));
		goto abort;
	}

	/* channel termination */
	if (term != -1) {
		if ((M_setstat(path, M58_PORT_TERM, term)) < 0) {
			printf("*** can't setstat M58_PORT_TERM: %s\n",
				   M_errstring(UOS_ErrnoGet()));
			goto abort;
		}
	}
	else {
		if ((M_getstat(path, M58_PORT_TERM, &term)) < 0) {
			printf("*** can't getstat M58_PORT_TERM: %s\n",
				   M_errstring(UOS_ErrnoGet()));
			goto abort;
		}
	}

	/* data storage mode */
	if (mode != -1) {
		if ((M_setstat(path, M58_DATA_MODE, mode)) < 0) {
			printf("*** can't setstat M58_DATA_MODE: %s\n",
				   M_errstring(UOS_ErrnoGet()));
			goto abort;
		}
	}
	else {
		if ((M_getstat(path, M58_DATA_MODE, &mode)) < 0) {
			printf("*** can't getstat M58_DATA_MODE: %s\n",
				   M_errstring(UOS_ErrnoGet()));
			goto abort;
		}
	}

	/*--------------------+
	|  print info         |
	+--------------------*/
	printf("channel number      : %d\n",chan);
	printf("data storage mode   : %d\n",mode);
	printf("channel termination : %s\n\n",(term==0 ? "active":"passive"));

	/*--------------------+
	|  call loop ...      |
	+--------------------*/
	do {
		if ((ret = M_read(path,&value)) < 0) {
			printf("*** can't read: %s\n",M_errstring(UOS_ErrnoGet()));
			break;
		}

	printf("read: 0x%02x = %s\n", value, UTL_Bindump(value,8,buf));

	} while(loopmode && UOS_KeyPressed() == -1);

	/*--------------------+
	|  cleanup            |
	+--------------------*/
	abort:

	if (M_close(path) < 0)
		printf("*** can't close path: %s\n",M_errstring(UOS_ErrnoGet()));

	return(0);
}



