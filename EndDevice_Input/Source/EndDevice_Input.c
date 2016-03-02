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

/**
 * 概要
 *
 * 本パートは、IO状態を取得し、ブロードキャストにて、これをネットワーク層に
 * 送付する。
 *
 */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <string.h>

#include <jendefs.h>
#include <AppHardwareApi.h>

#include "AppQueueApi_ToCoNet.h"

#include "config.h"
#include "ccitt8.h"
#include "Interrupt.h"

#include "EndDevice_Input.h"
#include "Version.h"

#include "utils.h"

#include "config.h"
#include "common.h"

#include "adc.h"
#include "SMBus.h"

// Serial options
#include "serial.h"
#include "fprintf.h"
#include "sprintf.h"

#include "input_string.h"
#include "Interactive.h"
#include "flash.h"

#include "ADXL345.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/**
 * DI1 の割り込み（立ち上がり）で起床後
 *   - PWM1 に DUTY50% で 100ms のブザー制御
 *
 * 以降１秒置きに起床して、DI1 が Hi (スイッチ開) かどうかチェックし、
 * Lo になったら、割り込みスリープに遷移、Hi が維持されていた場合は、
 * 一定期間 .u16Slp 経過後にブザー制御を 100ms 実施する。
 */
tsTimerContext sTimerPWM[1]; //!< タイマー管理構造体  @ingroup MASTER

/**
 * アプリケーションごとの振る舞いを記述するための関数テーブル
 */
tsCbHandler *psCbHandler = NULL;

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
static void vInitHardware(int f_warm_start);
#if !(defined(TWX0003) || defined(CNFMST))
static void vInitPulseCounter();
#endif
static void vInitADC();

static void vSerialInit();
void vSerInitMessage();
void vProcessSerialCmd(tsSerCmd_Context *pCmd);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
// Local data used by the tag during operation
tsAppData_Ed sAppData;

tsFILE sSerStream;
tsSerialPortSetup sSerPort;

uint8 u8Interrupt;
uint8 u8PowerUp; // 0x01:from Deep

void *pvProcessEv1, *pvProcessEv2;
void (*pf_cbProcessSerialCmd)(tsSerCmd_Context *);

/****************************************************************************/
/***        Functions                                                     ***/
/****************************************************************************/

/**
 * 始動時の処理
 */
void cbAppColdStart(bool_t bAfterAhiInit) {
	if (!bAfterAhiInit) {
		// before AHI initialization (very first of code)
		pvProcessEv1 = NULL;
		pvProcessEv2 = NULL;
		pf_cbProcessSerialCmd = NULL;

		// check Deep boot
		u8PowerUp = 0;
#ifdef JN514x
		if (u8AHI_PowerStatus() & 0x01) {
#elif JN516x
		if (u16AHI_PowerStatus() & 0x01) {
#endif
			u8PowerUp = 0x01;
		}

		// check DIO source
		sAppData.bWakeupByButton = FALSE;
		if(u8AHI_WakeTimerFiredStatus()) {
		} else
    	if(u32AHI_DioWakeStatus() & u32DioPortWakeUp) {
			// woke up from DIO events
    		sAppData.bWakeupByButton = TRUE;
		}

		// Module Registration
		ToCoNet_REG_MOD_ALL();
	} else {
		// disable brown out detect
		vAHI_BrownOutConfigure(0,//0:2.0V 1:2.3V
				FALSE,
				FALSE,
				FALSE,
				FALSE);

		// リセットICの無効化(イの一番に処理)
#ifdef LITE2525A
		vPortAsInput(DIO_VOLTAGE_CHECKER);
#else
		vPortSetLo(DIO_VOLTAGE_CHECKER);
		vPortAsOutput(DIO_VOLTAGE_CHECKER);
		vPortDisablePullup(DIO_VOLTAGE_CHECKER);
#endif

		// １次キャパシタ(e.g. 220uF)とスーパーキャパシタ (1F) の直結制御用(イの一番に処理)
#if defined(TWX0003)
		// このポートは入力扱いとして何も設定しない
		vPortAsInput(DIO_SUPERCAP_CONTROL);
#elif defined(LITE2525A)
		vPortAsInput(DIO_SUPERCAP_CONTROL);
#elif defined(CNFMST) // ToCoStick を想定する
		vPortAsOutput(PORT_OUT1);
		vAHI_DoSetDataOut(2, 0);
		bAHI_DoEnableOutputs(TRUE); // MISO を出力用に
#else
		vPortSetHi(DIO_SUPERCAP_CONTROL);
		vPortAsOutput(DIO_SUPERCAP_CONTROL);
		vPortDisablePullup(DIO_SUPERCAP_CONTROL);
#endif

		//	送信ステータスなどのLEDのための出力
#ifdef LITE2525A
		vPortSetLo(LED);
#else
		vPortSetHi(LED);
#endif
		vPortAsOutput(LED);

		// アプリケーション保持構造体の初期化
		memset(&sAppData, 0x00, sizeof(sAppData));

		// SPRINTFの初期化(128バイトのバッファを確保する)
		SPRINTF_vInit128();

		// フラッシュメモリからの読み出し
		//   フラッシュからの読み込みが失敗した場合、ID=15 で設定する
		sAppData.bFlashLoaded = Config_bLoad(&sAppData.sFlash);

		//	入力ボタンのプルアップを停止する
		if ((sAppData.sFlash.sData.u8mode == PKT_ID_IO_TIMER)	// ドアタイマー
			|| (sAppData.sFlash.sData.u8mode == PKT_ID_BUTTON && sAppData.sFlash.sData.i16param == 1 )){	// 押しボタンの立ち上がり検出時
			vPortDisablePullup(DIO_BUTTON); // 外部プルアップのため
		}

		// センサー用の制御 (Lo:Active), OPTION による制御を行っているのでフラッシュ読み込み後の制御が必要
#ifndef TWX0003
		vPortSetSns(TRUE);
		vPortAsOutput(DIO_SNS_POWER);
		vPortDisablePullup(DIO_SNS_POWER);
#endif

		// configure network
#ifdef LITE2525A
		if( IS_APPCONF_OPT_APP_TWELITE() && sAppData.sFlash.sData.u32appid == APP_ID && sAppData.sFlash.sData.u8ch == CHANNEL ){
			sToCoNet_AppContext.u32AppId = APP_TWELITE_ID;
			sToCoNet_AppContext.u8Channel = APP_TWELITE_CHANNEL;
		}else{
			if( IS_APPCONF_OPT_APP_TWELITE() && sAppData.sFlash.sData.u32appid == APP_ID ){
				sToCoNet_AppContext.u32AppId = APP_TWELITE_ID;
			}else{
				sToCoNet_AppContext.u32AppId = sAppData.sFlash.sData.u32appid;
			}
			sToCoNet_AppContext.u8Channel = sAppData.sFlash.sData.u8ch;
		}
#else
		sToCoNet_AppContext.u32AppId = sAppData.sFlash.sData.u32appid;
		sToCoNet_AppContext.u8Channel = sAppData.sFlash.sData.u8ch;
#endif

		sToCoNet_AppContext.u8RandMode = 0;

		sToCoNet_AppContext.bRxOnIdle = FALSE;

		sToCoNet_AppContext.u8CCA_Level = 1;
		sToCoNet_AppContext.u8CCA_Retry = 0;

		sAppData.u8Retry = ((sAppData.sFlash.sData.u8pow>>4)&0x0F) + 0x80;		// 強制再送
		sToCoNet_AppContext.u8TxPower = sAppData.sFlash.sData.u8pow&0x0F;

		// version info
		sAppData.u32ToCoNetVersion = ToCoNet_u32GetVersion();

		// M2がLoなら、設定モードとして動作する
#ifdef CNFMST
		sAppData.bConfigMode = TRUE;
#else
		vPortAsInput(PORT_CONF2);
		if( !u8PowerUp ){
			if( !IS_APPCONF_OPT_PASS_SETTINGS() || bPortRead(PORT_CONF2) ){
				sAppData.bConfigMode = TRUE;
			}
		}
#endif

		if (sAppData.bConfigMode) {
			// 設定モードで起動

			// 設定用のコンフィグ
			sToCoNet_AppContext.u32AppId = APP_ID_CNFMST;
			sToCoNet_AppContext.u8Channel = CHANNEL_CNFMST;
			sToCoNet_AppContext.bRxOnIdle = TRUE;

			// Other Hardware
			vInitHardware(FALSE);

			// イベント処理の初期化
#ifdef CNFMST
			sToCoNet_AppContext.u16ShortAddress = SHORTADDR_CNFMST;
			vInitAppConfigMaster();
#else
			sToCoNet_AppContext.u16ShortAddress = 0x78; // 子機のアドレス（なんでも良い）
			vInitAppConfig();
#endif
			// インタラクティブモードの初期化
			Interactive_vInit();
		} else
#if !defined(CNFMST)
		// SHT21
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_SHT21 ) {
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)

			// ADC の初期化
			vInitADC();

			// Other Hardware
			vInitHardware(FALSE);

			// イベント処理の初期化
			vInitAppSHT21();
		} else
#if !defined(TWX0003)
		//	ボタン起動モード
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_BUTTON ) {
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)
			sToCoNet_AppContext.bSkipBootCalib = TRUE; // 起動時のキャリブレーションを省略する(保存した値を確認)

			// Other Hardware
			vInitHardware(FALSE);

			// ADC の初期化
			vInitADC();

			// イベント処理の初期化
			vInitAppButton();
		} else
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_UART ) {
			sToCoNet_AppContext.bSkipBootCalib = TRUE; // 起動時のキャリブレーションを省略する(保存した値を確認)

			// Other Hardware
			vInitHardware(FALSE);

			// UART 処理
			vInitAppUart();

			// インタラクティブモード
			Interactive_vInit();
		} else
		//	磁気スイッチなど
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_IO_TIMER ) {
			// ドアタイマーで起動
			// sToCoNet_AppContext.u8CPUClk = 1; // runs at 8MHz (Doze を利用するのであまり効果が無いかもしれない)
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)

			// Other Hardware
			vInitHardware(FALSE);

			// イベント処理の初期化
			vInitAppDoorTimer();
		} else
		// BME280
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_BME280 ) {
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)
			// ADC の初期化
			vInitADC();

			// Other Hardware
			vInitHardware(FALSE);

			// イベント処理の初期化
			vInitAppBME280();
		} else
		// S11059-02
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_S1105902 ) {
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)
			// ADC の初期化
			vInitADC();

			// Other Hardware
			vInitHardware(FALSE);

			// イベント処理の初期化
			vInitAppS1105902();
		} else
		// ADT7410
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_ADT7410 ) {
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)

			// ADC の初期化
			vInitADC();

			// Other Hardware
			vInitHardware(FALSE);

			// イベント処理の初期化
			vInitAppADT7410();
		} else
		// MPL115A2
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_MPL115A2 ) {
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)

			// ADC の初期化
			vInitADC();

			// Other Hardware
			vInitHardware(FALSE);

			// イベント処理の初期化
			vInitAppMPL115A2();
		} else
		// LIS3DH
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_LIS3DH ) {
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)

			// ADC の初期化
			vInitADC();

			// Other Hardware
			vInitHardware(FALSE);

			// イベント処理の初期化
			vInitAppLIS3DH();
		} else
		// L3GD20
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_L3GD20 ) {
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)
			// ADC の初期化
			vInitADC();
			// Other Hardware
			vInitHardware(FALSE);
			// イベント処理の初期化
			vInitAppL3GD20();
		} else
		// ADXL345
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_ADXL345 ) {
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)

			// ADC の初期化
			vInitADC();

			// Other Hardware
			vInitHardware(FALSE);

			// イベント処理の初期化
			vInitAppADXL345();
		} else
		// ADXL345
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_ADXL345_LOWENERGY ) {
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)

			// ADC の初期化
			vInitADC();

			// Other Hardware
			vInitHardware(FALSE);

			// イベント処理の初期化
			vInitAppADXL345_LowEnergy();
		} else
		// TSL2561
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_TSL2561 ) {
				sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う
				sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)

				// ADC の初期化
				vInitADC();

				// Other Hardware
				vInitHardware(FALSE);

				// イベント処理の初期化
				vInitAppTSL2561();
		} else
		//	LM61等のアナログセンサ用
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_STANDARD	// アナログセンサ
			|| sAppData.sFlash.sData.u8mode == PKT_ID_LM61) {	// LM61
			// 通常アプリで起動
			sToCoNet_AppContext.u8CPUClk = 3; // runs at 32Mhz
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)
			sToCoNet_AppContext.bSkipBootCalib = TRUE; // 起動時のキャリブレーションを省略する(保存した値を確認)

			// ADC の初期化
			vInitADC();

			// Other Hardware
			vInitHardware(FALSE);
			vInitPulseCounter();

			// イベント処理の初期化
			vInitAppStandard();
		} else
#endif // TWX0003
#endif // CNFMST
	    {
			;
		} // 終端の else 節

		// イベント処理関数の登録
		if (pvProcessEv1) {
			ToCoNet_Event_Register_State_Machine(pvProcessEv1);
		}
		if (pvProcessEv2) {
			ToCoNet_Event_Register_State_Machine(pvProcessEv2);
		}

		// ToCoNet DEBUG
		ToCoNet_vDebugInit(&sSerStream);
		ToCoNet_vDebugLevel(TOCONET_DEBUG_LEVEL);
	}
}

/**
 * スリープ復帰時の処理
 * @param bAfterAhiInit
 */
void cbAppWarmStart(bool_t bAfterAhiInit) {
	if (!bAfterAhiInit) {
		// before AHI init, very first of code.
		//  to check interrupt source, etc.

		sAppData.bWakeupByButton = FALSE;
		uint32 u32WakeStatus = u32AHI_DioWakeStatus();
		if(u8AHI_WakeTimerFiredStatus()) {
		} else
		if( ( u32WakeStatus & PORT_INPUT_MASK_ADXL345) && sAppData.sFlash.sData.u8mode == PKT_ID_ADXL345 ){
			sAppData.bWakeupByButton = TRUE;
		}else
		if( u32WakeStatus & u32DioPortWakeUp) {
			// woke up from DIO events
			sAppData.bWakeupByButton = TRUE;
		}
	} else {
		// disable brown out detect
		vAHI_BrownOutConfigure(0,//0:2.0V 1:2.3V
				FALSE,
				FALSE,
				FALSE,
				FALSE);

		// センサー用の制御 (Lo:Active)
#ifndef TWX0003
		vPortSetSns(TRUE);
		vPortAsOutput(DIO_SNS_POWER);
#endif

		// Other Hardware
		Interactive_vReInit();
		vSerialInit();

		// TOCONET DEBUG
		ToCoNet_vDebugInit(&sSerStream);
		ToCoNet_vDebugLevel(TOCONET_DEBUG_LEVEL);

		//	センサ特有の初期化
		//	リードスイッチとUART用でない限りADCを初期化
		if( sAppData.sFlash.sData.u8mode != PKT_ID_UART &&
			sAppData.sFlash.sData.u8mode != PKT_ID_IO_TIMER ){
			// ADC の初期化
			vInitADC();
		}

		// 他のハードの待ち
		vInitHardware(TRUE);
		//	ADXL345の場合、割り込みの原因を判別する。
		if( sAppData.sFlash.sData.u8mode == PKT_ID_ADXL345 ){
			u8Interrupt = u8Read_Interrupt();
		}

		if (!sAppData.bWakeupByButton) {
			// タイマーで起きた
		} else {
			// ボタンで起床した
		}
	}
}

/**
 * メイン処理
 */
void cbToCoNet_vMain(void) {
	if (psCbHandler && psCbHandler->pf_cbToCoNet_vMain) {
		(*psCbHandler->pf_cbToCoNet_vMain)();
	}
}

/**
 * 受信処理
 */
void cbToCoNet_vRxEvent(tsRxDataApp *pRx) {
	if (psCbHandler && psCbHandler->pf_cbToCoNet_vRxEvent) {
		(*psCbHandler->pf_cbToCoNet_vRxEvent)(pRx);
	}
}

/**
 * 送信完了イベント
 */
void cbToCoNet_vTxEvent(uint8 u8CbId, uint8 bStatus) {
	V_PRINTF(LB ">>> TxCmp %s(tick=%d,req=#%d) <<<",
			bStatus ? "Ok" : "Ng",
			u32TickCount_ms & 0xFFFF,
			u8CbId
			);

	if (psCbHandler && psCbHandler->pf_cbToCoNet_vTxEvent) {
		(*psCbHandler->pf_cbToCoNet_vTxEvent)(u8CbId, bStatus);
	}

	return;
}

/**
 * ネットワークイベント
 * @param eEvent
 * @param u32arg
 */
void cbToCoNet_vNwkEvent(teEvent eEvent, uint32 u32arg) {
	if (psCbHandler && psCbHandler->pf_cbToCoNet_vNwkEvent) {
		(*psCbHandler->pf_cbToCoNet_vNwkEvent)(eEvent, u32arg);
	}
}

/**
 * ハードウェアイベント処理（割り込み遅延実行）
 */
void cbToCoNet_vHwEvent(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	if (psCbHandler && psCbHandler->pf_cbToCoNet_vHwEvent) {
		(*psCbHandler->pf_cbToCoNet_vHwEvent)(u32DeviceId, u32ItemBitmap);
	}
}

/**
 * ハードウェア割り込みハンドラ
 */
uint8 cbToCoNet_u8HwInt(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	bool_t bRet = FALSE;
	if (psCbHandler && psCbHandler->pf_cbToCoNet_u8HwInt) {
		bRet = (*psCbHandler->pf_cbToCoNet_u8HwInt)(u32DeviceId, u32ItemBitmap);
	}
	return bRet;
}

/**
 * ADCの初期化
 */
static void vInitADC() {
	// ADC
	vADC_Init(&sAppData.sObjADC, &sAppData.sADC, TRUE);
	sAppData.u8AdcState = 0xFF; // 初期化中

#ifdef USE_TEMP_INSTDOF_ADC2
	sAppData.sObjADC.u8SourceMask =
			TEH_ADC_SRC_VOLT | TEH_ADC_SRC_ADC_1 | TEH_ADC_SRC_TEMP;
#else
	sAppData.sObjADC.u8SourceMask =
			TEH_ADC_SRC_VOLT | TEH_ADC_SRC_ADC_1 | TEH_ADC_SRC_ADC_2;
#endif
}

/**
 * パルスカウンタの初期化
 * - cold boot 時に1回だけ初期化する
 */
#if !(defined(TWX0003) || defined(CNFMST))
static void vInitPulseCounter() {
	// カウンタの設定
	bAHI_PulseCounterConfigure(
		E_AHI_PC_0,
		1,      // 0:RISE, 1:FALL EDGE
		0,      // Debounce 0:off, 1:2samples, 2:4samples, 3:8samples
		FALSE,   // Combined Counter (32bitカウンタ)
		FALSE);  // Interrupt (割り込み)

	// カウンタのセット
	bAHI_SetPulseCounterRef(
		E_AHI_PC_0,
		0x0); // 何か事前に値を入れておく

	// カウンタのスタート
	bAHI_StartPulseCounter(E_AHI_PC_0); // start it

	// カウンタの設定
	bAHI_PulseCounterConfigure(
		E_AHI_PC_1,
		1,      // 0:RISE, 1:FALL EDGE
		0,      // Debounce 0:off, 1:2samples, 2:4samples, 3:8samples
		FALSE,   // Combined Counter (32bitカウンタ)
		FALSE);  // Interrupt (割り込み)

	// カウンタのセット
	bAHI_SetPulseCounterRef(
		E_AHI_PC_1,
		0x0); // 何か事前に値を入れておく

	// カウンタのスタート
	bAHI_StartPulseCounter(E_AHI_PC_1); // start it
}
#endif

/**
 * ハードウェアの初期化を行う
 * @param f_warm_start TRUE:スリープ起床時
 */
static void vInitHardware(int f_warm_start) {
#ifndef TWX0003
	// 入力ポートを明示的に指定する
	if( sAppData.sFlash.sData.u8mode == PKT_ID_BUTTON ){
		vPortAsInput(DIO_BUTTON);
		vAHI_DioWakeEnable(PORT_INPUT_MASK, 0); // also use as DIO WAKE SOURCE
		vAHI_DioWakeEdge(PORT_INPUT_MASK, 0); // 割り込みエッジ（立上がりに設定）
	}else if( sAppData.sFlash.sData.u8mode == PKT_ID_ADXL345 ){
		vPortAsInput(PORT_INPUT2);
		vPortAsInput(PORT_INPUT3);
		vPortDisablePullup(PORT_INPUT2);
		vPortDisablePullup(PORT_INPUT3);
		vAHI_DioWakeEnable(PORT_INPUT_MASK_ADXL345, 0); // also use as DIO WAKE SOURCE
		vAHI_DioWakeEdge(PORT_INPUT_MASK_ADXL345, 0); // 割り込みエッジ（立上がりに設定）
	}else if ( sAppData.sFlash.sData.u8mode == PKT_ID_ADXL345_LOWENERGY ) {
		vPortDisablePullup(PORT_INPUT2);
		vPortDisablePullup(PORT_INPUT3);
		vAHI_DioWakeEnable(0, PORT_INPUT_MASK_ADXL345); // DISABLE DIO WAKE SOURCE
	}
#endif

	// Serial Port の初期化
	{
		uint32 u32baud;

		// BPS ピンが Lo の時は 38400bps
		vPortAsInput(PORT_BAUD);
		u32baud = bPortRead(PORT_BAUD) ? UART_BAUD_SAFE : UART_BAUD;

		// シリアル初期化
		vSerialInit(u32baud, NULL);
	}

	// SMBUS の初期化
	vSMBusInit();
}

/** @ingroup MASTER
 * UART を初期化する
 * @param u32Baud ボーレート
 */
void vSerialInit(uint32 u32Baud, tsUartOpt *pUartOpt) {
	/* Create the debug port transmit and receive queues */
	static uint8 au8SerialTxBuffer[768];
	static uint8 au8SerialRxBuffer[256];

	/* Initialise the serial port to be used for debug output */
	sSerPort.pu8SerialRxQueueBuffer = au8SerialRxBuffer;
	sSerPort.pu8SerialTxQueueBuffer = au8SerialTxBuffer;
	sSerPort.u32BaudRate = u32Baud;
	sSerPort.u16AHI_UART_RTS_LOW = 0xffff;
	sSerPort.u16AHI_UART_RTS_HIGH = 0xffff;
	sSerPort.u16SerialRxQueueSize = sizeof(au8SerialRxBuffer);
	sSerPort.u16SerialTxQueueSize = sizeof(au8SerialTxBuffer);
	sSerPort.u8SerialPort = UART_PORT;
	sSerPort.u8RX_FIFO_LEVEL = E_AHI_UART_FIFO_LEVEL_1;
	SERIAL_vInitEx(&sSerPort, pUartOpt);

	/* prepare stream for vfPrintf */
	sSerStream.bPutChar = SERIAL_bTxChar;
	sSerStream.u8Device = UART_PORT;
}

/**
 * 初期化メッセージ
 */
void vSerInitMessage() {
	if( sAppData.sFlash.sData.u8mode == PKT_ID_ADXL345 && sAppData.sFlash.sData.i16param == NEKOTTER ){
		V_PRINTF(LB LB "*** " NEKO_NAME " (ED_Inp) %d.%02d-%d ***", VERSION_MAIN, VERSION_SUB, VERSION_VAR);
	}else{
		V_PRINTF(LB LB "*** " APP_NAME " (ED_Inp) %d.%02d-%d ***", VERSION_MAIN, VERSION_SUB, VERSION_VAR);
	}
	V_PRINTF(LB "* App ID:%08x Long Addr:%08x Short Addr %04x LID %02d Calib=%d",
			sToCoNet_AppContext.u32AppId, ToCoNet_u32GetSerial(), sToCoNet_AppContext.u16ShortAddress,
			sAppData.sFlash.sData.u8id,
			sAppData.sFlash.sData.u16RcClock);
	V_FLUSH();
}

/**
 * コマンド受け取り時の処理
 * @param pCmd
 */
void vProcessSerialCmd(tsSerCmd_Context *pCmd) {
	// アプリのコールバックを呼び出す
	if (pf_cbProcessSerialCmd) {
		(*pf_cbProcessSerialCmd)(pCmd);
	}

#if 0
	V_PRINTF(LB "! cmd len=%d data=", pCmd->u16len);
	int i;
	for (i = 0; i < pCmd->u16len && i < 8; i++) {
		V_PRINTF("%02X", pCmd->au8data[i]);
	}
	if (i < pCmd->u16len) {
		V_PRINTF("...");
	}
#endif

	return;
}

/**
 * 通常送信関数
 *
 * @param *pu8Data 送信するデータ本体
 * @param u8Length データ長
 * @return 送信成功であれば TRUE
 */
bool_t bSendMessage( uint8* pu8Data, uint8 u8Length ){
	bool_t	bOk = TRUE;

	// 暗号化鍵の登録
	if (IS_APPCONF_OPT_SECURE()) {
		bool_t bRes = bRegAesKey(sAppData.sFlash.sData.u32EncKey);
		V_PRINTF(LB "*** Register AES key (%d) ***", bRes);
	}

	// 初期化後速やかに送信要求
	V_PRINTF(LB"[SNS_COMP/TX]");
	sAppData.u16frame_count++; // シリアル番号を更新する
	tsTxDataApp sTx;
	memset(&sTx, 0, sizeof(sTx)); // 必ず０クリアしてから使う！
	uint8 *q =  sTx.auData;

	sTx.u32SrcAddr = ToCoNet_u32GetSerial();

	if (IS_APPCONF_OPT_SECURE()) {
		sTx.bSecurePacket = TRUE;
	}

	if (IS_APPCONF_OPT_TO_ROUTER()) {
		// ルータがアプリ中で一度受信して、ルータから親機に再配送
		sTx.u32DstAddr = TOCONET_NWK_ADDR_NEIGHBOUR_ABOVE;
	} else {
		// ルータがアプリ中では受信せず、単純に中継する
		sTx.u32DstAddr = TOCONET_NWK_ADDR_PARENT;
	}

	// ペイロードの準備
	S_OCTET('T');
	S_OCTET(sAppData.sFlash.sData.u8id);
	S_BE_WORD(sAppData.u16frame_count);

	if( sAppData.sFlash.sData.u8mode == 0xA1 ){
		S_OCTET(0x35);	// ADXL345 LowEnergy Mode の時、普通のADXL345として送る
	}else{
		S_OCTET(sAppData.sFlash.sData.u8mode); // パケット識別子
	}

	//	センサ固有のデータ
	memcpy(q,pu8Data,u8Length);
	q += u8Length;

	sTx.u8Cmd = 0; // 0..7 の値を取る。パケットの種別を分けたい時に使用する
	sTx.u8Len = q - sTx.auData; // パケットのサイズ
	sTx.u8CbId = sAppData.u16frame_count & 0xFF; // TxEvent で通知される番号、送信先には通知されない
	sTx.u8Seq = sAppData.u16frame_count & 0xFF; // シーケンス番号(送信先に通知される)
	sTx.u8Retry = sAppData.u8Retry;

	//	送信処理
	bOk = ToCoNet_Nwk_bTx(sAppData.pContextNwk, &sTx);
	if ( bOk ) {
		ToCoNet_Tx_vProcessQueue(); // 送信処理をタイマーを待たずに実行する
		V_PRINTF(LB"TxOk");
	} else {
		V_PRINTF(LB"TxFl");
	}
	return bOk;
}

/**
 * センサーアクティブ時のポートの制御を行う
 *
 * @param bActive ACTIVE時がTRUE
 */
#ifndef TWX0003
void vPortSetSns(bool_t bActive) {
	if (IS_APPCONF_OPT_INVERSE_SNS_ACTIVE()) {
		bActive = !bActive;
	}

	vPortSet_TrueAsLo(DIO_SNS_POWER, bActive);
}
#endif
/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
