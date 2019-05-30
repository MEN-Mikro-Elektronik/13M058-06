/****************************************************************************
 ************                                                    ************
 ************                   M58_SIMP                        ************
 ************                                                    ************
 ****************************************************************************
 *
 *       Author: see
 *
 *  Description: Simple example program for the M58 driver
 *
 *     Required: MDIS user interface library
 *     Switches: NO_MAIN_FUNC	(for systems with one namespace)
 *
 *---------------------------------------------------------------------------
 * Copyright (c) 1998-2019, MEN Mikro Elektronik GmbH
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
#include <string.h>
#include <MEN/men_typs.h>
#include <MEN/mdis_api.h>
#include <MEN/mdis_err.h>
#include <MEN/usr_oss.h>
#include <MEN/usr_utl.h>
#include <MEN/m58_drv.h>

static const char IdentString[]=MENT_XSTR(MAK_REVISION);

/*--------------------------------------+
|   DEFINES                             |
+--------------------------------------*/
/* none */

/*--------------------------------------+
|   TYPDEFS                             |
+--------------------------------------*/
/* none */

/*--------------------------------------+
|   EXTERNALS                           |
+--------------------------------------*/
/* none */

/*--------------------------------------+
|   GLOBALS                             |
+--------------------------------------*/
/* none */

/*--------------------------------------+
|   PROTOTYPES                          |
+--------------------------------------*/
static int32 m58_simple(char *arg1);
static void PrintError(char *info);

/********************************* main *************************************
 *
 *  Description: MAIN entry
 *
 *---------------------------------------------------------------------------
 *  Input......: argc, argv		command line arguments/counter
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/
int main(int argc, char **argv)
{
	if (argc < 2) {
		printf("m58_simpl <device>\n");
		return(1);
	}

	return m58_simple(argv[1]);
}


/******************************* m58_simple *********************************
 *
 *  Description:  Example (directly called in systems with one namespace)
 *
 *---------------------------------------------------------------------------
 *  Input......:  arg1..argn	arguments
 *  Output.....:  return        success (0) or error (1)
 *  Globals....:  ---
 *
 ****************************************************************************/
static int32 m58_simple(char *arg1)
{
	MDIS_PATH path;
	int32 value, gotsize, n;
	u_int8 blkbuf[2];
	char *device, buf[40];

	if (strcmp(arg1,"-?") == 0) {
		printf("m58_simpl <device>\n");
		return(1);
	}

	device = arg1;

	/*--------------------+
	|  open path          |
	+--------------------*/
	printf("open %s\n",device);

	if ((path = M_open(device)) < 0) {
		PrintError("open");
		return(1);
	}

	/*--------------------+
	|  config channels    |
	+--------------------*/
	for (n=0; n<4; n++) {
		/* set current channel */
		if ((M_setstat(path, M_MK_CH_CURRENT, n)) < 0) {
			PrintError("setstat M_MK_CH_CURRENT");
			goto abort;
		}

		/*---------------+
		| channels 0+1   |
		+---------------*/
		if (n < 2) {
			/* set direction=INPUT */
			if ((M_setstat(path, M_LL_CH_DIR, M_CH_IN)) < 0) {
				PrintError("setstat M_LL_CH_DIR");
				goto abort;
			}

			/* data storage mode=5 */
			if ((M_setstat(path, M58_DATA_MODE, 5)) < 0) {
				PrintError("setstat M58_DATA_MODE");
				goto abort;
			}

			/* enable block i/o */
			if ((M_setstat(path, M58_BUF_ENABLE, 1)) < 0) {
				PrintError("setstat M58_BUF_ENABLE");
				goto abort;
			}
		}
		/*---------------+
		| channels 2+3   |
		+---------------*/
		else {
			/* set direction=OUTPUT */
			if ((M_setstat(path, M_LL_CH_DIR, M_CH_OUT)) < 0) {
				PrintError("setstat M_LL_CH_DIR");
				goto abort;
			}
		}
	}

	/* enable interrupt */
	if ((M_setstat(path, M_MK_IRQ_ENABLE, 1)) < 0) {
		PrintError("setstat M_MK_IRQ_ENABLE");
		goto abort;
	}

	/*-------------------------------+
	|  channels 0+1: block read      |
	+-------------------------------*/
	printf("\nchannels 0+1: block read (2 bytes) ..\n");

	if ((gotsize = M_getblock(path,(u_int8*)blkbuf,2)) < 0)
		PrintError("getblock");
	else
		UTL_Memdump("read data",(char*)blkbuf,gotsize,1);

	/*-------------------------------+
	|  channels 2+3: write           |
	+-------------------------------*/
	for (value=0x22, n=2; n<4; n++) {
		/* set current channel */
		if ((M_setstat(path, M_MK_CH_CURRENT, n)) < 0) {
			PrintError("setstat M_MK_CH_CURRENT");
			goto abort;
		}

		printf("\nchannel %d: write 0x%02x = %s\n",
			   n, value, UTL_Bindump(value,8,buf));

		if (M_write(path,value) < 0)
			PrintError("write");
		else
			printf("success.\n");

		value <<= 1;
	}

	printf("\nPress key to continue\n");
	UOS_KeyWait();

	/*--------------------+
    |  cleanup            |
    +--------------------*/
	abort:
	printf("close device\n",device);
	M_close(path);

	return(0);
}

/********************************* PrintError ********************************
 *
 *  Description: Print MDIS error message
 *
 *---------------------------------------------------------------------------
 *  Input......: info	info string
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/
static void PrintError(char *info)
{
	printf("*** can't %s: %s\n", info, M_errstring(UOS_ErrnoGet()));
}

