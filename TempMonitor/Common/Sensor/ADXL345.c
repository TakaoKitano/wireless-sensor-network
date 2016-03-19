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
#include "ADXL345.h"
#include "SMBus.h"

#include "ccitt8.h"

#include "utils.h"

#include "Interactive.h"

#include "EndDevice_Input.h"

# include <serial.h>
# include <fprintf.h>
#undef SERIAL_DEBUG
#ifdef SERIAL_DEBUG
extern tsFILE sDebugStream;
#endif


/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#define ADXL345_X	ADXL345_DATAX0
#define ADXL345_Y	ADXL345_DATAY0
#define ADXL345_Z	ADXL345_DATAZ0

const uint8 ADXL345_AXIS[] = {
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
PRIVATE void vProcessSnsObj_ADXL345(void *pvObj, teEvent eEvent);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
static uint16 u16modeflag = 0x00;
static uint16 u16ThAccel = TH_ACCEL;

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/
void vADXL345_Init(tsObjData_ADXL345 *pData, tsSnsObj *pSnsObj) {
	vSnsObj_Init(pSnsObj);

	pSnsObj->pvData = (void*)pData;
	pSnsObj->pvProcessSnsObj = (void*)vProcessSnsObj_ADXL345;

	memset((void*)pData, 0, sizeof(tsObjData_ADXL345));
}

void vADXL345_Final(tsObjData_ADXL345 *pData, tsSnsObj *pSnsObj) {
	pSnsObj->u8State = E_SNSOBJ_STATE_INACTIVE;
}

//	センサの設定を記述する関数
bool_t bADXL345_Setting( int16 i16mode, tsADXL345Param sParam, bool_t bLink )
{
	u16modeflag = (uint16)i16mode;
	u16modeflag = ((i16mode&SHAKE) != 0) ? SHAKE : u16modeflag;

	uint8 com;
	if( u16modeflag == NEKOTTER || u16modeflag == SHAKE ){
		switch(sParam.u16Duration){
//		case 1:
//			com = 0x14;		//	Low Power Mode, 1.56Hz Sampling frequency
//			break;
//		case 3:
//			com = 0x15;		//	Low Power Mode, 3.13Hz Sampling frequency
//			break;
		case 6:
			com = 0x16;		//	Low Power Mode, 6.25Hz Sampling frequency
			break;
		case 12:
			com = 0x17;		//	Low Power Mode, 12.5Hz Sampling frequency
			break;
		case 25:
			com = 0x18;		//	Low Power Mode, 25Hz Sampling frequency
			break;
		case 50:
			com = 0x19;		//	Low Power Mode, 50Hz Sampling frequency
			break;
		case 100:
			com = 0x1A;		//	Low Power Mode, 100Hz Sampling frequency
			break;
		case 200:
			com = 0x1B;		//	Low Power Mode, 200Hz Sampling frequency
			break;
		case 400:
			com = 0x0C;		//	400Hz Sampling frequency
			break;
//		case 800:
//			com = 0x0D;		//	800Hz Sampling frequency
//			break;
//		case 1600:
//			com = 0x0E;		//	1600Hz Sampling frequency
//			break;
//		case 3200:
//			com = 0x0F;		//	3200Hz Sampling frequency
//			break;
		default:
			com = 0x08;		//	Low Power Mode, 25Hz Sampling frequency
			break;
		}
	}else{
		com = 0x1A;		//	Low Power Mode, 100Hz Sampling frequency
	}
	bool_t bOk = bSMBusWrite(ADXL345_ADDRESS, ADXL345_BW_RATE, 1, &com );

	com = 0x0B;		//	Full Resolution Mode, +-16g
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_DATA_FORMAT, 1, &com );

	if(bLink){
		com = 0x08;		//	Start Measuring
	}else{
		com = 0x28;		//	Link(Active -> Inactive -> Active ...), Start Measuring
	}
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_POWER_CTL, 1, &com );

	uint16 u16tempcom;
	//	タップを判別するための閾値
	if( (u16modeflag&S_TAP) || (u16modeflag&D_TAP) ){
		if( sParam.u16ThresholdTap != 0 ){
			u16tempcom = sParam.u16ThresholdTap*10/625;
			if( 0x00FF < u16tempcom ){
				com = 0xFF;
			}else{
				com = (uint8)u16tempcom;
			}
		}else{
			com = 0x32;			//	threshold of tap
		}
	}else{
		com = 0x00;
	}
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_THRESH_TAP, 1, &com );

	//	タップを認識するための時間
	if( (u16modeflag&S_TAP) || (u16modeflag&D_TAP) ){
		if( sParam.u16Duration != 0 ){
			u16tempcom = sParam.u16Duration*10/625;
			if( 0x00FF < u16tempcom ){
				com = 0xFF;
			}else{
				com = (uint8)u16tempcom;
			}
		}else{
			com = 0x0F;
		}
	}else{
		com = 0x00;
	}
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_DUR, 1, &com );

	//	次のタップまでの時間
	if( u16modeflag&D_TAP ){
		if( sParam.u16Latency != 0 ){
			u16tempcom = sParam.u16Latency*100/125;
			if( 0x00FF < u16tempcom ){
				com = 0xFF;
			}else{
				com = (uint8)u16tempcom;
			}
		}else{
			com = 0x50;
		}
	}else{
		com = 0x00;
	}
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_LATENT, 1, &com );

	//	ダブルタップを認識するための時間
	if( u16modeflag&D_TAP ){
		if( sParam.u16Window != 0 ){
			u16tempcom = sParam.u16Window*100/125;
			if( 0x00FF < u16tempcom ){
				com = 0xFF;
			}else{
				com = (uint8)u16tempcom;
			}
		}else{
			com = 0xD9;			// Window Width
		}
	}else{
		com = 0x00;
	}
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_WINDOW, 1, &com );

	//	タップを検知する軸の設定
	if( (u16modeflag&S_TAP) || (u16modeflag&D_TAP) ){
		com = 0x07;
	}else{
		com = 0x00;
	}
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_TAP_AXES, 1, &com );

	//	自由落下を検知するための閾値
	if( (u16modeflag&FREEFALL) != 0 && u16modeflag < SHAKE ){
		if( sParam.u16ThresholdFreeFall != 0 ){
			u16tempcom = sParam.u16ThresholdFreeFall*10/625;
			if( 0x00FF < u16tempcom ){
				com = 0xFF;
			}else{
				com = (uint8)u16tempcom;
			}
		}else{
			com = 0x07;			// threshold of freefall
		}
	}else{
		com = 0x00;
	}
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_THRESH_FF, 1, &com );

	//	自由落下を検知するための時間
	if( u16modeflag&FREEFALL ){
		if( sParam.u16TimeFreeFall != 0 ){
			u16tempcom = sParam.u16TimeFreeFall/5;
			if( 0x00FF < u16tempcom ){
				com = 0xFF;
			}else{
				com = (uint8)u16tempcom;
			}
		}else{
			com = 0x2D;			// time of freefall
		}
	}else{
		com = 0x00;
	}
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_TIME_FF, 1, &com );

	//	動いていることを判断するための閾値
	if( u16modeflag&ACTIVE ){
		if( sParam.u16ThresholdActive != 0 ){
			u16tempcom = sParam.u16ThresholdActive*10/625;
			if( 0x00FF < u16tempcom ){
				com = 0xFF;
			}else{
				com = (uint8)u16tempcom;
			}
		}else{
			com = 0x15;
		}
	}else{
		com = 0x00;
	}
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_THRESH_ACT, 1, &com );

	//	動いていないことを判断するための閾値
	if( u16modeflag&ACTIVE ){
		if( sParam.u16ThresholdInactive != 0 ){
			u16tempcom = sParam.u16ThresholdInactive*10/625;
			if( 0x00FF < u16tempcom ){
				com = 0xFF;
			}else{
				com = (uint8)u16tempcom;
			}
		} else {
			com = 0x14;
		}
	}else{
		com = 0x00;
	}
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_THRESH_INACT, 1, &com );

	//	動いていないことを判断するための時間(s)
	if( u16modeflag&ACTIVE ){
		if( sParam.u16TimeInactive != 0 ){
			u16tempcom = sParam.u16TimeInactive;
			if( 0x00FF < u16tempcom ){
				com = 0xFF;
			}else{
				com = (uint8)u16tempcom;
			}
		} else {
			com = 0x02;
		}
	}else{
		com = 0x00;
	}
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_TIME_INACT, 1, &com );

	//	動いている/いないことを判断するための軸
	if( u16modeflag&ACTIVE ){
		com = 0x77;
	}else{
		com = 0x00;
	}
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_ACT_INACT_CTL, 1, &com );

	//	割り込みピンの設定
	if( u16modeflag > 0  && u16modeflag < 32){
		com = 0x10;		//	ACTIVEは別ピン
	}else{
		com = 0x00;
	}
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_INT_MAP, 1, &com );

	//	有効にする割り込みの設定
	com = 0;
	if( (u16modeflag&SHAKE) != 0 ){
		com += 0x02;
	}else{
		if( u16modeflag&S_TAP ){
			com += 0x40;
		}
		if( u16modeflag&D_TAP ){
			com += 0x20;
		}
		if( u16modeflag&FREEFALL ){
			com += 0x04;
		}
		if( u16modeflag&ACTIVE ){
			com += 0x18;		//	INACTIVEも割り込みさせる
		}
	}
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_INT_ENABLE, 1, &com );

	if( u16modeflag == NEKOTTER || u16modeflag == SHAKE ){
		com = 0xC0 | 0x20 | READ_FIFO;
	}else{
		com = 0x00;
	}
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_FIFO_CTL, 1, &com );

	if( u16modeflag == SHAKE ){
		if( sParam.u16ThresholdTap != 0 ){
			u16ThAccel = sParam.u16ThresholdTap;
		}
	}

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
PUBLIC bool_t bADXL345reset()
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
PUBLIC bool_t bADXL345startRead()
{
	bool_t	bOk = TRUE;

	return bOk;
}

/****************************************************************************
 *
 * NAME: u16ADXL345readResult
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
PUBLIC int16 i16ADXL345readResult( uint8 u8axis )
{
	bool_t	bOk = TRUE;
	int16	i16result=0;
	uint8	au8data[2];

	//	各軸の読み込み
	switch( u8axis ){
		case ADXL345_IDX_X:
			bOk &= bGetAxis( ADXL345_IDX_X, au8data );
			break;
		case ADXL345_IDX_Y:
			bOk &= bGetAxis( ADXL345_IDX_Y, au8data );
			break;
		case ADXL345_IDX_Z:
			bOk &= bGetAxis( ADXL345_IDX_Z, au8data );
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

PUBLIC bool_t bNekotterreadResult( int16* ai16accel )
{
	bool_t	bOk = TRUE;
	int16	ai16result[3];
	uint8	au8data[2];
	int8	num;
	uint8	data;
	uint8	i, j;
	int16	ave;
	int16	sum[3] = { 0, 0, 0 };		//	サンプルの総和
	uint32	ssum[3] = { 0, 0, 0 };		//	サンプルの2乗和

	//	FIFOでたまった個数を読み込む
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_FIFO_STATUS, 0, NULL );
	bOk &= bSMBusSequentialRead( ADXL345_ADDRESS, 1, &data );

	num = (int8)(data&0x7f);
	for( i=0; i<num; i++ ){
		//	各軸の読み込み
		//	X軸
		bOk &= bGetAxis( ADXL345_IDX_X, au8data );
		ai16result[ADXL345_IDX_X] = (((au8data[1] << 8) | au8data[0]));
		//	Y軸
		bOk &= bGetAxis( ADXL345_IDX_Y, au8data );
		ai16result[ADXL345_IDX_Y] = (((au8data[1] << 8) | au8data[0]));
		//	Z軸
		bOk &= bGetAxis( ADXL345_IDX_Z, au8data );
		ai16result[ADXL345_IDX_Z] = (((au8data[1] << 8) | au8data[0]));

		//	総和と二乗和の計算
		for( j=0; j<3; j++ ){
			sum[j] += ai16result[j];
			ssum[j] += ai16result[j]*ai16result[j];
		}
		//vfPrintf(& sSerStream, "\n\r%2d:%d,%d,%d", i, ai16result[ADXL345_IDX_X], ai16result[ADXL345_IDX_Y], ai16result[ADXL345_IDX_Z]);
		//SERIAL_vFlush(E_AHI_UART_0);
	}

	for( i=0; i<3; i++ ){
	//	分散が評価値 分散の式を変形
		ave = sum[i]/num;
		ai16accel[i] = (int16)sqrt((double)(-1*(ave*ave)+(ssum[i]/num)));
	}

	//	ねこったーモードはじめ
	//	FIFOの設定をもう一度
	bOk &= bSetFIFO();
	//	終わり

    return bOk;
}

#define MY_ABS(c) ( c<0 ? -1*c : c )

PUBLIC bool_t bShakereadResult( int16* ai16accel )
{
	static 	int16	ai16TmpAccel[3]={0, 0, 0};
	bool_t	bOk = TRUE;
	uint8	au8data[2];
	int16	max = 0x8000;
	uint8	num;				//	FIFOのデータ数
	uint8	i;
	int16	sum[32];
	uint8	count = 0;
	int16	x[33];
	int16	y[33];
	int16	z[33];

	//	FIFOでたまった個数を読み込む
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_FIFO_STATUS, 0, NULL );
	bOk &= bSMBusSequentialRead( ADXL345_ADDRESS, 1, &num );

	//	FIFOの中身を全部読む
	num = (num&0x7f);
	if( num == READ_FIFO ){
		//	各軸の読み込み
		for( i=0; i<num; i++ ){
			//	X軸
			bOk &= bGetAxis( ADXL345_IDX_X, au8data );
			x[i] = (((au8data[1] << 8) | au8data[0]));
		}
		for( i=0; i<num; i++ ){
			//	Y軸
			bOk &= bGetAxis( ADXL345_IDX_Y, au8data );
			y[i] = (((au8data[1] << 8) | au8data[0]));
		}
		for( i=0; i<num; i++ ){
			//	Z軸
			bOk &= bGetAxis( ADXL345_IDX_Z, au8data );
			z[i] = (((au8data[1] << 8) | au8data[0]));
		}
		//	FIFOの設定をもう一度
		bOk &= bSetFIFO();

		for( i=0; i<num; i++ ){
			x[i] = (x[i]<<2)/10;
			y[i] = (y[i]<<2)/10;
			z[i] = (z[i]<<2)/10;

			if( i == 0 ){
				sum[i] = ( x[i]-ai16TmpAccel[0] + y[i]-ai16TmpAccel[1] + z[i]-ai16TmpAccel[2] );
			}else{
				sum[i] = ( x[i]-x[i-1] + y[i]-y[i-1] + z[i]-z[i-1] );
			}

			if( sum[i] < 0 ){
				sum[i] *= -1;
			}

			max = sum[i]>max ? sum[i] : max;

			if( sum[i] > u16ThAccel ){
				count++;
			}
#if 0
			vfPrintf(& sSerStream, "\n\r%2d:%d,%d,%d %d", i, x[i], y[i], z[i], sum[i] );
			SERIAL_vFlush(E_AHI_UART_0);
		}
		vfPrintf( &sSerStream, "\n\r" );
#else
		}
#endif
		ai16accel[0] = max;
		ai16accel[1] = z[0];
		ai16accel[2] = count;
		ai16TmpAccel[0] = x[num-1];
		ai16TmpAccel[1] = y[num-1];
		ai16TmpAccel[2] = z[num-1];
	}else{
		//	FIFOの設定をもう一度
		bOk &= bSetFIFO();
		ai16accel[0] = 0;
		ai16accel[1] = 0;
		ai16accel[2] = 0;
	}

	//	終わり

    return bOk;
}

uint8 u8Read_Interrupt( void )
{
	uint8	u8source;
	bool_t bOk = TRUE;

	bOk &= bSMBusWrite( ADXL345_ADDRESS, 0x30, 0, NULL );
	bOk &= bSMBusSequentialRead( ADXL345_ADDRESS, 1, &u8source );

	if(!bOk){
		u8source = 0xFF;
	}

	return u8source;
}

bool_t bSetFIFO( void )
{
	//	FIFOの設定をもう一度
	uint8 com = 0x00 | 0x20 | READ_FIFO;
	bool_t bOk = bSMBusWrite(ADXL345_ADDRESS, ADXL345_FIFO_CTL, 1, &com );
	com = 0xC0 | 0x20 | READ_FIFO;
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_FIFO_CTL, 1, &com );
	//	終わり

    return bOk;
}
/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/
PRIVATE bool_t bGetAxis( uint8 u8axis, uint8* au8data )
{
	bool_t bOk = TRUE;

	bOk &= bSMBusWrite( ADXL345_ADDRESS, ADXL345_AXIS[u8axis], 0, NULL );
	bOk &= bSMBusSequentialRead( ADXL345_ADDRESS, 2, au8data );

	return bOk;
}

// the Main loop
PRIVATE void vProcessSnsObj_ADXL345(void *pvObj, teEvent eEvent) {
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
			pObj->u8Interrupt = u8Interrupt;
			pObj->ai16Result[ADXL345_IDX_X] = SENSOR_TAG_DATA_ERROR;
			pObj->ai16Result[ADXL345_IDX_Y] = SENSOR_TAG_DATA_ERROR;
			pObj->ai16Result[ADXL345_IDX_Z] = SENSOR_TAG_DATA_ERROR;
			pObj->u8TickWait = ADXL345_CONVTIME;

			pObj->bBusy = TRUE;
#ifdef ADXL345_ALWAYS_RESET
			u8reset_flag = TRUE;
			if (!bADXL345reset()) {
				vSnsObj_NewState(pSnsObj, E_SNSOBJ_STATE_COMPLETE);
			}
#else
			//if (!bADXL345startRead()) { // kick I2C communication
			//	vSnsObj_NewState(pSnsObj, E_SNSOBJ_STATE_COMPLETE);
			//}
#endif
			pObj->u8TickCount = 0;
			break;

		default:
			break;
		}

		// wait until completion
		if (pObj->u8TickCount > pObj->u8TickWait) {
#ifdef ADXL345_ALWAYS_RESET
			if (u8reset_flag) {
				u8reset_flag = 0;
				if (!bADXL345startRead()) {
					vADXL345_new_state(pObj, E_SNSOBJ_STATE_COMPLETE);
				}

				pObj->u8TickCount = 0;
				pObj->u8TickWait = ADXL345_CONVTIME;
				break;
			}
#endif
			if( u16modeflag != NEKOTTER && u16modeflag != SHAKE ){
				pObj->ai16Result[ADXL345_IDX_X] = i16ADXL345readResult(ADXL345_IDX_X);
				pObj->ai16Result[ADXL345_IDX_Y] = i16ADXL345readResult(ADXL345_IDX_Y);
				pObj->ai16Result[ADXL345_IDX_Z] = i16ADXL345readResult(ADXL345_IDX_Z);
			}else if(u16modeflag == NEKOTTER){
				bNekotterreadResult(pObj->ai16Result);
			}else if(u16modeflag == SHAKE){
				bShakereadResult(pObj->ai16Result);
			}

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
