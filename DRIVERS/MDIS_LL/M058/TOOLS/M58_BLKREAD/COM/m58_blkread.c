/****************************************************************************
 ************                                                    ************
 ************                 M 5 8 _ B L K R E A D              ************
 ************                                                    ************
 ****************************************************************************
 *
 *       Author: see
 *
 *  Description: Configure and read M58 input ports (blockwise)
 *
 *     Required: usr_oss.l usr_utl.l
 *     Switches: -
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
	printf("Usage: m58_blkread [<opts>] <device> [<opts>]\n");
	printf("Function: Configure and read M58 channels (blockwise)\n");
	printf("Options:\n");
	printf("    device       device name                          [none]\n");
	printf("    -s=<size>    block size                           [128]\n");
	printf("    -b=<mode>    block i/o mode                       [0]\n");
	printf("                 0 = M_BUF_USRCTRL\n");
	printf("                 1 = M_BUF_CURRBUF\n");
	printf("                 2 = M_BUF_RINGBUF\n");
	printf("                 3 = M_BUF_RINGBUF_OVERWR\n");
	printf("    -<i>=<enb>   channel i buffering enable           [none]\n");
	printf("                 0 = disable block i/o, leave direction\n");
	printf("                 1 = enable  block i/o, set input direction\n");
	printf("    -e=<edge>    trigger edge      [none]\n");
	printf("                 0 = falling\n");
	printf("                 1 = rising\n");
	printf("    -m=<mode>    data storage mode                    [none]\n");
	printf("                 0..7 = (refer to SW-Doc.)\n");
	printf("    -t=<msec>    block read timeout [msec] (0=none)   [0]\n");
	printf("    -l           loop mode\n");
	printf("\n");
	printf("Copyright (c) 1998-2019, MEN Mikro Elektronik GmbH\n%s\n", IdentString);
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
	int32 blksize,gotsize,blkmode,tout,mode,edge,loopmode;
	int32 n,dir[4],bufenb[4];
	u_int8 *blkbuf = NULL;
	char *device,*str,*errstr,buf[40];

	/*--------------------+
	|  check arguments    |
	+--------------------*/
	if ((errstr = UTL_ILLIOPT("s=b=0=1=2=3=e=m=t=l?", buf))) {  /* check args */
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

	blksize   = ((str = UTL_TSTOPT("s=")) ? atoi(str) : 128);
	blkmode   = ((str = UTL_TSTOPT("b=")) ? atoi(str) : M_BUF_USRCTRL);
	edge      = ((str = UTL_TSTOPT("e=")) ? atoi(str) : -1);
	mode      = ((str = UTL_TSTOPT("m=")) ? atoi(str) : -1);
	bufenb[0] = ((str = UTL_TSTOPT("0=")) ? atoi(str) : -1);
	bufenb[1] = ((str = UTL_TSTOPT("1=")) ? atoi(str) : -1);
	bufenb[2] = ((str = UTL_TSTOPT("2=")) ? atoi(str) : -1);
	bufenb[3] = ((str = UTL_TSTOPT("3=")) ? atoi(str) : -1);
	tout      = ((str = UTL_TSTOPT("t=")) ? atoi(str) : 0);
	loopmode  = (UTL_TSTOPT("l") ? 1 : 0);

	/*--------------------+
	|  create buffer      |
	+--------------------*/
	if ((blkbuf = (u_int8*)malloc(blksize)) == NULL) {
		printf("*** can't alloc %d bytes\n",blksize);
		return(1);
	}

	/*--------------------+
	|  open path          |
	+--------------------*/
	if ((path = M_open(device)) < 0) {
		printf("*** can't open path: %s\n",M_errstring(UOS_ErrnoGet()));
		return(1);
	}

	/*--------------------+
	|  config             |
	+--------------------*/
	/* block i/o mode */
	if (blkmode != -1) {
		if ((M_setstat(path, M_BUF_RD_MODE, blkmode)) < 0) {
			printf("*** can't setstat M_BUF_RD_MODE: %s\n",
				   M_errstring(UOS_ErrnoGet()));
			goto abort;
		}
	}
	else {
		if ((M_getstat(path, M_BUF_RD_MODE, &blkmode)) < 0) {
			printf("*** can't getstat M_BUF_RD_MODE: %s\n",
				   M_errstring(UOS_ErrnoGet()));
			goto abort;
		}
	}

	/* block read timeout */
	if ((M_setstat(path, M_BUF_RD_TIMEOUT, tout)) < 0) {
		printf("*** can't setstat M_BUF_RD_TIMEOUT: %s\n",
			   M_errstring(UOS_ErrnoGet()));
		goto abort;
	}

	/* channel 0..3 */
	for (n=0; n<4; n++) {
		/* set current channel */
		if ((M_setstat(path, M_MK_CH_CURRENT, n)) < 0) {
			printf("*** can't setstat M_MK_CH_CURRENT: %s\n",
				   M_errstring(UOS_ErrnoGet()));
			goto abort;
		}

		/* buffering enable */
		if (bufenb[n] != -1) {
			if ((M_setstat(path, M58_BUF_ENABLE, bufenb[n])) < 0) {
				printf("*** can't setstat M58_BUF_ENABLE: %s\n",
					   M_errstring(UOS_ErrnoGet()));
				goto abort;
			}

			/* set input direction */
			if (bufenb[n]) {
				if ((M_setstat(path, M_LL_CH_DIR, M_CH_IN)) < 0) {
					printf("*** can't setstat M_LL_CH_DIR: %s\n",
						   M_errstring(UOS_ErrnoGet()));
					goto abort;
				}
			}
		}
		else {
			if ((M_getstat(path, M58_BUF_ENABLE, &bufenb[n])) < 0) {
				printf("*** can't getstat M58_BUF_ENABLE: %s\n",
					   M_errstring(UOS_ErrnoGet()));
				goto abort;
			}
		}

		/* get direction */
		if ((M_getstat(path, M_LL_CH_DIR, &dir[n])) < 0) {
			printf("*** can't getstat M_LL_CH_DIR: %s\n",
				   M_errstring(UOS_ErrnoGet()));
			goto abort;
		}
	}

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

	/* enable interrupt */
	if ((M_setstat(path, M_MK_IRQ_ENABLE, 1)) < 0) {
		printf("*** can't setstat  M_MK_IRQ_ENABLE: %s\n",
			   M_errstring(UOS_ErrnoGet()));
		goto abort;
	}

	/*--------------------+
	|  print info         |
	+--------------------*/
	for (n=0; n<4; n++) {
		printf("channel %d           : ",n);

		switch(dir[n]) {
			case M_CH_IN:	printf("INPUT,  ");	break;
			case M_CH_OUT:	printf("OUTPUT, ");	break;
		}

		if (bufenb[n])
			printf("block i/o ENABLED");
		else
			printf("block i/o DISABLED");

		printf("\n");
	}

	printf("block size          : %d bytes\n",blksize);
	printf("block i/o mode      : %d\n",blkmode);
	printf("block read timeout  : %d msec\n",tout);
	printf("trigger edge        : %s\n",(edge==0 ? "falling":"rising"));
	printf("data storage mode   : %d\n",mode);

	/*--------------------+
	|  read block         |
	+--------------------*/
	do {
		printf("\nwaiting for data ..\n");

		if ((gotsize = M_getblock(path,(u_int8*)blkbuf,blksize)) < 0) {
			printf("*** can't getblock: %s\n",
				   M_errstring(UOS_ErrnoGet()));
			break;
		}

		UTL_Memdump("read data",(char*)blkbuf,gotsize,1);

	} while(loopmode && UOS_KeyPressed() == -1);

	/*--------------------+
	|  cleanup            |
	+--------------------*/
	abort:

	if (blkbuf)
		free(blkbuf);

	if (M_close(path) < 0)
		printf("*** can't close path: %s\n",M_errstring(UOS_ErrnoGet()));

	return(0);
}



