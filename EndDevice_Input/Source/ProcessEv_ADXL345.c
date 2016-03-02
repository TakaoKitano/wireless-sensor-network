/****************************************************************************
 * (C) Tokyo Cosmos Electric, Inc. (TOCOS) - all rights reserved.
 *
 * Condition to use: (refer to detailed conditions in Japanese)
 *   - The full or part of source code is limited to use for TWE (TOCOS
 *     Wireless Engine) as compiled and flash programmed.
 *   - The full or part of source code is prohibited to distribute without
 *     permission from TOCOS.
 *
 * 利用条件:
 *   - 本ソースコードは、別途ソースコードライセンス記述が無い限り東京コスモス電機が著作権を
 *     保有しています。
 *   - 本ソースコードは、無保証・無サポートです。本ソースコードや生成物を用いたいかなる損害
 *     についても東京コスモス電機は保証致しません。不具合等の報告は歓迎いたします。
 *   - 本ソースコードは、東京コスモス電機が販売する TWE シリーズ上で実行する前提で公開
 *     しています。他のマイコン等への移植・流用は一部であっても出来ません。
 *
 ****************************************************************************/

#include <jendefs.h>

#include "utils.h"

#include "ccitt8.h"

#include "Interactive.h"
#include "EndDevice_Input.h"

#include "SMBus.h"

#include "sensor_driver.h"
#include "ADXL345.h"

static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
static void vStoreSensorValue();
static void vProcessADXL345(teEvent eEvent);

static void vSendToAppTweLite();
static bool_t bSendToSampMonitor(void);

static uint8 u8PlayDice( int16* accel );
static uint8 u8ShakePower( int16* Data );
static void vRead_Register( void );

static uint8 u8sns_cmplt = 0;

uint8 u8Power = 0;

uint16 au16DutyCurve[10] = { 0, 30, 50, 100, 150, 310, 500, 900, 1300, 1800 };
uint16 au16DutyLinear[10] = { 0, 200, 400, 600, 800, 1000, 1200, 1400, 1600, 1800 };

uint16 au16ThTable[4] = { TH_ACCEL, 500, 800, 1200 };

static bool_t bFirst = TRUE;
static bool_t bLightOff = FALSE;
static bool_t bLightOff_Hold = FALSE;
static int16 i16AveAccel = 0;
static bool_t bFaceUp = TRUE;
static bool_t bNoChangePkt = FALSE;

static uint8 au8TmpData[MAX_TX_APP_PAYLOAD];
static uint8 u8TmpLength = 0;

static tsSnsObj sSnsObj;
static tsObjData_ADXL345 sObjADXL345;

#define ACTIVE_NUM 2
#define INACTIVE_NUM 4

enum {
	E_SNS_ADC_CMP_MASK = 1,
	E_SNS_ADXL345_CMP = 2,
	E_SNS_ALL_CMP = 3
};

//	割り込み判定マクロ
#define bIS_TAP()  sObjADXL345.u8Interrupt&0x40
#define bIS_DTAP()  sObjADXL345.u8Interrupt&0x20
#define bIS_FREEFALL()  sObjADXL345.u8Interrupt&0x04
#define bIS_ACTIVE()  sObjADXL345.u8Interrupt&0x10
#define bIS_INACTIVE()  sObjADXL345.u8Interrupt&0x08
#define bIS_WATERMARK()  (sObjADXL345.u8Interrupt&0x02)

//	サイコロの各々の面だったときに送信するデータマクロ
#define DICE1() \
	S_OCTET(0x01);\
	S_OCTET(0x0F);\
	S_OCTET( 0x00 );\
	S_OCTET( 0x00 );\
	S_OCTET( 0x00 );\
	S_OCTET( 0x00 );\
	S_OCTET( 0x00 );

#define DICE2() \
	S_OCTET(0x08);\
	S_OCTET(0x0F);\
	S_OCTET( 0x7D );\
	S_OCTET( 0x00 );\
	S_OCTET( 0x00 );\
	S_OCTET( 0x00 );\
	S_OCTET( 0x00 );

#define DICE3() \
	S_OCTET(0x09);\
	S_OCTET(0x0F);\
	S_OCTET( 0x7D );\
	S_OCTET( 0x00 );\
	S_OCTET( 0x00 );\
	S_OCTET( 0x00 );\
	S_OCTET( 0x00 );

#define DICE4() \
	S_OCTET(0x0A);\
	S_OCTET(0x0F);\
	S_OCTET( 0x7D );\
	S_OCTET( 0x00 );\
	S_OCTET( 0x7D );\
	S_OCTET( 0x00 );\
	S_OCTET( 0x00 );

#define DICE5() \
	S_OCTET(0x0B);\
	S_OCTET(0x0F);\
	S_OCTET( 0x7D );\
	S_OCTET( 0x00 );\
	S_OCTET( 0x7D );\
	S_OCTET( 0x00 );\
	S_OCTET( 0x00 );

#define DICE6() \
	S_OCTET(0x0E);\
	S_OCTET(0x0F);\
	S_OCTET( 0x7D );\
	S_OCTET( 0x7D );\
	S_OCTET( 0x7D );\
	S_OCTET( 0x00 );\
	S_OCTET( 0x00 );

#define POWER0() \
	S_OCTET(0x00);\
	S_OCTET(0x0F);

#define POWER1() \
	S_OCTET(0x01);\
	S_OCTET(0x0F);

#define POWER2() \
	S_OCTET(0x02);\
	S_OCTET(0x0F);

#define POWER3() \
	S_OCTET(0x04);\
	S_OCTET(0x0F);


#define POWER4() \
	S_OCTET(0x08);\
	S_OCTET(0x0F);

/*
 * ADC 計測をしてデータ送信するアプリケーション制御
 */
PRSEV_HANDLER_DEF(E_STATE_IDLE, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_START_UP) {
		if (u32evarg & EVARG_START_UP_WAKEUP_RAMHOLD_MASK) {
			// Warm start message
			V_PRINTF(LB LB "*** Warm starting woke by %s. ***", sAppData.bWakeupByButton ? "DIO" : "WakeTimer");
		} else {
			// 開始する
			// start up message
			vSerInitMessage();

			V_PRINTF(LB "*** Cold starting");
			V_PRINTF(LB "* start end device[%d]", u32TickCount_ms & 0xFFFF);
			// ADXL345 の初期化
		}

		// RC クロックのキャリブレーションを行う
		ToCoNet_u16RcCalib(sAppData.sFlash.sData.u16RcClock);

		// センサーがらみの変数の初期化
		u8sns_cmplt = 0;

		vADXL345_Init( &sObjADXL345, &sSnsObj );
		if( bFirst ){
			V_PRINTF(LB "*** ADXL345 Setting...");
			bADXL345_Setting( sAppData.sFlash.sData.i16param, sAppData.sFlash.sData.sADXL345Param, IS_APPCONF_OPT_ADXL345_DISABLE_LINK() );
			if( sAppData.sFlash.sData.sADXL345Param.u16ThresholdTap != 0 ){
				au16ThTable[0] = sAppData.sFlash.sData.sADXL345Param.u16ThresholdTap;
			}
			if( sAppData.sFlash.sData.sADXL345Param.u16ThresholdFreeFall != 0 ){
				au16ThTable[1] = sAppData.sFlash.sData.sADXL345Param.u16ThresholdFreeFall;
			}
			if( sAppData.sFlash.sData.sADXL345Param.u16ThresholdActive != 0 ){
				au16ThTable[2] = sAppData.sFlash.sData.sADXL345Param.u16ThresholdActive;
			}
			if( sAppData.sFlash.sData.sADXL345Param.u16ThresholdInactive != 0 ){
				au16ThTable[3] = sAppData.sFlash.sData.sADXL345Param.u16ThresholdInactive;
			}
			vRead_Register();
		}

		vSnsObj_Process(&sSnsObj, E_ORDER_KICK);
		if (bSnsObj_isComplete(&sSnsObj)) {
			// 即座に完了した時はセンサーが接続されていない、通信エラー等
			u8sns_cmplt |= E_SNS_ADXL345_CMP;
			V_PRINTF(LB "*** ADXL345 comm err?");
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
			return;
		}

		// ADC の取得
		vADC_WaitInit();
		vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);

		// RUNNING 状態
		ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
	} else {
		V_PRINTF(LB "*** unexpected state.");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

PRSEV_HANDLER_DEF(E_STATE_RUNNING, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
		// 短期間スリープからの起床をしたので、センサーの値をとる
	if ((eEvent == E_EVENT_START_UP) && (u32evarg & EVARG_START_UP_WAKEUP_RAMHOLD_MASK) ) {
		V_PRINTF("#");
		vProcessADXL345(E_EVENT_START_UP);
	}

	static uint8 aCounter=0;
	static uint8 iCounter=0;
	static bool_t bHold = FALSE;

	// 送信処理に移行
	if (u8sns_cmplt == E_SNS_ALL_CMP) {
		if( sAppData.sFlash.sData.i16param&SHAKE ){
			if(aCounter == ACTIVE_NUM){
				aCounter = 0;
				if(  sAppData.sFlash.sData.i16param != SHAKE_FAN && sAppData.sFlash.sData.i16param != SHAKE_HOLD ){
					V_PRINTF(LB"Clear Tmp");
					i16AveAccel = 0;
				}
			}
			aCounter++;

			if( iCounter == INACTIVE_NUM){
				iCounter = 0;
			}

			if(sObjADXL345.ai16Result[0] > au16ThTable[0] ){
				if( sAppData.sFlash.sData.i16param == SHAKE_HOLD && aCounter == 1 ){
					V_PRINTF(LB"Clear Tmp1");
					i16AveAccel = 0;
				}
				if( sObjADXL345.ai16Result[0] > i16AveAccel ){
					i16AveAccel = sObjADXL345.ai16Result[0];
				}
				bHold = FALSE;
				iCounter = 0;
				bLightOff = FALSE;
				V_PRINTF(LB"Ave = %d", i16AveAccel);
			}else if( ( aCounter == 1 && ( sAppData.sFlash.sData.i16param == SHAKE_HOLD || sAppData.sFlash.sData.i16param == SHAKE_FAN ) )||
					i16AveAccel <= au16ThTable[0] ){
				if( sObjADXL345.ai16Result[1] < 0 ){
					if( bFaceUp == TRUE || i16AveAccel > au16ThTable[0] ){
						bLightOff_Hold = TRUE;
						i16AveAccel = 0;
						V_PRINTF(LB"Turn!!");

					}else{
						bLightOff_Hold = FALSE;
					}
					bFaceUp = FALSE;
				}else{
					bFaceUp = TRUE;
					bLightOff_Hold = FALSE;
				}

				if( sAppData.sFlash.sData.i16param == SHAKE_HOLD || sAppData.sFlash.sData.i16param == SHAKE_FAN ){
					bHold = TRUE;
				}

				iCounter++;
				if(iCounter > 1 && sAppData.sFlash.sData.i16param == SHAKE_FAN ){
					i16AveAccel = 0;
				}

				bLightOff = TRUE;
			}

			V_PRINTF(LB"Wake=%s Off=%s Accel=%d Param=%d",
					sAppData.bWakeupByButton ? "TRUE": "FALSE",
					bLightOff ? "TRUE": "FALSE",
					i16AveAccel,
					sAppData.sFlash.sData.i16param);
			V_PRINTF(LB"aCount = %d", aCounter );
			V_PRINTF(LB"iCount = %d", iCounter );

			if( (( sAppData.sFlash.sData.i16param == SHAKE_FAN || sAppData.sFlash.sData.i16param == SHAKE_HOLD ) && bLightOff_Hold) ||
				(aCounter == ACTIVE_NUM && i16AveAccel > au16ThTable[0] && bHold == FALSE ) ||
				iCounter == INACTIVE_NUM ||
				( i16AveAccel <= au16ThTable[0]  && bNoChangePkt == FALSE && iCounter == 1 )  ){
				if( iCounter == INACTIVE_NUM &&
					!(( sAppData.sFlash.sData.i16param == SHAKE_FAN || sAppData.sFlash.sData.i16param == SHAKE_HOLD ) && bLightOff_Hold) ){
					bNoChangePkt = TRUE;
				}else{
					bNoChangePkt = FALSE;
				}
				ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_TX);
			}
			else{
				ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
			}
		}else{
			ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_TX);
		}
	}

	// タイムアウト
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 2000) {
		V_PRINTF(LB"! TIME OUT (E_STATE_RUNNING)");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

PRSEV_HANDLER_DEF(E_STATE_APP_WAIT_TX, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		// ネットワークの初期化
		if (!sAppData.pContextNwk) {
			// 初回のみ
			sAppData.sNwkLayerTreeConfig.u8Role = TOCONET_NWK_ROLE_ENDDEVICE;
			sAppData.pContextNwk = ToCoNet_NwkLyTr_psConfig_MiniNodes(&sAppData.sNwkLayerTreeConfig);
			if (sAppData.pContextNwk) {
				ToCoNet_Nwk_bInit(sAppData.pContextNwk);
				ToCoNet_Nwk_bStart(sAppData.pContextNwk);
			} else {
				ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
				return;
			}
		} else {
			// 一度初期化したら RESUME
			ToCoNet_Nwk_bResume(sAppData.pContextNwk);
		}

		if( bFirst && sAppData.sFlash.sData.i16param != NORMAL &&
			sAppData.sFlash.sData.i16param != NEKOTTER &&
			(sAppData.sFlash.sData.i16param&SHAKE) == 0 ){
			bFirst = FALSE;
			V_PRINTF(LB"*** First Sleep...");
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
		}else{
			bFirst = FALSE;

			if( IS_APPCONF_OPT_APP_TWELITE() ){		//	App_Twelites
				vSendToAppTweLite();
			}else{									//	Samp_Monitor
				if( !bSendToSampMonitor() ){
					ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // 送信失敗
				}
			}

#ifdef LITE2525A
			vPortSetHi(LED);
#else
			vPortSetLo(LED);
#endif
			V_PRINTF(" FR=%04X", sAppData.u16frame_count);
		}

	}
	if (eEvent == E_ORDER_KICK) { // 送信完了イベントが来たのでスリープする
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}

	// タイムアウト
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 100) {
		V_PRINTF(LB"! TIME OUT (E_STATE_APP_WAIT_TX)");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

PRSEV_HANDLER_DEF(E_STATE_APP_SLEEP, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		// Sleep は必ず E_EVENT_NEW_STATE 内など１回のみ呼び出される場所で呼び出す。
		V_PRINTF(LB"! Sleeping...");
		V_FLUSH();

		// Mininode の場合、特別な処理は無いのだが、ポーズ処理を行う
		ToCoNet_Nwk_bPause(sAppData.pContextNwk);

		// センサー用の電源制御回路を Hi に戻す
		vPortSetSns(FALSE);

#ifdef LITE2525A
		vPortSetLo(LED);
#else
		vPortSetHi(LED);
#endif
		// 周期スリープに入る
		if( sAppData.sFlash.sData.i16param == DICE ||
			sAppData.sFlash.sData.i16param == NORMAL ||
			sAppData.sFlash.sData.i16param == NEKOTTER ){

			uint32 u32SleepDur_ms = sAppData.sFlash.sData.u32Slp;
			if(IS_APPCONF_OPT_WAKE_RANDOM()){		//	起床ランダムのオプションが立っていた時
				uint32 u32max = u32SleepDur_ms>>3;		//	だいたい±10%
				uint32 u32Rand = ToCoNet_u32GetRand();
				uint32 u32Rand_max = u32Rand%(u32max+1);
				if( (u32Rand&0x8000) != 0 ){
					if( u32Rand_max+10 < u32SleepDur_ms && u32SleepDur_ms > 10 ){	//	スリープ時間が10ms以下にならないようにする
						u32SleepDur_ms -= u32Rand_max;
					}
				}else{
					u32SleepDur_ms += u32Rand_max;
				}
				V_PRINTF( LB"ORG=%d,MAX=%d,RND=%08X,RMX=%d,SLD=%d", sAppData.sFlash.sData.u32Slp, u32max, u32Rand, u32Rand_max, u32SleepDur_ms );
				V_FLUSH();
			}

			//	割り込みの設定
			vAHI_DioSetDirection(PORT_INPUT_MASK_ADXL345, 0); // set as input
			(void)u32AHI_DioInterruptStatus(); // clear interrupt register
			vAHI_DioWakeEnable(PORT_INPUT_MASK_ADXL345, 0); // also use as DIO WAKE SOURCE
			vAHI_DioWakeEdge(PORT_INPUT_MASK_ADXL345, 0); // 割り込みエッジ(立上がりに設定)

			ToCoNet_vSleep( E_AHI_WAKE_TIMER_0, u32SleepDur_ms, FALSE, FALSE);
		}else{
			if( IS_APPCONF_OPT_APP_TWELITE() &&
				( bLightOff && (sAppData.sFlash.sData.i16param&SHAKE) == 0 ) ){
				vAHI_DioWakeEnable(0, PORT_INPUT_MASK_ADXL345); // DISABLE DIO WAKE SOURCE
				u8Read_Interrupt();	//	加速度センサの割り込みレジスタをクリア
				ToCoNet_vSleep( E_AHI_WAKE_TIMER_0, sAppData.sFlash.sData.u32Slp, FALSE, FALSE);
			}
			else{
				//	割り込みの設定
				vAHI_DioSetDirection(PORT_INPUT_MASK_ADXL345, 0); // set as input
				(void)u32AHI_DioInterruptStatus(); // clear interrupt register
				vAHI_DioWakeEnable(PORT_INPUT_MASK_ADXL345, 0); // also use as DIO WAKE SOURCE
				vAHI_DioWakeEdge(PORT_INPUT_MASK_ADXL345, 0); // 割り込みエッジ(立上がりに設定)

				u8Read_Interrupt();	//	加速度センサの割り込みレジスタをクリア

				ToCoNet_vSleep( E_AHI_WAKE_TIMER_0, 0, FALSE, FALSE);
			}
		}
	}
}

/**
 * イベント処理関数リスト
 */
static const tsToCoNet_Event_StateHandler asStateFuncTbl[] = {
	PRSEV_HANDLER_TBL_DEF(E_STATE_IDLE),
	PRSEV_HANDLER_TBL_DEF(E_STATE_RUNNING),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_WAIT_TX),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_SLEEP),
	PRSEV_HANDLER_TBL_TRM
};

/**
 * イベント処理関数
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	ToCoNet_Event_StateExec(asStateFuncTbl, pEv, eEvent, u32evarg);
}

#if 0
/**
 * ハードウェア割り込み
 * @param u32DeviceId
 * @param u32ItemBitmap
 * @return
 */
static uint8 cbAppToCoNet_u8HwInt(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	uint8 u8handled = FALSE;

	switch (u32DeviceId) {
	case E_AHI_DEVICE_ANALOGUE:
		break;

	case E_AHI_DEVICE_SYSCTRL:
		break;

	case E_AHI_DEVICE_TIMER0:
		break;

	case E_AHI_DEVICE_TICK_TIMER:
		break;

	default:
		break;
	}

	return u8handled;
}
#endif

/**
 * ハードウェアイベント（遅延実行）
 * @param u32DeviceId
 * @param u32ItemBitmap
 */
static void cbAppToCoNet_vHwEvent(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	switch (u32DeviceId) {
	case E_AHI_DEVICE_TICK_TIMER:
		vProcessADXL345(E_EVENT_TICK_TIMER);
		break;

	case E_AHI_DEVICE_ANALOGUE:
		/*
		 * ADC完了割り込み
		 */
		V_PUTCHAR('@');
		vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);
		if (bSnsObj_isComplete(&sAppData.sADC)) {
			u8sns_cmplt |= E_SNS_ADC_CMP_MASK;
			vStoreSensorValue();
		}
		break;

	case E_AHI_DEVICE_SYSCTRL:
		break;

	case E_AHI_DEVICE_TIMER0:
		break;

	default:
		break;
	}
}

#if 0
/**
 * メイン処理
 */
static void cbAppToCoNet_vMain() {
	/* handle serial input */
	vHandleSerialInput();
}
#endif

#if 0
/**
 * ネットワークイベント
 * @param eEvent
 * @param u32arg
 */
static void cbAppToCoNet_vNwkEvent(teEvent eEvent, uint32 u32arg) {
	switch(eEvent) {
	case E_EVENT_TOCONET_NWK_START:
		break;

	default:
		break;
	}
}
#endif


#if 0
/**
 * RXイベント
 * @param pRx
 */
static void cbAppToCoNet_vRxEvent(tsRxDataApp *pRx) {

}
#endif

/**
 * TXイベント
 * @param u8CbId
 * @param bStatus
 */
static void cbAppToCoNet_vTxEvent(uint8 u8CbId, uint8 bStatus) {
	// 送信完了
	V_PRINTF(LB"! Tx Cmp = %d", bStatus);
	ToCoNet_Event_Process(E_ORDER_KICK, 0, vProcessEvCore);
}
/**
 * アプリケーションハンドラー定義
 *
 */
static tsCbHandler sCbHandler = {
	NULL, // cbAppToCoNet_u8HwInt,
	cbAppToCoNet_vHwEvent,
	NULL, // cbAppToCoNet_vMain,
	NULL, // cbAppToCoNet_vNwkEvent,
	NULL, // cbAppToCoNet_vRxEvent,
	cbAppToCoNet_vTxEvent
};

/**
 * アプリケーション初期化
 */
void vInitAppADXL345() {
	psCbHandler = &sCbHandler;
	pvProcessEv1 = vProcessEvCore;
}

static void vProcessADXL345(teEvent eEvent) {
	if (bSnsObj_isComplete(&sSnsObj)) {
		 return;
	}

	// イベントの処理
	vSnsObj_Process(&sSnsObj, eEvent); // ポーリングの時間待ち
	if (bSnsObj_isComplete(&sSnsObj)) {
		u8sns_cmplt |= E_SNS_ADXL345_CMP;

		V_PRINTF(LB"!ADXL345: X : %d, Y : %d, Z : %d",
			sObjADXL345.ai16Result[ADXL345_IDX_X],
			sObjADXL345.ai16Result[ADXL345_IDX_Y],
			sObjADXL345.ai16Result[ADXL345_IDX_Z]
		);

		// 完了時の処理
		if (u8sns_cmplt == E_SNS_ALL_CMP) {
			ToCoNet_Event_Process(E_ORDER_KICK, 0, vProcessEvCore);
		}
	}
}

/**
 * センサー値を格納する
 */
static void vStoreSensorValue() {
	// センサー値の保管
	sAppData.sSns.u16Adc1 = sAppData.sObjADC.ai16Result[TEH_ADC_IDX_ADC_1];
#ifdef USE_TEMP_INSTDOF_ADC2
	sAppData.sSns.u16Adc2 = sAppData.sObjADC.ai16Result[TEH_ADC_IDX_TEMP];
#else
	sAppData.sSns.u16Adc2 = sAppData.sObjADC.ai16Result[TEH_ADC_IDX_ADC_2];
#endif
	sAppData.sSns.u8Batt = ENCODE_VOLT(sAppData.sObjADC.ai16Result[TEH_ADC_IDX_VOLT]);

	// ADC1 が 1300mV 以上(SuperCAP が 2600mV 以上)である場合は SUPER CAP の直結を有効にする
	if (sAppData.sSns.u16Adc1 >= VOLT_SUPERCAP_CONTROL) {
		vPortSetLo(DIO_SUPERCAP_CONTROL);
	}
}

static void vSendToAppTweLite(){
	static uint8 u8Analog = 0;
	static bool_t bBack = FALSE;
	// 初期化後速やかに送信要求
	V_PRINTF(LB"[SNS_COMP/TX]");

	tsTxDataApp sTx;
	memset(&sTx, 0, sizeof(sTx)); // 必ず０クリアしてから使う！

	sTx.u32SrcAddr = ToCoNet_u32GetSerial();

	uint8* q = sTx.auData;
	uint8 crc = u8CCITT8((uint8*) &sToCoNet_AppContext.u32AppId, 4);

	if( bNoChangePkt ){
		memcpy( q, au8TmpData, u8TmpLength );
		q += u8TmpLength;
		V_PRINTF(LB"Send Same Pakket.");
	}else{
		sAppData.u16frame_count++; // シリアル番号を更新する
		S_OCTET(crc);								//
		S_OCTET(0x01);								// プロトコルバージョン
		S_OCTET(0x78);								// アプリケーション論理アドレス
		S_BE_DWORD(ToCoNet_u32GetSerial());			// シリアル番号
		S_OCTET(0x00);								// 宛先
		S_BE_WORD(u32TickCount_ms & 0xFFFF);		// タイムスタンプ
		S_OCTET(0);									// 中継フラグ
		S_BE_WORD(sAppData.sObjADC.ai16Result[TEH_ADC_IDX_VOLT]);	//	電源電圧
		S_OCTET(0);									// 温度(ダミー)

		if( sAppData.sFlash.sData.i16param == DICE ){
			uint8 u8Dice = u8PlayDice( sObjADXL345.ai16Result );
			switch(u8Dice){
			case 1:
				DICE1();
				break;
			case 2:
				DICE2();
				break;
			case 3:
				DICE3();
				break;
			case 4:
				DICE4();
				break;
			case 5:
				DICE5();
				break;
			case 6:
				DICE6();
				break;
			}
		}else if( sAppData.sFlash.sData.i16param&SHAKE ){
			uint8 u8PowTemp = u8ShakePower( sObjADXL345.ai16Result );
			if( sAppData.sFlash.sData.i16param == SHAKE_ACC1 || sAppData.sFlash.sData.i16param == SHAKE_ACC2 || sAppData.sFlash.sData.i16param == SHAKE_ACC3 ){
				if( i16AveAccel > au16ThTable[0] ){
					if( sAppData.sFlash.sData.i16param == SHAKE_ACC1 ){
						if(!bBack){
							u8Power = u8PowTemp>0 ? u8Power+1 : u8Power ;
							if( u8Power >= 4 ){
								bBack = TRUE;
							}
						}else{
							u8Power = u8PowTemp>0 ? u8Power-1 : u8Power ;
							if( u8Power <= 0 ){
								bBack = FALSE;
							}
						}
					}else
					if( sAppData.sFlash.sData.i16param == SHAKE_ACC2 ){
						u8Power = u8PowTemp>0 ? u8Power+1 : u8Power ;
						if( u8Power > 4 ){
							u8Power = 0;
						}
					}else{
						if(bFaceUp){
							if( u8Power < 4 ){
								u8Power = u8PowTemp>0 ? u8Power+1 : u8Power ;
							}
						}else{
							if( u8Power > 0 ){
								u8Power = u8PowTemp>0 ? u8Power-1 : u8Power ;
							}
						}
					}
				}
			}else{
				u8Power = u8PowTemp;
			}

			V_PRINTF( LB"Power = %d", u8Power );

			switch(u8Power){
			case 0:
				POWER0();
				break;
			case 1:
				POWER1();
				break;
			case 2:
				POWER2();
				break;
			case 3:
				POWER3();
				break;
			case 4:
				POWER4();
				break;
			default:
				POWER0();
				break;
			}
			if( i16AveAccel > au16ThTable[0] ){
				if( bFaceUp ){
					if(u8Analog < 9  ){
						u8Analog++;
					}
				}else{
					if( u8Analog > 0 ){
						u8Analog--;
					}
				}
			}

			uint8 u8LowBit;
			uint8 u8HighBit;
			if( IS_APPCONF_OPT_ADXL345_SHAKE_LINEAR() ){
				u8LowBit = (au16DutyLinear[u8Analog]>>2)&0x03;
				u8HighBit = au16DutyLinear[u8Analog]>>4;
			}else{
				u8LowBit = (au16DutyCurve[u8Analog]>>2)&0x03;
				u8HighBit = au16DutyCurve[u8Analog]>>4;
			}

			S_OCTET(u8HighBit);
			S_OCTET(0x00);
			S_OCTET(0x00);
			S_OCTET(0x00);
			S_OCTET(u8LowBit);
		}else{
			uint8 IOMap = 0;
			if( bLightOff ){
				bLightOff = FALSE;
			}else{
				if( bIS_FREEFALL() ){
					IOMap += 1<<2;
					bLightOff = TRUE;
				}
				else
					if( bIS_DTAP() ){
						IOMap += 1<<1;
						bLightOff = TRUE;
					}
					else
						if( bIS_TAP() ){
							IOMap += 1;
							bLightOff = TRUE;
						}
			}
			IOMap += bIS_INACTIVE() ? 0 : bIS_ACTIVE() ? 1<<3 : 0;

			// DIO の設定
			S_OCTET(IOMap);
			S_OCTET(0x0F);

			uint8	i;
			uint16	u16v;
			uint8 u8LSBs = 0;
			for(i=0; i<3; i++){
				u16v = ((sObjADXL345.ai16Result[i]+1600)*5)>>3;	//	+-16g -> 0 - 2400 に変換
				u16v >>= 2;

				uint8 u8MSB = (u16v >> 2) & 0xFF;
				S_OCTET(u8MSB);

				// 下2bitを u8LSBs に詰める
				u8LSBs >>= 2;
				u8LSBs |= ((u16v << 6) & 0xC0);
			}

			S_OCTET(sAppData.sObjADC.ai16Result[TEH_ADC_IDX_VOLT] >> 5);
			u8LSBs >>= 2;
			u8LSBs |= ((sAppData.sObjADC.ai16Result[TEH_ADC_IDX_VOLT] << 6) & 0xC0);
			S_OCTET(u8LSBs);
		}
		u8TmpLength = q - sTx.auData;
		memcpy( au8TmpData, sTx.auData, u8TmpLength );
	}
	sTx.u8Cmd = 0x02+0; // パケット種別
	// 送信する
	sTx.u32DstAddr = TOCONET_MAC_ADDR_BROADCAST; // ブロードキャスト
	sTx.u32SrcAddr = sToCoNet_AppContext.u16ShortAddress;
	sTx.bAckReq = FALSE;
	sTx.u8Retry = sAppData.u8Retry;

	sTx.u8Len = q - sTx.auData; // パケットのサイズ
	sTx.u8CbId = sAppData.u16frame_count & 0xFF; // TxEvent で通知される番号、送信先には通知されない
	sTx.u8Seq = sAppData.u16frame_count & 0xFF; // シーケンス番号(送信先に通知される)

	ToCoNet_bMacTxReq(&sTx);
	ToCoNet_Tx_vProcessQueue(); // 送信処理をタイマーを待たずに実行する
}

static bool_t bSendToSampMonitor( void ){
	uint8	au8Data[12];
	uint8*	q = au8Data;
	static bool_t bBack = FALSE;

	if( bNoChangePkt ){
		sAppData.u16frame_count--; // 更新しない
		memcpy( q, au8TmpData, u8TmpLength );
		q += u8TmpLength;
		V_PRINTF(LB"Send Same Pakket.");
	}else{
		S_OCTET(sAppData.sSns.u8Batt);
		S_BE_WORD(sAppData.sSns.u16Adc1);
		S_BE_WORD(sAppData.sSns.u16Adc2);
		if( sAppData.sFlash.sData.i16param == DICE ){
			S_BE_WORD(u8PlayDice( sObjADXL345.ai16Result));
			S_BE_WORD(0x0000);
			S_BE_WORD(0x0000);
		}else if( sAppData.sFlash.sData.i16param&SHAKE ){
			uint8 u8PowTemp = u8ShakePower( sObjADXL345.ai16Result );
			if( sAppData.sFlash.sData.i16param == SHAKE_ACC1 || sAppData.sFlash.sData.i16param == SHAKE_ACC2 || sAppData.sFlash.sData.i16param == SHAKE_ACC3 ){
				if( i16AveAccel > au16ThTable[0] ){
					if( sAppData.sFlash.sData.i16param == SHAKE_ACC1 ){
						if(!bBack){
							u8Power = u8PowTemp>0 ? u8Power+1 : u8Power ;
							if( u8Power >= 4 ){
								bBack = TRUE;
							}
						}else{
							u8Power = u8PowTemp>0 ? u8Power-1 : u8Power ;
							if( u8Power <= 0 ){
								bBack = FALSE;
							}
						}
					}else
					if( sAppData.sFlash.sData.i16param == SHAKE_ACC2 ){
						u8Power = u8PowTemp>0 ? u8Power+1 : u8Power ;
						if( u8Power > 4 ){
							u8Power = 0;
						}
					}else{
						if(bFaceUp){
							if( u8Power < 4 ){
								u8Power = u8PowTemp>0 ? u8Power+1 : u8Power ;
							}
						}else{
							if( u8Power > 0 ){
								u8Power = u8PowTemp>0 ? u8Power-1 : u8Power ;
							}
						}
					}
				}
			}else{
				u8Power = u8PowTemp;
			}

			V_PRINTF( LB"Power = %d", u8Power );
			S_BE_WORD( u8Power );
			S_BE_WORD(0x0000);
			S_BE_WORD(0x0000);
		}else{
			S_BE_WORD(sObjADXL345.ai16Result[ADXL345_IDX_X]);
			S_BE_WORD(sObjADXL345.ai16Result[ADXL345_IDX_Y]);
			S_BE_WORD(sObjADXL345.ai16Result[ADXL345_IDX_Z]);
		}

		if( sAppData.sFlash.sData.i16param == DICE ){
			S_OCTET( 0xFD );
		}
		else
		if( sAppData.sFlash.sData.i16param == NEKOTTER ){
			S_OCTET( 0xFF );
		}
		else
		if( (sAppData.sFlash.sData.i16param&SHAKE) != 0 ){
			S_OCTET( 0xFC );
		}
		else
		if( bIS_FREEFALL() ){
			S_OCTET( FREEFALL );
		}
		else
		if( bIS_DTAP() ){
			S_OCTET( D_TAP );
		}
		else
		if( bIS_TAP() ){
			S_OCTET( S_TAP );
		}
		else
		if( bIS_INACTIVE() ){
			S_OCTET( INACTIVE );
		}
		else
		if( bIS_ACTIVE() ){
			S_OCTET( ACTIVE );
		}
		else{
		S_OCTET( 0x00 );
		}
		u8TmpLength = q-au8Data;
		memcpy( au8TmpData, au8Data, u8TmpLength );
	}

	return bSendMessage( au8Data, q-au8Data );

}

/**
 *
 */
static uint8 u8PlayDice( int16* accel )
{
	int16 max = -30000;
	int16 min = 30000;
	uint8 max_a = 0, min_a = 0;
	uint8 i;

	uint8 u8Dice = 0;

	for( i=0; i<3; i++ ){
		if( max < accel[i] ){
			max = accel[i];
			max_a = i;
		}
		if( min > accel[i] ){
			min = accel[i];
			min_a = i;
		}
	}
	if( min < 0 ){
		min = -min;
		V_PRINTF(LB"Abs Min");		//	これなくすとWarning
	}
	if( max < 0 ){
		max = -max;
		V_PRINTF(LB"Abs Max");		//	これなくすとWarning
	}

	if( max > min ){
		switch(max_a){
		case 0:
			u8Dice = 5;
			break;
		case 1:
			u8Dice = 4;
			break;
		case 2:
			u8Dice = 1;
			break;
		}
	}else{
		switch(min_a){
		case 0:
			u8Dice = 2;
			break;
		case 1:
			u8Dice = 3;
			break;
		case 2:
			u8Dice = 6;
			break;
		}
	}
	return u8Dice;
}


/**
 *		振る強さを9段階(0-8)で返す
 *		-16000mg～16000mgの値域の標準偏差の最大値がだいたい2000強(実験的)
 */
static uint8 u8ShakePower( int16* accel )
{
	uint8 i = 0;
	int16 Power;

	Power = i16AveAccel;

	if( Power >=  au16ThTable[0] ){
		i = 1;
	}
	if( Power >= au16ThTable[1] &&
		au16ThTable[1] > au16ThTable[0]){
		i = 2;
	}
	if( Power >= au16ThTable[2] &&
		au16ThTable[2] > au16ThTable[1] &&
		au16ThTable[2] > au16ThTable[0]){
		i = 3;
	}
	if( Power >= au16ThTable[3] &&
		au16ThTable[3] > au16ThTable[2] &&
		au16ThTable[3] > au16ThTable[1] &&
		au16ThTable[3] > au16ThTable[0]){
		i = 4;
	}

	return i;
}

void vRead_Register( void )
{
	uint8	u8source;
	bool_t bOk = TRUE;

	bOk &= bSMBusWrite( ADXL345_ADDRESS, ADXL345_THRESH_TAP, 0, NULL );
	bOk &= bSMBusSequentialRead( ADXL345_ADDRESS, 1, &u8source );
	V_PRINTF( LB"THT = %02X, %d", u8source, u8source*625/10 );

	bOk &= bSMBusWrite( ADXL345_ADDRESS, ADXL345_DUR, 0, NULL );
	bOk &= bSMBusSequentialRead( ADXL345_ADDRESS, 1, &u8source );
	V_PRINTF( LB"DUR = %02X, %d", u8source, u8source*625/10 );

	bOk &= bSMBusWrite( ADXL345_ADDRESS, ADXL345_LATENT, 0, NULL );
	bOk &= bSMBusSequentialRead( ADXL345_ADDRESS, 1, &u8source );
	V_PRINTF( LB"LAT = %02X, %d", u8source, u8source*125/100 );

	bOk &= bSMBusWrite( ADXL345_ADDRESS, ADXL345_WINDOW, 0, NULL );
	bOk &= bSMBusSequentialRead( ADXL345_ADDRESS, 1, &u8source );
	V_PRINTF( LB"WIN = %02X, %d", u8source, u8source*125/100 );

	bOk &= bSMBusWrite( ADXL345_ADDRESS, ADXL345_THRESH_FF, 0, NULL );
	bOk &= bSMBusSequentialRead( ADXL345_ADDRESS, 1, &u8source );
	V_PRINTF( LB"THF = %02X, %d", u8source, u8source*625/10 );

	bOk &= bSMBusWrite( ADXL345_ADDRESS, ADXL345_TIME_FF, 0, NULL );
	bOk &= bSMBusSequentialRead( ADXL345_ADDRESS, 1, &u8source );
	V_PRINTF( LB"TIF = %02X, %d", u8source, u8source*5 );

	bOk &= bSMBusWrite( ADXL345_ADDRESS, ADXL345_THRESH_ACT, 0, NULL );
	bOk &= bSMBusSequentialRead( ADXL345_ADDRESS, 1, &u8source );
	V_PRINTF( LB"THA = %02X, %d", u8source, u8source*625/10 );

	bOk &= bSMBusWrite( ADXL345_ADDRESS, ADXL345_THRESH_INACT, 0, NULL );
	bOk &= bSMBusSequentialRead( ADXL345_ADDRESS, 1, &u8source );
	V_PRINTF( LB"THI = %02X, %d", u8source, u8source*625/10 );

	bOk &= bSMBusWrite( ADXL345_ADDRESS, ADXL345_TIME_INACT, 0, NULL );
	bOk &= bSMBusSequentialRead( ADXL345_ADDRESS, 1, &u8source );
	V_PRINTF( LB"TII = %02X, %d", u8source, u8source );
}
