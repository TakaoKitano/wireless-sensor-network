/****************************************************************************
 * (C) Tokyo Cosmos Electric, Inc. (TOCOS) - 2012 all rights reserved.
 *
 * Condition to use:
 *   - The full or part of source code is limited to use for TWE (TOCOS
 *     Wireless Engine) as compiled and flash programmed.
 *   - The full or part of source code is prohibited to distribute without
 *     permission from TOCOS.
 *
 ****************************************************************************/

#ifndef  ADXL345_INCLUDED
#define  ADXL345_INCLUDED

#if defined __cplusplus
extern "C" {
#endif

/****************************************************************************/
/***        Include Files                                                 ***/
/****************************************************************************/
#include "appsave.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#define ADXL345_IDX_X 0
#define ADXL345_IDX_Y 1
#define ADXL345_IDX_Z 2

#define ADXL345_IDX_BEGIN 0
#define ADXL345_IDX_END (ADXL345_IDX_Z+1) // should be (last idx + 1)

#ifdef LITE2525A
#define ADXL345_ADDRESS		(0x1D)
#else
#define ADXL345_ADDRESS		(0x53)
#endif

#define ADXL345_CONVTIME    (0)//(24+2) // 24ms MAX

#define ADXL345_DATA_NOTYET	(-32768)
#define ADXL345_DATA_ERROR	(-32767)

#define ADXL345_THRESH_TAP		0x1D
#define ADXL345_OFSX			0x1E
#define ADXL345_OFSY			0x1F
#define ADXL345_OFSZ			0x20
#define ADXL345_DUR				0x21
#define ADXL345_LATENT			0x22
#define ADXL345_WINDOW			0x23
#define ADXL345_THRESH_ACT		0x24
#define ADXL345_THRESH_INACT	0x25
#define ADXL345_TIME_INACT		0x26
#define ADXL345_ACT_INACT_CTL	0x27
#define ADXL345_THRESH_FF		0x28
#define ADXL345_TIME_FF			0x29
#define ADXL345_TAP_AXES		0x2A
#define ADXL345_ACT_TAP_STATUS	0x2B
#define ADXL345_BW_RATE			0x2C
#define ADXL345_POWER_CTL		0x2D
#define ADXL345_INT_ENABLE		0x2E
#define ADXL345_INT_MAP			0x2F
#define ADXL345_INT_SOURCE		0x30
#define ADXL345_DATA_FORMAT		0x31
#define ADXL345_DATAX0			0x32
#define ADXL345_DATAX1			0x33
#define ADXL345_DATAY0			0x34
#define ADXL345_DATAY1			0x35
#define ADXL345_DATAZ0			0x36
#define ADXL345_DATAZ1			0x37
#define ADXL345_FIFO_CTL		0x38
#define ADXL345_FIFO_STATUS		0x39

#define NORMAL				0
#define S_TAP				1
#define D_TAP				2
#define FREEFALL			4
#define ACTIVE				8
#define INACTIVE			16
#define SHAKE				32
#define SHAKE_ACC1			33
#define SHAKE_ACC2			32+2
#define SHAKE_ACC3			32+3
#define SHAKE_HOLD			32+4
#define SHAKE_FAN			32+5
#define NEKOTTER			256
#define DICE				512

#define READ_FIFO 12

#define TH_ACCEL 120
#define TH_COUNT 0

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/
typedef struct {
	// protected
	bool_t	bBusy;			// should block going into sleep

	// data
	int16	ai16Result[3];
	uint8	u8Interrupt;

	// working
	uint8	u8TickCount, u8TickWait;
} tsObjData_ADXL345;

/****************************************************************************/
/***        Exported Functions (state machine)                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions (primitive funcs)                          ***/
/****************************************************************************/
void vADXL345_Init(tsObjData_ADXL345 *pData, tsSnsObj *pSnsObj );
bool_t bADXL345_Setting( int16 i16mode, tsADXL345Param sParam, bool_t bLink );
void vADXL345_Final(tsObjData_ADXL345 *pData, tsSnsObj *pSnsObj);

PUBLIC bool_t bADXL345reset();
PUBLIC bool_t bADXL345startRead();
PUBLIC int16 i16ADXL345readResult( uint8 u8axis );
PUBLIC bool_t bNekotterreadResult( int16* ai16accel );
PUBLIC bool_t bShakereadResult( int16* ai16accel );
uint8 u8Read_Interrupt( void );
bool_t bSetFIFO( void );

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/
extern uint8 u8Interrupt;

#if defined __cplusplus
}
#endif

#endif  /* ADXL345_INCLUDED */

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/

