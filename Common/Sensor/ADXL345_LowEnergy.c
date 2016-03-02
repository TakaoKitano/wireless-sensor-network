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

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include<math.h>

#include "jendefs.h"
#include "AppHardwareApi.h"
#include "string.h"
#include "fprintf.h"

#include "sensor_driver.h"
#include "ADXL345_LowEnergy.h"
#include "SMBus.h"

#include "ccitt8.h"

#include "utils.h"

#include "Interactive.h"

#undef SERIAL_DEBUG
#ifdef SERIAL_DEBUG
# include <serial.h>
# include <fprintf.h>
extern tsFILE sDebugStream;
#endif
tsFILE sSerStream;

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#ifdef LITE2525A
#define ADXL345_ADDRESS		(0x1D)
#else
#define ADXL345_ADDRESS		(0x53)
#endif

#define ADXL345_CONVTIME    (10)//(24+2) // 24ms MAX

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

#define ADXL345_X	ADXL345_DATAX0
#define ADXL345_Y	ADXL345_DATAY0
#define ADXL345_Z	ADXL345_DATAZ0

const uint8 ADXL345_LOWENERGY_AXIS[] = {
		ADXL345_X,
		ADXL345_Y,
		ADXL345_Z
};


/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
PRIVATE bool_t bGetAxis( uint8 u8axis, uint8* au8data );
PRIVATE void vProcessSnsObj_ADXL345_LowEnergy(void *pvObj, teEvent eEvent);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/
void vADXL345_LowEnergy_Init(tsObjData_ADXL345 *pData, tsSnsObj *pSnsObj) {
	vSnsObj_Init(pSnsObj);

	pSnsObj->pvData = (void*)pData;
	pSnsObj->pvProcessSnsObj = (void*)vProcessSnsObj_ADXL345_LowEnergy;

	memset((void*)pData, 0, sizeof(tsObjData_ADXL345));
}

void vADXL345_LowEnergy_Final(tsObjData_ADXL345 *pData, tsSnsObj *pSnsObj) {
	pSnsObj->u8State = E_SNSOBJ_STATE_INACTIVE;
}

//	センサの設定を記述する関数
bool_t bADXL345_LowEnergy_Setting()
{
	bool_t bOk = TRUE;

	uint8 com = 0x19;		//	Low Power Mode, 100Hz Sampling frequency
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_BW_RATE, 1, &com );
	com = 0x0B;		//	Full Resolution Mode, +-16g
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_DATA_FORMAT, 1, &com );
	return bOk;
}

/****************************************************************************
 *
 * NAME: bADXL345reset
 *
 * DESCRIPTION:
 *   to reset ADXL345 device
 *
 * RETURNS:
 * bool_t	fail or success
 *
 ****************************************************************************/
PUBLIC bool_t bADXL345_LowEnergyReset()
{
	bool_t bOk = TRUE;
	return bOk;
}

/****************************************************************************
 *
 * NAME: vHTSstartReadTemp
 *
 * DESCRIPTION:
 * Wrapper to start a read of the temperature sensor.
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PUBLIC bool_t bADXL345_LowEnergyStartRead()
{

	uint8 com = 0x08;		//	Start Measuring
	bool_t bOk = bSMBusWrite(ADXL345_ADDRESS, ADXL345_POWER_CTL, 1, &com );

	return bOk;
}

/****************************************************************************
 *
 * NAME: u16ADXL345_LowEnergyreadResult
 *
 * DESCRIPTION:
 * Wrapper to read a measurement, followed by a conversion function to work
 * out the value in degrees Celcius.
 *
 * RETURNS:
 * int16: 0~10000 [1 := 5Lux], 100 means 500 Lux.
 *        0x8000, error
 *
 * NOTES:
 * the data conversion fomula is :
 *      ReadValue / 1.2 [LUX]
 *
 ****************************************************************************/
PUBLIC int16 i16ADXL345_LowEnergyReadResult( uint8 u8axis )
{
	bool_t	bOk = TRUE;
	int16	i16result=0;
	uint8	au8data[2];

	//	各軸の読み込み
	switch( u8axis ){
		case ADXL345_LOWENERGY_IDX_X:
			bOk &= bGetAxis( ADXL345_LOWENERGY_IDX_X, au8data );
			break;
		case ADXL345_LOWENERGY_IDX_Y:
			bOk &= bGetAxis( ADXL345_LOWENERGY_IDX_Y, au8data );
			break;
		case ADXL345_LOWENERGY_IDX_Z:
			bOk &= bGetAxis( ADXL345_LOWENERGY_IDX_Z, au8data );
			break;
		default:
			bOk = FALSE;
	}
	i16result = (((au8data[1] << 8) | au8data[0]));
	i16result = i16result*4/10;			//	1bitあたり4mg  10^-2まで有効

	if (bOk == FALSE) {
		i16result = SENSOR_TAG_DATA_ERROR;
	}


	return i16result;
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/
PRIVATE bool_t bGetAxis( uint8 u8axis, uint8* au8data )
{
	bool_t bOk = TRUE;

	bOk &= bSMBusWrite( ADXL345_ADDRESS, ADXL345_LOWENERGY_AXIS[u8axis], 0, NULL );
	bOk &= bSMBusSequentialRead( ADXL345_ADDRESS, 2, au8data );

	return bOk;
}

// the Main loop
PRIVATE void vProcessSnsObj_ADXL345_LowEnergy(void *pvObj, teEvent eEvent) {
	tsSnsObj *pSnsObj = (tsSnsObj *)pvObj;
	tsObjData_ADXL345 *pObj = (tsObjData_ADXL345 *)pSnsObj->pvData;

	// general process (independent from each state)
	switch (eEvent) {
		case E_EVENT_TICK_TIMER:
			if (pObj->u8TickCount < 100) {
				pObj->u8TickCount += pSnsObj->u8TickDelta;
#ifdef SERIAL_DEBUG
vfPrintf(&sDebugStream, "+");
#endif
			}
			break;
		case E_EVENT_START_UP:
			pObj->u8TickCount = 100; // expire immediately
#ifdef SERIAL_DEBUG
vfPrintf(&sDebugStream, "\n\rADXL345 WAKEUP");
#endif
			break;
		default:
			break;
	}

	// state machine
	switch(pSnsObj->u8State)
	{
	case E_SNSOBJ_STATE_INACTIVE:
		// do nothing until E_ORDER_INITIALIZE event
		break;

	case E_SNSOBJ_STATE_IDLE:
		switch (eEvent) {
		case E_EVENT_NEW_STATE:
			break;

		case E_ORDER_KICK:
			vSnsObj_NewState(pSnsObj, E_SNSOBJ_STATE_MEASURING);

			#ifdef SERIAL_DEBUG
			vfPrintf(&sDebugStream, "\n\rADXL345 KICKED");
			#endif

			break;

		default:
			break;
		}
		break;

	case E_SNSOBJ_STATE_MEASURING:
		switch (eEvent) {
		case E_EVENT_NEW_STATE:
			pObj->ai16Result[ADXL345_LOWENERGY_IDX_X] = SENSOR_TAG_DATA_ERROR;
			pObj->ai16Result[ADXL345_LOWENERGY_IDX_Y] = SENSOR_TAG_DATA_ERROR;
			pObj->ai16Result[ADXL345_LOWENERGY_IDX_Z] = SENSOR_TAG_DATA_ERROR;
			pObj->u8TickWait = ADXL345_CONVTIME;

			pObj->bBusy = TRUE;
#ifdef ADXL345_ALWAYS_RESET
			u8reset_flag = TRUE;
			if (!bADXL345reset()) {
				vSnsObj_NewState(pSnsObj, E_SNSOBJ_STATE_COMPLETE);
			}
#else
			if (!bADXL345_LowEnergyStartRead()) { // kick I2C communication
				vSnsObj_NewState(pSnsObj, E_SNSOBJ_STATE_COMPLETE);
			}
#endif
			pObj->u8TickCount = 0;
			break;

		default:
			break;
		}

		// wait until completion
		if (pObj->u8TickCount > pObj->u8TickWait) {
			pObj->ai16Result[ADXL345_LOWENERGY_IDX_X] = i16ADXL345_LowEnergyReadResult(ADXL345_LOWENERGY_IDX_X);
			pObj->ai16Result[ADXL345_LOWENERGY_IDX_Y] = i16ADXL345_LowEnergyReadResult(ADXL345_LOWENERGY_IDX_Y);
			pObj->ai16Result[ADXL345_LOWENERGY_IDX_Z] = i16ADXL345_LowEnergyReadResult(ADXL345_LOWENERGY_IDX_Z);

			uint8 com = 0x00;		//	End Measuring
			bSMBusWrite(ADXL345_ADDRESS, ADXL345_POWER_CTL, 1, &com );

			// data arrival
			pObj->bBusy = FALSE;
			vSnsObj_NewState(pSnsObj, E_SNSOBJ_STATE_COMPLETE);
		}
		break;

	case E_SNSOBJ_STATE_COMPLETE:
		switch (eEvent) {
		case E_EVENT_NEW_STATE:
			#ifdef SERIAL_DEBUG
			vfPrintf(&sDebugStream, "\n\rADXL345_CP: %d", pObj->i16Result);
			#endif

			break;

		case E_ORDER_KICK:
			// back to IDLE state
			vSnsObj_NewState(pSnsObj, E_SNSOBJ_STATE_IDLE);
			break;

		default:
			break;
		}
		break;

	default:
		break;
	}
}
/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
