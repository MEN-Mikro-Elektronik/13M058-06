/****************************************************************************
 ************                                                    ************
 ************                 M 5 8 _ B L K W R I T E            ************
 ************                                                    ************
 ****************************************************************************
 *
 *       Author: see
 *        $Date: 2013/06/26 16:53:48 $
 *    $Revision: 1.5 $
 *
 *  Description: Configure and write M58 output ports (blockwise)
 *
 *     Required: usr_oss.l usr_utl.l
 *     Switches: -
 *
 *-------------------------------[ History ]---------------------------------
 *
 * $Log: m58_blkwrite.c,v $
 * Revision 1.5  2013/06/26 16:53:48  gv
 * R: 1: Porting to MDIS
 *    2: Mixing of Tabs & space characters for indentation
 * M: 1: Changed according to MDIS Porting Guide 0.9
 *    2: Cosmetics : replacement of all spaces by 1 hard Tab for indentation
 *
 * Revision 1.4  2003/06/06 13:59:48  kp
 * cosmetics
 *
 * Revision 1.3  2000/04/14 14:34:04  Schmidt
 * usage function now static
 * Bug fix: free() returned wrong blk
 *
 * Revision 1.2  1998/10/05 18:08:48  Schmidt
 * main() : unreferenced local variable 'value' and 'ret' removed, cosmetics
 *
 * Revision 1.1  1998/10/01 15:55:01  see
 * Added by mcvs
 *
 *---------------------------------------------------------------------------
 * (c) Copyright 1998 by MEN mikro elektronik GmbH, Nuernberg, Germany
 ****************************************************************************/

static const char RCSid[]="$Header: /dd2/CVSR/COM/DRIVERS/MDIS_LL/M058/TOOLS/M58_BLKWRITE/COM/m58_blkwrite.c,v 1.5 2013/06/26 16:53:48 gv Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
	printf("Usage: m58_blkwrite [<opts>] <device> <value(s)> [<opts>]\n");
	printf("Function: Configure and write M58 channels (blockwise)\n");
	printf("Options:\n");
	printf("    device       device name                          [none]\n");
	printf("    value(s)     block data bytes (hex)               [none]\n");
	printf("    -<i>=<enb>   channel i buffering enable           [none]\n");
	printf("                 0 = disable block i/o, leave direction\n");
	printf("                 1 = enable  block i/o, set output direction\n");
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
int main(int argc, char **argv)
{
	MDIS_PATH path=0;
	int32 blksize,gotsize,waitkey,cnt;
	int32 n,dir[4],bufenb[4];
	u_int8 *blkbuf = NULL, *p;
	char *device,*str,*errstr,buf[40];

	/*--------------------+
	|  check arguments    |
	+--------------------*/
	if ((errstr = UTL_ILLIOPT("0=1=2=3=w?", buf))) {  /* check args */
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
	for (blksize=0, device=NULL, n=1; n<argc; n++) {
		if (*argv[n] != '-') {
			if (device == NULL)
				device = argv[n];
			else
				blksize++;
		}
	}

	if (!device || blksize==0) {
		usage();
		return(1);
	}

	bufenb[0] = ((str = UTL_TSTOPT("0=")) ? atoi(str) : -1);
	bufenb[1] = ((str = UTL_TSTOPT("1=")) ? atoi(str) : -1);
	bufenb[2] = ((str = UTL_TSTOPT("2=")) ? atoi(str) : -1);
	bufenb[3] = ((str = UTL_TSTOPT("3=")) ? atoi(str) : -1);
	waitkey   = (UTL_TSTOPT("w") ? 1 : 0);

	/*--------------------+
	|  create buffer      |
	+--------------------*/
	if ((p = blkbuf = (u_int8*)malloc(blksize)) == NULL) {
		printf("*** can't alloc %d bytes\n",blksize);
		return(1);
	}

	for (cnt=0, n=1; n<argc; n++) {
		if (*argv[n] != '-') {
			if (cnt++)
				*p++ = (u_int8)UTL_Atox(argv[n]);
		}
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
	/* channel 0..3 */
	for (n=0; n<4; n++) {
		/* set current channel */
		if ((M_setstat(path, M_MK_CH_CURRENT, n)) < 0) {
			printf("*** can't getstat M_MK_CH_CURRENT: %s\n",
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

			/* set output direction */
			if (bufenb[n]) {
				if ((M_setstat(path, M_LL_CH_DIR, M_CH_OUT)) < 0) {
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

	/*--------------------+
	|  write block        |
	+--------------------*/
	printf("\nwriting data ..\n");

	if ((gotsize = M_setblock(path,(u_int8*)blkbuf,blksize)) < 0) {
		printf("*** can't setblock: %s\n",
			   M_errstring(UOS_ErrnoGet()));
		goto abort;
	}

	UTL_Memdump("\nwritten data",(char*)blkbuf,gotsize,1);

	if (waitkey) {
		printf("\nPress key to continue\n");
		UOS_KeyWait();
	}

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




