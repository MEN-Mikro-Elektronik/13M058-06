/****************************************************************************
 ************                                                    ************
 ************                 M 5 8 _ W R I T E                  ************
 ************                                                    ************
 ****************************************************************************
 *
 *       Author: see
 *        $Date: 2013/06/26 16:53:44 $
 *    $Revision: 1.5 $
 *
 *  Description: Configure and write M58 out ports
 *
 *     Required: usr_oss.l usr_utl.l
 *     Switches: -
 *
 *-------------------------------[ History ]---------------------------------
 *
 * $Log: m58_write.c,v $
 * Revision 1.5  2013/06/26 16:53:44  gv
 * R: 1: Porting to MDIS
 *    2: Mixing of Tabs & space characters for indentation
 * M: 1: Changed according to MDIS Porting Guide 0.9
 *    2: Cosmetics : replacement of all spaces by 1 hard Tab for indentation
 *
 * Revision 1.4  2003/06/06 13:59:44  kp
 * cosmetics
 *
 * Revision 1.3  2000/04/14 14:34:01  Schmidt
 * usage function now static
 *
 * Revision 1.2  1998/10/05 18:08:40  Schmidt
 * main: unreferenced local variable 'ret' removed
 *
 * Revision 1.1  1998/10/01 15:54:49  see
 * Added by mcvs
 *
 *---------------------------------------------------------------------------
 * (c) Copyright 1998 by MEN mikro elektronik GmbH, Nuernberg, Germany
 ****************************************************************************/

static const char RCSid[]="$Header: /dd2/CVSR/COM/DRIVERS/MDIS_LL/M058/TOOLS/M58_WRITE/COM/m58_write.c,v 1.5 2013/06/26 16:53:44 gv Exp $";

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
	printf("Usage: m58_write [<opts>] <device> <value> [<opts>]\n");
	printf("Function: Configure and write M58 channel\n");
	printf("Options:\n");
	printf("    device       device name                     [none]\n");
	printf("    value        value to write (hex)            [none]\n");
	printf("    -c=<chan>    channel number (0..3)           [none]\n");
	printf("    -w           wait for key before exit\n");
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
int main(int argc,char **argv)
{
	MDIS_PATH path=0;
	int32 chan,waitkey,value,n;
	char *device,*str,*errstr,buf[40];

	/*--------------------+
	|  check arguments    |
	+--------------------*/
	if ((errstr = UTL_ILLIOPT("c=w?", buf))) {	/* check args */
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
	for (device=NULL, value=-1, n=1; n<argc; n++)
		if (*argv[n] != '-') {
			if (device==NULL)
				device = argv[n];
			else {
				value = UTL_Atox(argv[n]);
				break;
			}
		}

	if (!device || value==-1) {
		usage();
		return(1);
	}

	chan      = ((str = UTL_TSTOPT("c=")) ? atoi(str) : -1);
	waitkey   = (UTL_TSTOPT("w") ? 1 : 0);

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
	if ((M_setstat(path, M_LL_CH_DIR, M_CH_OUT)) < 0) {
		printf("*** can't setstat M_LL_CH_DIR: %s\n",
			   M_errstring(UOS_ErrnoGet()));
		goto abort;
	}

	/*--------------------+
	|  print info         |
	+--------------------*/
	printf("channel number      : %d\n",chan);

	/*--------------------+
	|  write channel      |
	+--------------------*/
	if (M_write(path,value) < 0)
		printf("*** can't write: %s\n",M_errstring(UOS_ErrnoGet()));

	printf("\nwrite: 0x%02x = %s\n", value, UTL_Bindump(value,8,buf));

	if (waitkey) {
		printf("\nPress key to continue\n");
		UOS_KeyWait();
	}

	/*--------------------+
	|  cleanup            |
	+--------------------*/
	abort:

	if (M_close(path) < 0)
		printf("*** can't close path: %s\n",M_errstring(UOS_ErrnoGet()));

	return(0);
}
