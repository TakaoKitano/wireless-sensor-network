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
#include "jendefs.h"
#include "AppHardwareApi.h"
#include "string.h"

#include "sensor_driver.h"
#include "ADT7410.h"
#include "SMBus.h"

#include "ccitt8.h"

#undef SERIAL_DEBUG
#ifdef SERIAL_DEBUG
# include <serial.h>
# include <fprintf.h>
extern tsFILE sSerStream;
tsFILE sDebugStream;
#endif

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#define ADT7410_ADDRESS     (0x48)

#define ADT7410_TRIG        (0x23)

#define ADT7410_CONVTIME    (24+2) // 24ms MAX

#define ADT7410_SOFT_RST    (0x2F)

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
PRIVATE void vProcessSnsObj_ADT7410(void *pvObj, teEvent eEvent);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/
void vADT7410_Init(tsObjData_ADT7410 *pData, tsSnsObj *pSnsObj) {
	vSnsObj_Init(pSnsObj);

	pSnsObj->pvData = (void*)pData;
	pSnsObj->pvProcessSnsObj = (void*)vProcessSnsObj_ADT7410;

	memset((void*)pData, 0, sizeof(tsObjData_ADT7410));
#ifdef SERIAL_DEBUG
	sDebugStream = sSerStream;
#endif
}

void vADT7410_Final(tsObjData_ADT7410 *pData, tsSnsObj *pSnsObj) {
	pSnsObj->u8State = E_SNSOBJ_STATE_INACTIVE;
}


/****************************************************************************
 *
 * NAME: bADT7410reset
 *
 * DESCRIPTION:
 *   to reset ADT7410 device
 *
 * RETURNS:
 * bool_t	fail or success
 *
 ****************************************************************************/
PUBLIC bool_t bADT7410reset( bool_t bMode16 )
{
	bool_t bOk = TRUE;
	uint8 u8conf = 0x80;		//	16bit mode

	bOk &= bSMBusWrite(ADT7410_ADDRESS, ADT7410_SOFT_RST, 0, NULL);

	//	16bitモードの場合 設定変更
	if( bMode16 == TRUE ){
		bOk &= bSMBusWrite(ADT7410_ADDRESS, 0x03, 1, &u8conf );
	}
	// then will need to wait at least 15ms

	return bOk;
}

/****************************************************************************
 *
 * NAME: vHTSstartRead
 *
 * DESCRIPTION:
 * Wrapper to start a read of the temperature sensor.
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PUBLIC bool_t bADT7410startRead(bool_t bMode16)
{
	bool_t bOk = TRUE;
        uint8 u8confOneShotMode16 = 0xa0;       //      16bit mode
        uint8 u8confOneShotMode13 = 0x20;       //      13bit mode

        if( bMode16 == TRUE ){
                bOk &= bSMBusWrite(ADT7410_ADDRESS, 0x03, 1, &u8confOneShotMode16 );
        } else {
                bOk &= bSMBusWrite(ADT7410_ADDRESS, 0x03, 1, &u8confOneShotMode13 );
        }

        return bOk;
}

/****************************************************************************
 *
 * NAME: u16ADT7410readResult
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
PUBLIC int16 i16ADT7410readResult( bool_t bMode16 )
{
	bool_t bOk = TRUE;
	uint16 u16result;
	int32 i32result;
	float temp;
	uint8 au8data[2];

	//	レジスタ0x00を読み込む宣言
	bOk &= bSMBusWrite(ADT7410_ADDRESS, 0x00, 0, NULL);
	bOk &= bSMBusSequentialRead(ADT7410_ADDRESS, 2, au8data);
	if (bOk == FALSE) {
	    	i32result = SENSOR_TAG_DATA_ERROR;
	}

	u16result = ((au8data[0] << 8) | au8data[1]);	//	読み込んだ数値を代入
	if ( bMode16 == FALSE ) {		//	13bitモード
	    	i32result = (int32)u16result >> 3;
    		//	符号判定
	    	if(u16result & 0x8000 ){
    			i32result -= 8192;
    		}
    		temp = (float)i32result/16.0;
	} else {		//	16bitモード
    		i32result = (int32)u16result;
    		//	符号判定
    		if(u16result & 0x8000){
    			i32result -= 65536;
    		}
    		temp = (float)i32result/128.0;
	}

#ifdef SERIAL_DEBUG
vfPrintf(&sDebugStream, "\n\rADT7410 DATA %x", *((uint16*)au8data) );
#endif

	return (int16)(temp*100);
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/
// the Main loop
PRIVATE void vProcessSnsObj_ADT7410(void *pvObj, teEvent eEvent) {
	tsSnsObj *pSnsObj = (tsSnsObj *)pvObj;
	tsObjData_ADT7410 *pObj = (tsObjData_ADT7410 *)pSnsObj->pvData;

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
                        if (!bADT7410startRead(TRUE)) {
                                vSnsObj_NewState(pSnsObj, E_SNSOBJ_STATE_COMPLETE);
                        }
			vSnsObj_NewState(pSnsObj, E_SNSOBJ_STATE_MEASURING);

			#ifdef SERIAL_DEBUG
			vfPrintf(&sDebugStream, "\n\rADT7410 KICKED");
			#endif

			break;

		default:
			break;
		}
		break;

	case E_SNSOBJ_STATE_MEASURING:
		switch (eEvent) {

                case E_EVENT_START_UP:
                        pObj->i16Result = i16ADT7410readResult(TRUE);
                        vSnsObj_NewState(pSnsObj, E_SNSOBJ_STATE_COMPLETE);
			break;

		default:
			break;
		}
		break;

	case E_SNSOBJ_STATE_COMPLETE:
		switch (eEvent) {
		case E_EVENT_NEW_STATE:
			#ifdef SERIAL_DEBUG
			vfPrintf(&sDebugStream, "\n\rADT7410_CP: %d", pObj->i16Result);
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
