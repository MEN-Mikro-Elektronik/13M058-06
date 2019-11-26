/****************************************************************************
 ************                                                    ************
 ************                 M 5 8 _ R E A D                    ************
 ************                                                    ************
 ****************************************************************************
 *
 *       Author: see
 *
 *  Description: Configure and read M58 input ports
 *
 *     Required: usr_oss.l usr_utl.l
 *     Switches: -
 *
 *---------------------------------------------------------------------------
 * Copyright 1998-2019, MEN Mikro Elektronik GmbH
 ****************************************************************************/
/*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>

#include <MEN/men_typs.h>
#include <MEN/usr_oss.h>
#include <MEN/usr_utl.h>
#include <MEN/mdis_api.h>
#include <MEN/m58_drv.h>

static const char IdentString[]=MENT_XSTR(MAK_REVISION);

/*--------------------------------------+
|   DEFINES                             |
+--------------------------------------*/

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
	printf("Copyright 1998-2019, MEN Mikro Elektronik GmbH\n%s\n", IdentString);
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



