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

#ifndef  BME280_INCLUDED
#define  BME280_INCLUDED

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
#define BME280_IDX_X 0
#define BME280_IDX_Y 1
#define BME280_IDX_Z 2

#define BME280_IDX_BEGIN 0
#define BME280_IDX_END (BME280_IDX_Z+1) // should be (last idx + 1)
/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/
typedef struct {
	// protected
	bool_t	bBusy;			// should block going into sleep

	// data
	int16	i16Temp;
	uint16	u16Hum;
	uint16	u16Pres;

	// working
	uint8	u8TickCount, u8TickWait;
} tsObjData_BME280;

/****************************************************************************/
/***        Exported Functions (state machine)                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions (primitive funcs)                          ***/
/****************************************************************************/
void vBME280_Init(tsObjData_BME280 *pData, tsSnsObj *pSnsObj );
bool_t bBME280_Setting();
void vBME280_Final(tsObjData_BME280 *pData, tsSnsObj *pSnsObj);

PUBLIC bool_t bBME280Reset();
PUBLIC bool_t bBME280StartRead();
PUBLIC int16 i16BME280ReadResult( int16* Temp, uint16* Pres, uint16* Hum );

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/
#if defined __cplusplus
}
#endif

#endif  /* BME280_INCLUDED */

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/

