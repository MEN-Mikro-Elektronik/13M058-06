/*********************  P r o g r a m  -  M o d u l e ***********************
 *
 *         Name: m58_drv.c
 *      Project: M58 module driver (MDIS5)
 *
 *       Author: see
 *
 *  Description: Low level driver for M58 modules
 *
 *               The M58 module is a 4 * 8-bit binary i/o module with trigger
 *               and interrupt capabilities.
 *
 *               The driver handles the M058 i/o ports as 4 channels:
 *
 *                  PORTA = channel 0
 *                  PORTB = channel 1
 *                  PORTC = channel 2
 *                  PORTD = channel 3
 *
 *               The direction of each channel can be defined as input or
 *               output (1)(2).
 *
 *               The physical termination of each input channel can be defined
 *               as active or passive (1)(2).
 *
 *               The data storage mode, i.e. which channel(s) are latched on
 *               read access or trigger edge, can be configured (1)(2).
 *
 *               The trigger line can be configured to generate interrupts
 *               on falling or rising edges (1)(2).
 *
 *               Each interrupt can produce a defineable user signal and
 *               reading input channels into a buffer.
 *
 *               (1) = defineable via status call
 *               (2) = defineable via descriptor key
 *
 *     Required: OSS, DESC, DBG, ID, MBUF libraries
 *     Switches: _ONE_NAMESPACE_PER_DRIVER_, MAC_BYTESWAP,
 *               _BIG_ENDIAN_, _LITTLE_ENDIAN_
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

#define _NO_LL_HANDLE		/* ll_defs.h: don't define LL_HANDLE struct */

#include <MEN/men_typs.h>	/* system dependend definitions   */
#include <MEN/maccess.h>	/* hw access macros and types     */
#include <MEN/dbg.h>		/* debug functions                */
#include <MEN/oss.h>		/* oss functions                  */
#include <MEN/desc.h>		/* descriptor functions           */
#include <MEN/mbuf.h>		/* buffer lib functions and types */
#include <MEN/modcom.h>		/* id prom functions              */
#include <MEN/mdis_api.h>	/* MDIS global defs               */
#include <MEN/mdis_com.h>	/* MDIS common defs               */
#include <MEN/mdis_err.h>	/* MDIS error codes               */
#include <MEN/ll_defs.h>	/* low level driver definitions   */

/*-----------------------------------------+
|  DEFINES                                 |
+-----------------------------------------*/
/* general */
#define CH_NUMBER			4			/* nr of device channels */
#define USE_IRQ				TRUE		/* interrupt required  */
#define ADDRSPACE_COUNT		1			/* nr of required address spaces */
#define ADDRSPACE_SIZE		256			/* size of address space */
#define MOD_ID_MAGIC		0x5346		/* id prom magic word */
#define MOD_ID_SIZE			128			/* id prom size [bytes] */
#define MOD_ID				58			/* id prom module id */

/* debug settings */
#define DBG_MYLEVEL			llHdl->dbgLevel
#define DBH					llHdl->dbgHdl

/* byte ordering check */
#if defined(_BIG_ENDIAN_) && defined(_LITTLE_ENDIAN_)
# error "Byte ordering collision, do not define _BIG_ENDIAN_ and _LITTLE_ENDIAN_ together"
#endif
#if !defined(_BIG_ENDIAN_) && !defined(_LITTLE_ENDIAN_)
# error "Byte ordering unknown, please define _BIG_ENDIAN_ or _LITTLE_ENDIAN_"
#endif

/* register offsets */
#if ( defined(_BIG_ENDIAN_) && !defined(MAC_BYTESWAP) ) ||	\
	( defined(_LITTLE_ENDIAN_) && defined(MAC_BYTESWAP) )
# define PORTD_REG 0x00		/* port D */
# define PORTC_REG 0x01		/* port C */
# define PORTB_REG 0x02		/* port B */
# define PORTA_REG 0x03		/* port A */
#else
# define PORTD_REG 0x01		/* port D */
# define PORTC_REG 0x00		/* port C */
# define PORTB_REG 0x03		/* port B */
# define PORTA_REG 0x02		/* port A */
#endif

#define CTRL0_REG 0x80		/* control 0 */
#define CTRL1_REG 0x82		/* control 1 */
#define CTRL2_REG 0x84		/* control 2 */
#define CTRL3_REG 0x86		/* control 3 */

/* register flags */
#define TR		0x08
#define IEN		0x08

/* register masks */
#define CONFIG	0x07

/* PORT_DIR definitions */
#define PORT_DIR_OUT		0x00
#define PORT_DIR_IN			0x01

/*-----------------------------------------+
|  TYPEDEFS                                |
+-----------------------------------------*/
/* ll handle */
typedef struct {
	/* general */
	int32			memAlloc;		/* size allocated for the handle */
	OSS_HANDLE		*osHdl;			/* oss handle */
	OSS_IRQ_HANDLE	*irqHdl;		/* irq handle */
	DESC_HANDLE		*descHdl;		/* desc handle */
	MACCESS			ma;				/* hw access handle */
	MDIS_IDENT_FUNCT_TBL idFuncTbl;	/* id function table */
	OSS_SIG_HANDLE	*sigHdl;		/* signal handle */
	/* debug */
	u_int32			dbgLevel;		/* debug level */
	DBG_HANDLE		*dbgHdl;		/* debug handle */
	/* misc */
	u_int32			idCheck;		/* id check enabled */
	u_int32			irqCount;		/* interrupt counter */
	u_int32			portDir[CH_NUMBER];		/* port direction */
	u_int32			portTerm[CH_NUMBER];	/* port termination */
	u_int8			portReg[CH_NUMBER];		/* port registers */
	u_int32			trigEdge;		/* trigger edge */
	u_int32			dataMode;		/* data storage mode */
	/* buffers */
	u_int32			bufEnable[CH_NUMBER];	/* buffer this channel */
	int32			bufRdSize;		/* nr of enabled input  channels */
	int32			bufWrSize;		/* nr of enabled output channel */
	MBUF_HANDLE		*bufHdl;		/* input buffer handle */
} LL_HANDLE;

/* include files which need LL_HANDLE */
#include <MEN/ll_entry.h>   /* low level driver jumptable  */
#include <MEN/m58_drv.h>   /* M58 driver header file */

static const char IdentString[]=MENT_XSTR(MAK_REVISION);

/*-----------------------------------------+
|  PROTOTYPES                              |
+-----------------------------------------*/
static char* Ident( void );
static int32 Cleanup(LL_HANDLE *llHdl, int32 retCode);
static u_int32 CalcBufSize(LL_HANDLE *llHdl, u_int32 portDir);

static int32 M58_Init(DESC_SPEC *descSpec, OSS_HANDLE *osHdl,
					   MACCESS *ma, OSS_SEM_HANDLE *devSemHdl,
					   OSS_IRQ_HANDLE *irqHdl, LL_HANDLE **llHdlP);
static int32 M58_Exit(LL_HANDLE **llHdlP );
static int32 M58_Read(LL_HANDLE *llHdl, int32 ch, int32 *value);
static int32 M58_Write(LL_HANDLE *llHdl, int32 ch, int32 value);
static int32 M58_SetStat(LL_HANDLE *llHdl,int32 ch, int32 code, INT32_OR_64 value32_or_64);
static int32 M58_GetStat(LL_HANDLE *llHdl, int32 ch, int32 code, INT32_OR_64 *value32_or_64P);
static int32 M58_BlockRead(LL_HANDLE *llHdl, int32 ch, void *buf, int32 size,
							int32 *nbrRdBytesP);
static int32 M58_BlockWrite(LL_HANDLE *llHdl, int32 ch, void *buf, int32 size,
							 int32 *nbrWrBytesP);
static int32 M58_Irq(LL_HANDLE *llHdl );
static int32 M58_Info(int32     infoType, ... );


/**************************** M58_GetEntry *********************************
 *
 *  Description:  Initialize drivers jump table
 *
 *---------------------------------------------------------------------------
 *  Input......:  ---
 *  Output.....:  drvP  pointer to the initialized jump table structure
 *  Globals....:  ---
 ****************************************************************************/
#ifdef _ONE_NAMESPACE_PER_DRIVER_
	extern void LL_GetEntry( LL_ENTRY* drvP )
#else
	extern void GetEntry( LL_ENTRY* drvP )
#endif
{
	drvP->init        = M58_Init;
	drvP->exit        = M58_Exit;
	drvP->read        = M58_Read;
	drvP->write       = M58_Write;
	drvP->blockRead   = M58_BlockRead;
	drvP->blockWrite  = M58_BlockWrite;
	drvP->setStat     = M58_SetStat;
	drvP->getStat     = M58_GetStat;
	drvP->irq         = M58_Irq;
	drvP->info        = M58_Info;
}

/******************************** M58_Init ***********************************
 *
 *  Description:  Allocate and return ll handle, initialize hardware
 *
 *                The function initializes all channels with the
 *                definitions made in the descriptor. The interrupt
 *                is disabled.
 *
 *                The following descriptor keys are used:
 *
 *                Deskriptor key        Default          Range
 *                --------------------  ---------------  -------------
 *                DEBUG_LEVEL_DESC      OSS_DBG_DEFAULT  see dbg.h
 *                DEBUG_LEVEL_MBUF      OSS_DBG_DEFAULT  see dbg.h
 *                DEBUG_LEVEL           OSS_DBG_DEFAULT  see dbg.h
 *                ID_CHECK              1                0..1
 *                TRIG_EDGE				0                0..1
 *                DATA_MODE				0                0..7
 *                CHANNEL_n/PORT_DIR	1                0..1
 *                CHANNEL_n/PORT_TERM	1                0..1
 *                CHANNEL_n/BUF_ENABLE	1                0..1
 *                IN_BUF/SIZE           8                0..max
 *                IN_BUF/MODE           0                M_BUF_xxx
 *                IN_BUF/TIMEOUT        0                0..max
 *                IN_BUF/HIGHWATER      0                0..max
 *
 *                TRIG_EDGE defines the trigger edge for data storage
 *                and interrupt generation:
 *
 *                   0 = falling edge
 *                   1 = risiing edge
 *
 *                DATA_MODE defines the data storage mode, i.e. which
 *                channel(s) are stored on read access or trigger edge.
 *                (For data storage modes, see M58_SetStat Function)
 *
 *                PORT_DIR defines the direction of channel n:
 *
 *                   0 = output
 *                   1 = input
 *
 *                PORT_TERM defines the termination of input channel n:
 *
 *                   0 = active
 *                   1 = passive
 *
 *                BUF_ENABLE enables/disables block i/o of channel n. If
 *                enabled, the corresponding channel is used with block i/o
 *                calls:
 *
 *                   0 = disable
 *                   1 = enable
 *
 *                SIZE defines the size of the input buffer [bytes]
 *                (minimum size is 8).
 *
 *                MODE defines the buffer's block i/o mode (see MDIS-Doc.):
 *
 *                   0 = M_BUF_USRCTRL
 *                   1 = M_BUF_RINGBUF
 *                   2 = M_BUF_RINGBUF_OVERWR
 *                   3 = M_BUF_CURRBUF
 *
 *                TIMEOUT defines the buffers read timeout [msec]
 *                (where timeout=0: no timeout) (see MDIS-Doc.).
 *
 *                HIGHWATER defines the buffer level in [bytes], of the
 *                corresponding highwater buffer event (see MDIS-Doc.).
 *
 *---------------------------------------------------------------------------
 *  Input......:  descSpec   pointer to descriptor data
 *                osHdl      oss handle
 *                ma         hw access handle
 *                devSemHdl  device semaphore handle
 *                irqHdl     irq handle
 *  Output.....:  llHdlP     ptr to low level driver handle
 *                return     success (0) or error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 M58_Init(
	DESC_SPEC       *descP,
	OSS_HANDLE      *osHdl,
	MACCESS         *ma,
	OSS_SEM_HANDLE  *devSemHdl,
	OSS_IRQ_HANDLE  *irqHdl,
	LL_HANDLE       **llHdlP
)
{
	LL_HANDLE *llHdl = NULL;
	u_int32 bufSize, bufMode, bufTout, bufHigh, bufDbgLevel;
	u_int32 gotsize, value, n;
	u_int16 ctrl;
	int32 error;

	/*------------------------------+
	|  prepare the handle           |
	+------------------------------*/
	/* alloc */
	if ((*llHdlP = llHdl = (LL_HANDLE*)
		 OSS_MemGet(osHdl, sizeof(LL_HANDLE), &gotsize)) == NULL)
		return(ERR_OSS_MEM_ALLOC);

	/* clear */
	OSS_MemFill(osHdl, gotsize, (char*)llHdl, 0x00);

	/* init */
	llHdl->memAlloc   = gotsize;
	llHdl->osHdl      = osHdl;
	llHdl->irqHdl     = irqHdl;
	llHdl->ma         = *ma;
	llHdl->portReg[0] = PORTA_REG;
	llHdl->portReg[1] = PORTB_REG;
	llHdl->portReg[2] = PORTC_REG;
	llHdl->portReg[3] = PORTD_REG;

	/*------------------------------+
	|  init id function table       |
	+------------------------------*/
	/* drivers ident function */
	llHdl->idFuncTbl.idCall[0].identCall = Ident;
	/* libraries ident functions */
	llHdl->idFuncTbl.idCall[1].identCall = DESC_Ident;
	llHdl->idFuncTbl.idCall[2].identCall = OSS_Ident;
	llHdl->idFuncTbl.idCall[3].identCall = MBUF_Ident;
	/* terminator */
	llHdl->idFuncTbl.idCall[4].identCall = NULL;

	/*------------------------------+
	|  prepare debugging            |
	+------------------------------*/
	DBG_MYLEVEL = OSS_DBG_DEFAULT;	/* set OS specific debug level */
	DBGINIT((NULL,&DBH));

	DBGWRT_1((DBH, "LL - M58_Init\n"));

	/*------------------------------+
	|  scan descriptor              |
	+------------------------------*/
	/* prepare access */
	if ((error = DESC_Init(descP, osHdl, &llHdl->descHdl)))
		return( Cleanup(llHdl,error) );

	/* DEBUG_LEVEL_DESC */
	if ((error = DESC_GetUInt32(llHdl->descHdl, OSS_DBG_DEFAULT,
								&value, "DEBUG_LEVEL_DESC")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

	DESC_DbgLevelSet(llHdl->descHdl, value);	/* set level */

	/* DEBUG_LEVEL_MBUF */
	if ((error = DESC_GetUInt32(llHdl->descHdl, OSS_DBG_DEFAULT,
								&bufDbgLevel, "DEBUG_LEVEL_MBUF")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

	/* DEBUG_LEVEL */
	if ((error = DESC_GetUInt32(llHdl->descHdl, OSS_DBG_DEFAULT,
								&llHdl->dbgLevel, "DEBUG_LEVEL")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

	/* ID_CHECK */
	if ((error = DESC_GetUInt32(llHdl->descHdl, TRUE,
								&llHdl->idCheck, "ID_CHECK")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

	/* TRIG_EDGE */
	if ((error = DESC_GetUInt32(llHdl->descHdl, 0x00,
								&llHdl->trigEdge, "TRIG_EDGE")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

	if (llHdl->trigEdge > 0x01)
		return( Cleanup(llHdl,ERR_LL_ILL_PARAM) );

	/* DATA_MODE */
	if ((error = DESC_GetUInt32(llHdl->descHdl, 0x00, &llHdl->dataMode,
								"DATA_MODE")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

	if (llHdl->dataMode > 0x07)
		return( Cleanup(llHdl,ERR_LL_ILL_PARAM) );

	/* channel 0..3 params */
	for (n=0; n<CH_NUMBER; n++) {
		/* CHANNEL_n/PORT_DIR */
		if ((error = DESC_GetUInt32(llHdl->descHdl, 0x01, &llHdl->portDir[n],
									"CHANNEL_%d/PORT_DIR", n)) &&
			error != ERR_DESC_KEY_NOTFOUND)
			return( Cleanup(llHdl,error) );

		if (llHdl->portDir[n] > 0x01)
			return( Cleanup(llHdl,ERR_LL_ILL_PARAM) );

		/* CHANNEL_n/PORT_TERM */
		if ((error = DESC_GetUInt32(llHdl->descHdl, 0x01, &llHdl->portTerm[n],
									"CHANNEL_%d/PORT_TERM", n)) &&
			error != ERR_DESC_KEY_NOTFOUND)
			return( Cleanup(llHdl,error) );

		if (llHdl->portTerm[n] > 0x01)
			return( Cleanup(llHdl,ERR_LL_ILL_PARAM) );

		/* CHANNEL_n/BUF_ENABLE */
		if ((error = DESC_GetUInt32(llHdl->descHdl, 0x01, &llHdl->bufEnable[n],
									"CHANNEL_%d/BUF_ENABLE", n)) &&
			error != ERR_DESC_KEY_NOTFOUND)
			return( Cleanup(llHdl,error) );

		if (llHdl->bufEnable[n] > 0x01)
			return( Cleanup(llHdl,ERR_LL_ILL_PARAM) );
	}

	/* SIZE */
	if ((error = DESC_GetUInt32(llHdl->descHdl, 8, &bufSize,
								"IN_BUF/SIZE")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

	if (bufSize < 8)
		return( Cleanup(llHdl,ERR_LL_ILL_PARAM) );

	/* MODE */
	if ((error = DESC_GetUInt32(llHdl->descHdl, M_BUF_USRCTRL, &bufMode,
								"IN_BUF/MODE")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

	/* TIMEOUT */
	if ((error = DESC_GetUInt32(llHdl->descHdl, 0x00, &bufTout,
								"IN_BUF/TIMEOUT")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

	/* HIGHWATER */
	if ((error = DESC_GetUInt32(llHdl->descHdl, 0x00, &bufHigh,
								"IN_BUF/HIGHWATER")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

	/* calculate buffer params */
	llHdl->bufRdSize  = CalcBufSize(llHdl, PORT_DIR_IN);
	llHdl->bufWrSize  = CalcBufSize(llHdl, PORT_DIR_OUT);

	/*------------------------------+
	|  install buffer               |
	+------------------------------*/
	/* create input buffer */
	if ((error = MBUF_Create(llHdl->osHdl, devSemHdl, llHdl,
							 bufSize, 1, bufMode, MBUF_RD,
							 bufHigh, bufTout, irqHdl, &llHdl->bufHdl)))
		return( Cleanup(llHdl,error) );

	/* set debug level */
	MBUF_SetStat(llHdl->bufHdl, NULL, M_BUF_RD_DEBUG_LEVEL, bufDbgLevel);

	/*------------------------------+
	|  check module id              |
	+------------------------------*/
	if (llHdl->idCheck) {
		int modIdMagic = m_read((U_INT32_OR_64)llHdl->ma, 0);
		int modId      = m_read((U_INT32_OR_64)llHdl->ma, 1);

		if (modIdMagic != MOD_ID_MAGIC) {
			DBGWRT_ERR((DBH," *** M58_Init: illegal magic=0x%04x\n",modIdMagic));
			error = ERR_LL_ILL_ID;
			return( Cleanup(llHdl,error) );
		}
		if (modId != MOD_ID) {
			DBGWRT_ERR((DBH," *** M58_Init: illegal id=%d\n",modId));
			error = ERR_LL_ILL_ID;
			return( Cleanup(llHdl,error) );
		}
	}

	/*------------------------------+
	|  init hardware                |
	+------------------------------*/
	/* disable irqs */
	MWRITE_D16(llHdl->ma, CTRL3_REG, 0x00);

	/* reset all ports */
	MWRITE_D8(llHdl->ma, PORTA_REG, 0x00);
	MWRITE_D8(llHdl->ma, PORTB_REG, 0x00);
	MWRITE_D8(llHdl->ma, PORTC_REG, 0x00);
	MWRITE_D8(llHdl->ma, PORTD_REG, 0x00);

	/* config port dir */
	ctrl = (u_int16)((llHdl->portDir[3] << 3) |
					 (llHdl->portDir[2] << 2) |
					 (llHdl->portDir[1] << 1) |
					 (llHdl->portDir[0] << 0));

	MWRITE_D16(llHdl->ma, CTRL0_REG, ctrl);

	/* config port term */
	ctrl = (u_int16)((llHdl->portTerm[3] << 3) |
					 (llHdl->portTerm[2] << 2) |
					 (llHdl->portTerm[1] << 1) |
					 (llHdl->portTerm[0] << 0));

	MWRITE_D16(llHdl->ma, CTRL1_REG, ctrl);

	/* config trigger/data mode */
	ctrl = (u_int16)((llHdl->trigEdge << 3) |
					 (llHdl->dataMode << 0));

	MWRITE_D16(llHdl->ma, CTRL2_REG, ctrl);

	return(ERR_SUCCESS);
}

/****************************** M58_Exit *************************************
 *
 *  Description:  De-initialize hardware and cleanup memory
 *
 *                The function deinitializes all channels by setting them
 *                to input direction and passive termination. The interrupt
 *                is disabled.
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdlP  	ptr to low level driver handle
 *  Output.....:  return    success (0) or error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 M58_Exit(
	LL_HANDLE    **llHdlP
)
{
	LL_HANDLE *llHdl = *llHdlP;
	int32 error = 0;

	DBGWRT_1((DBH, "LL - M58_Exit\n"));

	/*------------------------------+
	|  de-init hardware             |
	+------------------------------*/
	MWRITE_D16(llHdl->ma, CTRL0_REG, 0x0f);		/* all inputs */
	MWRITE_D16(llHdl->ma, CTRL1_REG, 0x0f);		/* all passive */
	MWRITE_D16(llHdl->ma, CTRL3_REG, 0x00);		/* disable irq */

	/*------------------------------+
	|  cleanup memory               |
	+------------------------------*/
	error = Cleanup(llHdl,error);

	return(error);
}

/****************************** M58_Read *************************************
 *
 *  Description:  Reads value from device
 *
 *                The function reads the state of the current channel.
 *
 *                Bits D7..D0 of the read value correspond with the port
 *                pins PIO_x7..x0.
 *
 *                If the channel's direction is not configured as input
 *                an ERR_LL_ILL_DIR error is returned.
 *
 *                Notes
 *                -----
 *                Pay attention to your correct channel configuration and
 *                data storage mode, since this has effect on correct
 *                channel latching (see also M58_SetStat and M58_BlockRead).
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdl    ll handle
 *                ch       current channel
 *  Output.....:  valueP   read value
 *                return   success (0) or error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 M58_Read(
	LL_HANDLE *llHdl,
	int32 ch,
	int32 *value
)
{
	DBGWRT_1((DBH, "LL - M58_Read: ch=%d\n",ch));

	/* check channel direction */
	if (llHdl->portDir[ch] !=  PORT_DIR_IN)
		return(ERR_LL_ILL_DIR);

	/* read channel */
	*value = MREAD_D8(llHdl->ma, llHdl->portReg[ch]) & 0xff;

	return(ERR_SUCCESS);
}

/****************************** M58_Write ************************************
 *
 *  Description:  Write value to device
 *
 *                The function writes a value to the current channel (D7..D0).
 *
 *                Bits D7..D0 of the write value correspond with the port
 *                pins PIO_x7..x0.
 *
 *                If the channel's direction is not configured as output
 *                an ERR_LL_ILL_DIR error is returned.
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdl    ll handle
 *                ch       current channel
 *                value    value to write
 *  Output.....:  return   success (0) or error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 M58_Write(
	LL_HANDLE *llHdl,
	int32 ch,
	int32 value
)
{
	DBGWRT_1((DBH, "LL - M58_Write: ch=%d, value=0x%x, llHdl->ma=0x%x\n",ch,value,llHdl->ma));

	/* check channel direction */
	if (llHdl->portDir[ch] !=  PORT_DIR_OUT)
		return(ERR_LL_ILL_DIR);

	/* write channel */
	MWRITE_D8(llHdl->ma, llHdl->portReg[ch], value & 0xff);

	return(ERR_SUCCESS);
}

/****************************** M58_SetStat **********************************
 *
 *  Description:  Set driver status
 *
 *                Following status codes are supported:
 *
 *                Code                 Description                Values
 *                -------------------  -------------------------  ----------
 *                M_LL_DEBUG_LEVEL     driver debug level         see oss.h
 *                M_MK_IRQ_ENABLE      interrupt enable           0..1
 *                M_LL_IRQ_COUNT       interrupt counter          0..max
 *                M_LL_CH_DIR          direction of curr chan     M_CH_IN,
 *                                                                M_CH_OUT
 *                M_BUF_xxx            input buffer codes         see MDIS
 *                -------------------  -------------------------  ----------
 *                M58_BUF_ENABLE       block i/o of curr chan     0..1
 *                M58_PORT_TERM        termination of curr chan   0..1
 *                M58_TRIG_EDGE        trigger edge               0..1
 *                M58_DATA_MODE        data storage mode          0..7
 *                M58_TRIG_SIG_SET     trigger signal enable      1..max
 *                M58_TRIG_SIG_CLR     trigger signal disable     -
 *                -------------------  -------------------------  ----------
 *
 *                With M_LL_CH_DIR the direction of the current channel can
 *                be altered (see MDIS Doc.).
 *
 *                M58_BUF_ENABLE enables/disables block i/o of current channel.
 *                If enabled, the channel is used with block i/o calls:
 *
 *                   0 = disable
 *                   1 = enable
 *
 *                M58_PORT_TERM defines the termination of current channel:
 *
 *                   0 = active  (M58_TERM_ACTIVE)
 *                   1 = passive (M58_TRIG_RISE)
 *
 *                M58_TRIG_EDGE defines the trigger edge for data storage
 *                and interrupt generation:
 *
 *                   0 = falling (M58_TRIG_FALL)
 *                   1 = rising  (M58_TRIG_RISE)
 *
 *                M58_DATA_MODE defines the data storage mode, i.e. which
 *                channel(s) are latched on read access or trigger edge:
 *                (The data storage mode has only effect on input channels)
 *
 *                   Mode     Chan. 0     Chan. 1     Chan. 2     Chan. 3
 *                   ----     -------     -------     -------     -------
 *                   0        read #0     read #1     read #2     read #3
 *                   1        read #0     read #0     read #0     read #0
 *                   2        read #0     read #1     read #2     trigger
 *                   3        read #0     read #1     trigger     trigger
 *                   4        read #0     trigger     trigger     trigger
 *                   5        trigger     trigger     trigger     trigger
 *                   6        (reserved)
 *                   7        (data storage blocked)
 *
 *                   (read #i) = Port state is latched when reading channel i
 *                   (trigger) = Port state is latched at trigger edge.
 *
 *                Modes 0..1 should normally be used for direct input, Mode 5
 *                for buffered input. Modes 2..4 allow mixed usage of direct
 *                and buffered i/o.
 *
 *                M58_TRIG_SIG_SET enables the trigger signal. The signal
 *                code is passed as value. Note that code=0 is not allowed.
 *
 *                M58_TRIG_SIG_CLR disables the trigger signal.
 *
 *                Notes
 *                -----
 *                Changing M_LL_CH_DIR, M58_BUF_ENABLE or M58_DATA_MODE while
 *                block i/o is running, will cause unpredictable results !
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdl         ll handle
 *                code          status code
 *                ch            current channel
 *                value32_or_64 data or
 *                              ptr to block data struct (M_SG_BLOCK)(*)
 *                              (*) = for block status codes
 *  Output.....:  return        success (0) or error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 M58_SetStat(
	LL_HANDLE *llHdl,
	int32  code,
	int32  ch,
	INT32_OR_64 value32_or_64
)
{
	int32       value = (int32)value32_or_64; /* 32bit value     */
	/* INT32_OR_64 valueP = value32_or_64;       /\* stores 32/64bit pointer *\/ */

	int32 error = ERR_SUCCESS;

	DBGWRT_1((DBH, "LL - M58_SetStat: ch=%d code=0x%04x value=0x%x\n",
			  ch,code,value));

	switch(code) {
		/*--------------------------+
		|  debug level              |
		+--------------------------*/
		case M_LL_DEBUG_LEVEL:
			llHdl->dbgLevel = value;
			break;
		/*--------------------------+
		|  enable interrupts        |
		+--------------------------*/
		case M_MK_IRQ_ENABLE:
			if (value) 			/* enable irqs */
				MSETMASK_D16(llHdl->ma, CTRL3_REG, IEN);
			else 				/* disable irqs */
				MCLRMASK_D16(llHdl->ma, CTRL3_REG, IEN);

			break;
		/*--------------------------+
		|  set irq counter          |
		+--------------------------*/
		case M_MK_IRQ_COUNT:
			llHdl->irqCount = value;
			break;
		/*--------------------------+
		|  channel direction        |
		+--------------------------*/
		case M_LL_CH_DIR:
			switch(value) {
				case M_CH_OUT:
					llHdl->portDir[ch] = PORT_DIR_OUT;
					MCLRMASK_D16(llHdl->ma, CTRL0_REG, 1<<ch);
					break;
				case M_CH_IN:
					llHdl->portDir[ch] = PORT_DIR_IN;
					MSETMASK_D16(llHdl->ma, CTRL0_REG, 1<<ch);
					break;
				default:
					error = ERR_LL_ILL_PARAM;
			}

			/* update buffer params */
			llHdl->bufRdSize  = CalcBufSize(llHdl, PORT_DIR_IN);
			llHdl->bufWrSize  = CalcBufSize(llHdl, PORT_DIR_OUT);

			break;
		/*--------------------------+
		|  set channel termination  |
		+--------------------------*/
		case M58_PORT_TERM:
			switch(value) {
				case M58_TERM_ACTIVE:
					llHdl->portTerm[ch] = value;
					MCLRMASK_D16(llHdl->ma, CTRL1_REG, 1<<ch);
					break;
				case M58_TERM_PASSIVE:
					llHdl->portTerm[ch] = value;
					MSETMASK_D16(llHdl->ma, CTRL1_REG, 1<<ch);
					break;
				default:
					error = ERR_LL_ILL_PARAM;
			}

			break;
		/*--------------------------+
		|  set trigger edge         |
		+--------------------------*/
		case M58_TRIG_EDGE:
			switch(value) {
				case M58_TRIG_FALL:
					llHdl->trigEdge = value;
					MCLRMASK_D16(llHdl->ma, CTRL2_REG, TR);
					break;
				case M58_TRIG_RISE:
					llHdl->trigEdge = value;
					MSETMASK_D16(llHdl->ma, CTRL2_REG, TR);
					break;
				default:
					error = ERR_LL_ILL_PARAM;
			}

			break;
		/*--------------------------+
		|  set data storage mode    |
		+--------------------------*/
		case M58_DATA_MODE:
			if (!IN_RANGE(value,0,CONFIG))
				return(ERR_LL_ILL_PARAM);

			llHdl->dataMode = value;
			MCLRMASK_D16(llHdl->ma, CTRL2_REG, CONFIG);
			MSETMASK_D16(llHdl->ma, CTRL2_REG, value);
			break;
		/*--------------------------+
		|  trigger signal enable    |
		+--------------------------*/
		case M58_TRIG_SIG_SET:
			/* illegal signal code ? */
			if (value == 0)
				return(ERR_LL_ILL_PARAM);

			/* already defined ? */
			if (llHdl->sigHdl != NULL) {
				DBGWRT_ERR((DBH, " *** M58_SetStat: signal already installed"));
				return(ERR_OSS_SIG_SET);
			}

			/* install signal */
			if ((error = OSS_SigCreate(llHdl->osHdl, value, &llHdl->sigHdl)))
				return(error);

			break;
		/*--------------------------+
		|  trigger signal disable   |
		+--------------------------*/
		case M58_TRIG_SIG_CLR:
			/* not defined ? */
			if (llHdl->sigHdl == NULL) {
				DBGWRT_ERR((DBH, " *** M58_SetStat: signal not installed"));
				return(ERR_OSS_SIG_CLR);
			}

			/* remove signal */
			if ((error = OSS_SigRemove(llHdl->osHdl, &llHdl->sigHdl)))
				return(error);

			break;
		/*--------------------------+
		|  channel block i/o        |
		+--------------------------*/
		case M58_BUF_ENABLE:
			llHdl->bufEnable[ch] = value;

			/* update buffer params */
			llHdl->bufRdSize  = CalcBufSize(llHdl, PORT_DIR_IN);
			llHdl->bufWrSize  = CalcBufSize(llHdl, PORT_DIR_OUT);
			break;
		/*--------------------------+
		|  MBUF + unknown           |
		+--------------------------*/
		default:
			if (M_BUF_CODE(code))
				error = MBUF_SetStat(llHdl->bufHdl, NULL, code, value);
			else
				error = ERR_LL_UNK_CODE;
	}

	return(error);
}

/****************************** M58_GetStat **********************************
 *
 *  Description:  Get driver status
 *
 *                Following status codes are supported:
 *
 *                Code                 Description                Values
 *                -------------------  -------------------------  ----------
 *                M_LL_DEBUG_LEVEL     driver debug level         see oss.h
 *                M_LL_CH_NUMBER       number of channels         4
 *                M_LL_CH_DIR          direction of curr chan     M_CH_IN,
 *                                                                M_CH_OUT
 *                M_LL_CH_LEN          length of curr chan [bits] 1..max
 *                M_LL_CH_TYP          description of curr chan   M_CH_BINARY
 *                M_LL_IRQ_COUNT       interrupt counter          0..max
 *                M_LL_ID_CHECK        eeprom is checked          0..1
 *                M_LL_ID_SIZE         eeprom size [bytes]        128
 *                M_LL_BLK_ID_DATA     eeprom raw data            -
 *                M_MK_BLK_REV_ID      ident function table ptr   -
 *                M_BUF_xxx            input buffer codes         see MDIS
 *                -------------------  -------------------------  ----------
 *                M58_BUF_ENABLE       block i/o of curr chan     0..1
 *                M58_PORT_TERM        termination of curr chan   0..1
 *                M58_TRIG_EDGE        trigger edge               0..1
 *                M58_DATA_MODE        data storage mode          0..7
 *                M58_TRIG_SIG_SET     trigger signal code        0..max
 *                M58_BUF_RDSIZE       nr of enabled input  chan. 0..4
 *                M58_BUF_WRSIZE       nr of enabled output chan. 0..4
 *                -------------------  -------------------------  ----------
 *
 *                M_LL_CH_DIR returns  the direction of the current channel
 *                (see MDIS Doc.).
 *
 *                M58_BUF_ENABLE returns if current channel is used with
 *                block i/o calls:
 *
 *                   0 = disabled
 *                   1 = enabled
 *
 *                M58_PORT_TERM returns the termination of current channel:
 *
 *                   0 = active  (M58_TERM_ACTIVE)
 *                   1 = passive (M58_TRIG_RISE)
 *
 *                M58_TRIG_EDGE returns the trigger edge for data storage
 *                and interrupt generation:
 *
 *                   0 = falling (M58_TRIG_FALL)
 *                   1 = rising  (M58_TRIG_RISE)
 *
 *                M58_DATA_MODE returns the data storage mode, i.e. which
 *                channel(s) are latched on read access or trigger edge.
 *                (For data storage modes, see M58_SetStat Function)
 *
 *                M58_TRIG_SIG_SET returns the signal code of an installed
 *                trigger signal. Zero is returned, if no signal installed.
 *
 *                M58_BUF_RDSIZE returns the number of input channels where
 *                block i/o is enabled (minimum size for M58_BlockRead).
 *
 *                M58_BUF_WRSIZE returns the number of output channels where
 *                block i/o is enabled (minimum size for M58_BlockWrite).
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdl             ll handle
 *                code              status code
 *                ch                current channel
 *                value32_or_64P    ptr to block data struct (M_SG_BLOCK)(*)
 *                                  (*) = for block status codes
 *  Output.....:  value32_or_64P    data ptr or
 *                                  ptr to block data struct (M_SG_BLOCK)(*)
 *                return            success (0) or error code
 *                                  (*) = for block status codes
 *  Globals....:  ---
 ****************************************************************************/
static int32 M58_GetStat(
	LL_HANDLE *llHdl,
	int32  code,
	int32  ch,
	INT32_OR_64 *value32_or_64P
)
{
	int32       *valueP = (int32*)value32_or_64P; /* pointer to 32bit value  */
	INT32_OR_64	*value64P = value32_or_64P;       /* stores 32/64bit pointer  */
	M_SG_BLOCK *blk = (M_SG_BLOCK*)value32_or_64P;
	int32 dummy;
	int32 error = ERR_SUCCESS;

	DBGWRT_1((DBH, "LL - M58_GetStat: ch=%d code=0x%04x\n",
			  ch,code));

	switch(code)
	{
		/*--------------------------+
		|  debug level              |
		+--------------------------*/
		case M_LL_DEBUG_LEVEL:
			*valueP = llHdl->dbgLevel;
			break;
		/*--------------------------+
		|  nr of channels           |
		+--------------------------*/
		case M_LL_CH_NUMBER:
			*valueP = CH_NUMBER;
			break;
		/*--------------------------+
		|  channel direction        |
		+--------------------------*/
		case M_LL_CH_DIR:
			switch(llHdl->portDir[ch]) {
				case PORT_DIR_OUT:	*valueP = M_CH_OUT;	break;
				case PORT_DIR_IN:	*valueP = M_CH_IN;	break;
				default: error = ERR_LL; /* should never occure */
			}

			break;
		/*--------------------------+
		|  channel length [bits]    |
		+--------------------------*/
		case M_LL_CH_LEN:
			*valueP = 8;
			break;
		/*--------------------------+
		|  channel type info        |
		+--------------------------*/
		case M_LL_CH_TYP:
			*valueP = M_CH_BINARY;
			break;
		/*--------------------------+
		|  irq counter              |
		+--------------------------*/
		case M_LL_IRQ_COUNT:
			*valueP = llHdl->irqCount;
			break;
		/*--------------------------+
		|  id prom check enabled    |
		+--------------------------*/
		case M_LL_ID_CHECK:
			*valueP = llHdl->idCheck;
			break;
		/*--------------------------+
		|   id prom size            |
		+--------------------------*/
		case M_LL_ID_SIZE:
			*valueP = MOD_ID_SIZE;
			break;
		/*--------------------------+
		|   id prom data            |
		+--------------------------*/
		case M_LL_BLK_ID_DATA:
		{
			u_int8 n;
			u_int16 *dataP = (u_int16*)blk->data;

			if (blk->size < MOD_ID_SIZE)		/* check buf size */
				return(ERR_LL_USERBUF);

			for (n=0; n<MOD_ID_SIZE/2; n++)		/* read MOD_ID_SIZE/2 words */
				*dataP++ = (int16)m_read((U_INT32_OR_64)llHdl->ma, n);

			break;
		}
		/*--------------------------+
		|   ident table pointer     |
		|   (treat as non-block!)   |
		+--------------------------*/
		case M_MK_BLK_REV_ID:
			*value64P = (INT32_OR_64)&llHdl->idFuncTbl;
			break;
		/*--------------------------+
		|  get channel termination  |
		+--------------------------*/
		case M58_PORT_TERM:
			*valueP = llHdl->portTerm[ch];
			break;
		/*--------------------------+
		|  get trigger edge         |
		+--------------------------*/
		case M58_TRIG_EDGE:
			*valueP = llHdl->trigEdge;
			break;
		/*--------------------------+
		|  get data storage mode    |
		+--------------------------*/
		case M58_DATA_MODE:
			*valueP = llHdl->dataMode;
			break;
		/*--------------------------+
		|  trigger signal code      |
		+--------------------------*/
		case M58_TRIG_SIG_SET:
			if (llHdl->sigHdl == NULL)
				*valueP = 0x00;
			else
				OSS_SigInfo(llHdl->osHdl, llHdl->sigHdl, valueP, &dummy);

			break;
		/*--------------------------+
		|  channel block i/o        |
		+--------------------------*/
		case M58_BUF_ENABLE:
			*valueP = llHdl->bufEnable[ch];
			break;
		/*--------------------------+
		|  enabled input  channels  |
		+--------------------------*/
		case M58_BUF_RDSIZE:
			*valueP = llHdl->bufRdSize;
			break;
		/*--------------------------+
		|  enabled output channels  |
		+--------------------------*/
		case M58_BUF_WRSIZE:
			*valueP = llHdl->bufWrSize;
			break;
		/*--------------------------+
		|  MBUF + unknown           |
		+--------------------------*/
		default:
			if (M_BUF_CODE(code))
				error = MBUF_GetStat(llHdl->bufHdl, NULL, code, valueP);
			else
				error = ERR_LL_UNK_CODE;
	}

	return(error);
}

/******************************* M58_BlockRead *******************************
 *
 *  Description:  Read data block from device
 *
 *                Following block i/o modes are supported:
 *
 *                   M_BUF_USRCTRL         direct input
 *                   M_BUF_RINGBUF         buffered input
 *                   M_BUF_RINGBUF_OVERWR  buffered input
 *                   M_BUF_CURRBUF         buffered input
 *
 *                (Can be defined via M_BUF_RD_MODE setstat, see MDIS-Doc.)
 *
 *                Direct Input Mode
 *                -----------------
 *                For the M_BUF_USRCTRL mode, the function reads all input
 *                channels, which are enabled for block i/o in ascending order
 *                into the given data buffer:
 *
 *                   +---------+
 *                   |  byte 0 |  first enabled input channel
 *                   +---------+
 *                   |  byte 1 |
 *                   +---------+
 *                   |   ...   |
 *                   +---------+
 *                   |  byte k |  last enabled input channel
 *                   +---------+
 *
 *                The maximum size which can be read depends on the number
 *                of enabled input channels and can be queried via the
 *                M58_BUF_RDSIZE getstat.
 *
 *                If no input channel is enabled ERR_LL_READ is returned.
 *
 *                Pay attention to your correct channel configuration and
 *                data storage mode, since this has effect on correct
 *                channel latching. It is recommended to use one of the
 *                following data storage modes:
 *
 *                   mode=0: each channel 0..3 is latched when reading it
 *                   mode=1: all channels are latched when reading channel 0
 *                   mode=2: each channel 0..2 is latched when reading it
 *                   mode=3: each channel 0..1 is latched when reading it
 *                   mode=4:      channel    0 is latched when reading it
 *
 *                Buffered Input Mode
 *                -------------------
 *                For all other modes, the function copies requested number
 *                of bytes from the input buffer to the given data buffer.
 *                (see also function M58_Irq)
 *
 *                For details on buffered input modes refer to the MDIS-Doc.
 *
 *                Pay attention to your correct channel configuration and
 *                data storage mode, since this has effect on correct
 *                channel latching. It is recommended to use one of the
 *                following data storage modes:
 *
 *                   mode=2: channel 3        latched when trigger occured
 *                   mode=3: channel 2+3      latched when trigger occured
 *                   mode=4: channel 1+2+3    latched when trigger occured
 *                   mode=5: channel 0+1+2+3  latched when trigger occured
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdl        ll handle
 *                ch           current channel
 *                buf          data buffer
 *                size         data buffer size
 *  Output.....:  nbrRdBytesP  number of read bytes
 *                return       success (0) or error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 M58_BlockRead(
	LL_HANDLE *llHdl,
	int32     ch,
	void      *buf,
	int32     size,
	int32     *nbrRdBytesP
)
{
	u_int8 *bufP = (u_int8*)buf;
	u_int32 n;
	int32 bufMode;
	int32 error;

	DBGWRT_1((DBH, "LL - M58_BlockRead: ch=%d, size=%d\n",ch,size));

	/* get current buffer mode */
	if ((error = MBUF_GetBufferMode(llHdl->bufHdl, &bufMode)))
		return(error);

	/*-------------------------+
	| read from hardware       |
	+-------------------------*/
	if (bufMode == M_BUF_USRCTRL) {
		/* check if any channel to read */
		if (llHdl->bufRdSize == 0)
			return(ERR_LL_READ);

		/* check size */
		if (size < llHdl->bufRdSize)
			return(ERR_LL_USERBUF);

		/* read port A..D */
		for (n=0; n<CH_NUMBER; n++)
			if (llHdl->bufEnable[n] && (llHdl->portDir[n] == PORT_DIR_IN))
				*bufP++ = MREAD_D8(llHdl->ma, llHdl->portReg[n]);

		*nbrRdBytesP = (u_int32)(bufP - (u_int8*)buf);
	}
	/*-------------------------+
	| read from input buffer   |
	+-------------------------*/
	else {
		/* read from buffer */
		if ((error = MBUF_Read(llHdl->bufHdl, bufP, size, nbrRdBytesP)))
			return(error);
	}

	return(ERR_SUCCESS);
}

/****************************** M58_BlockWrite *******************************
 *
 *  Description:  Write data block to device
 *
 *                Following block i/o modes are supported:
 *
 *                   M_BUF_USRCTRL         direct input
 *
 *                (Can be defined via M_BUF_WR_MODE setstat, see MDIS-Doc.)
 *
 *                Direct Input Mode
 *                -----------------
 *                The function writes the values from the given buffer to
 *                all output channels, which are enabled for block i/o in
 *                ascending order:
 *
 *                   +---------+
 *                   |  byte 0 |  first enabled output channel
 *                   +---------+
 *                   |  byte 1 |
 *                   +---------+
 *                   |   ...   |
 *                   +---------+
 *                   |  byte k |  last enabled output channel
 *                   +---------+
 *
 *                The maximum size which can be written depends on the number
 *                of enabled output channels and can be queried via the
 *                M58_BUF_WRSIZE getstat.
 *
 *                If no output channel is enabled ERR_LL_WRITE is returned.
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdl        ll handle
 *                ch           current channel
 *                buf          data buffer
 *                size         data buffer size
 *  Output.....:  nbrWrBytesP  number of written bytes
 *                return       success (0) or error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 M58_BlockWrite(
	LL_HANDLE *llHdl,
	int32     ch,
	void      *buf,
	int32     size,
	int32     *nbrWrBytesP
)
{
	u_int8 *bufP = (u_int8*)buf;
	u_int32 n;

	DBGWRT_1((DBH, "LL - M58_BlockWrite: ch=%d, size=%d\n",ch,size));

	/* check if any channel to write */
	if (llHdl->bufWrSize == 0)
		return(ERR_LL_WRITE);

	/* check size */
	if (size < llHdl->bufWrSize)
		return(ERR_LL_USERBUF);

	/*-------------------------+
	| write to  hardware       |
	+-------------------------*/
	for (n=0; n<CH_NUMBER; n++)
		if (llHdl->bufEnable[n] && (llHdl->portDir[n] == PORT_DIR_OUT))
			MWRITE_D8(llHdl->ma, llHdl->portReg[n], *bufP++);

	/* return nr of written bytes */
	*nbrWrBytesP = (int32)(bufP - (u_int8*)buf);

	return(ERR_SUCCESS);
}


/****************************** M58_Irq *************************************
 *
 *  Description:  Interrupt service routine
 *
 *                The interrupt is triggered when the defined edge of the
 *                trigger signal occurs.
 *
 *                If an input buffer is used, all input channels, which
 *                are enabled for block i/o are stored in ascending order
 *                in the input buffer:
 *
 *                   +---------+
 *                   |  byte 0 |  first enabled input channel
 *                   +---------+
 *                   |  byte 1 |
 *                   +---------+
 *                   |   ...   |
 *                   +---------+
 *                   |  byte k |  last enabled input channel
 *                   +---------+
 *
 *                If trigger signal is enabled, the defined signal is send
 *                to the user process.
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdl    ll handle
 *  Output.....:  return   LL_IRQ_DEVICE    => device has caused irq
 *                         LL_IRQ_DEV_NOT   => not caused
 *                         LL_IRQ_UNKNOWN   => unknown
 *  Globals....:  ---
 ****************************************************************************/
static int32 M58_Irq(
	LL_HANDLE *llHdl
)
{
	int32 n, got;
	u_int16 dummy;
	u_int8 *bufP;

	IDBGWRT_1((DBH, ">>> M58_Irq:\n"));

	/*----------------------+
	| reset irq             |
	+----------------------*/
	dummy = MREAD_D16(llHdl->ma, CTRL3_REG);

	/*----------------------+
	| fill buffer           |
	+----------------------*/
	for (n=0; n<CH_NUMBER; n++){
		if (llHdl->bufEnable[n] && (llHdl->portDir[n] == PORT_DIR_IN)) {
			/* get buffer ptr - overrun ? */
			if ((bufP = (u_int8*)MBUF_GetNextBuf(llHdl->bufHdl, 1, &got)) == NULL)
				break;

			/* fill buffer entry */
			*bufP = MREAD_D8(llHdl->ma, llHdl->portReg[n]);

			/* say: buffer written */
			MBUF_ReadyBuf(llHdl->bufHdl);
		}
	}

	/*----------------------+
	| send signal           |
	+----------------------*/
	if (llHdl->sigHdl)
		OSS_SigSend(llHdl->osHdl, llHdl->sigHdl);

	llHdl->irqCount++;

	return(LL_IRQ_UNKNOWN);		/* say: unknown */
}

/****************************** M58_Info ************************************
 *
 *  Description:  Get information about hardware and driver requirements.
 *
 *                Following info codes are supported:
 *
 *                Code                      Description
 *                ------------------------  -----------------------------
 *                LL_INFO_HW_CHARACTER      hardware characteristics
 *                LL_INFO_ADDRSPACE_COUNT   nr of required address spaces
 *                LL_INFO_ADDRSPACE         address space information
 *                LL_INFO_IRQ               interrupt required
 *                LL_INFO_LOCKMODE          process lock mode required
 *
 *                The LL_INFO_HW_CHARACTER code returns all address and
 *                data modes (OR'ed), which are supported from the
 *                hardware (MDIS_MAxx, MDIS_MDxx).
 *
 *                The LL_INFO_ADDRSPACE_COUNT code returns the number
 *                of address spaces used from the driver.
 *
 *                The LL_INFO_ADDRSPACE code returns information about one
 *                specific address space (MDIS_MAxx, MDIS_MDxx). The returned
 *                data mode represents the widest hardware access used from
 *                the driver.
 *
 *                The LL_INFO_IRQ code returns, if the driver supports an
 *                interrupt routine (TRUE or FALSE).
 *
 *                The LL_INFO_LOCKMODE code returns, which process locking
 *                mode is required from the driver (LL_LOCK_xxx).
 *
 *---------------------------------------------------------------------------
 *  Input......:  infoType     info code
 *                ...          argument(s)
 *  Output.....:  return       success (0) or error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 M58_Info(
	int32  infoType,
	...
)
{
	int32   error = ERR_SUCCESS;
	va_list argptr;

	va_start(argptr, infoType );

	switch(infoType) {
		/*-------------------------------+
		|  hardware characteristics      |
		|  (all addr/data modes OR'ed)   |
		+-------------------------------*/
		case LL_INFO_HW_CHARACTER:
		{
			u_int32 *addrModeP = va_arg(argptr, u_int32*);
			u_int32 *dataModeP = va_arg(argptr, u_int32*);

			*addrModeP = MDIS_MA08;
			*dataModeP = MDIS_MD08 | MDIS_MD16;
			break;
		}
		/*-------------------------------+
		|  nr of required address spaces |
		|  (total spaces used)           |
		+-------------------------------*/
		case LL_INFO_ADDRSPACE_COUNT:
		{
			u_int32 *nbrOfAddrSpaceP = va_arg(argptr, u_int32*);

			*nbrOfAddrSpaceP = ADDRSPACE_COUNT;
			break;
		}
		/*-------------------------------+
		|  address space type            |
		|  (widest used data mode)       |
		+-------------------------------*/
		case LL_INFO_ADDRSPACE:
		{
			u_int32 addrSpaceIndex = va_arg(argptr, u_int32);
			u_int32 *addrModeP = va_arg(argptr, u_int32*);
			u_int32 *dataModeP = va_arg(argptr, u_int32*);
			u_int32 *addrSizeP = va_arg(argptr, u_int32*);

			if (addrSpaceIndex >= ADDRSPACE_COUNT)
				error = ERR_LL_ILL_PARAM;
			else {
				*addrModeP = MDIS_MA08;
				*dataModeP = MDIS_MD16;
				*addrSizeP = ADDRSPACE_SIZE;
			}

			break;
		}
		/*-------------------------------+
		|   interrupt required           |
		+-------------------------------*/
		case LL_INFO_IRQ:
		{
			u_int32 *useIrqP = va_arg(argptr, u_int32*);

			*useIrqP = USE_IRQ;
			break;
		}
		/*-------------------------------+
		|   process lock mode            |
		+-------------------------------*/
		case LL_INFO_LOCKMODE:
		{
			u_int32 *lockModeP = va_arg(argptr, u_int32*);

			*lockModeP = LL_LOCK_CALL;
			break;
		}
		/*-------------------------------+
		|   (unknown)                    |
		+-------------------------------*/
		default:
		error = ERR_LL_ILL_PARAM;
	}

	va_end(argptr);
	return(error);
}

/*******************************  Ident  ************************************
 *
 *  Description:  Return ident string
 *
 *---------------------------------------------------------------------------
 *  Input......:  -
 *  Output.....:  return  ptr to ident string
 *  Globals....:  -
 ****************************************************************************/
static char* Ident( void )  /* nodoc */
{
	return( (char*) IdentString );
}

/********************************* Cleanup **********************************
 *
 *  Description: Close all handles, free memory and return error code
 *               NOTE: The ll handle is invalid after calling this function
 *
 *---------------------------------------------------------------------------
 *  Input......: llHdl      ll handle
 *               retCode    return value
 *  Output.....: return	    retCode
 *  Globals....: -
 ****************************************************************************/
static int32 Cleanup(
	LL_HANDLE    *llHdl,
	int32        retCode    /* nodoc */
)
{
	/*------------------------------+
	|  close handles                |
	+------------------------------*/
	/* clean up desc */
	if (llHdl->descHdl)
		DESC_Exit(&llHdl->descHdl);

	/* clean up buffer */
	if (llHdl->bufHdl)
		MBUF_Remove(&llHdl->bufHdl);

	/* clean up signal */
	if (llHdl->sigHdl)
		OSS_SigRemove(llHdl->osHdl, &llHdl->sigHdl);

	/* cleanup debug */
	DBGEXIT((&DBH));

	/*------------------------------+
	|  free memory                  |
	+------------------------------*/
	/* free my handle */
	OSS_MemFree(llHdl->osHdl, (int8*)llHdl, llHdl->memAlloc);

	/*------------------------------+
	|  return error code            |
	+------------------------------*/
	return(retCode);
}

/********************************* CalcBufSize ******************************
 *
 *  Description: Calculates the buffer size for all enabled channels
 *               with matching port direction.
 *
 *---------------------------------------------------------------------------
 *  Input......: llHdl      ll handle
 *               portDir    port direction (PORT_DIR_xxx)
 *  Output.....: return     buffer size [bytes]
 *  Globals....: -
 ****************************************************************************/
static u_int32 CalcBufSize(
	LL_HANDLE *llHdl,
	u_int32   portDir     /* nodoc */
)
{
	u_int32 n, size;

	for (size=0, n=0; n<CH_NUMBER; n++)
		if (llHdl->bufEnable[n] && (llHdl->portDir[n] == portDir))
			size++;

	DBGWRT_1((DBH," buf%sSize=%d\n",portDir ? "Rd":"Wr",size));

	return(size);
}




