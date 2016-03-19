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

// 本ファイルは、Interactive.h からインクルードされる

/*************************************************************************
 * VERSION
 *************************************************************************/

#include "Version.h"
#if defined(PARENT)
# define VERSION_CODE 1
#elif defined(ROUTER)
# define VERSION_CODE 2
#elif defined(ENDDEVICE_INPUT)
# define VERSION_CODE 3
#elif defined(ENDDEVICE_REMOTE)
# define VERSION_CODE 4
#else
# error "VERSION_CODE is not defined"
#endif

/*************************************************************************
 * OPTION 設定
 *************************************************************************/

/**
 * メッセージ出力モードの可否
 */
#define E_APPCONF_OPT_VERBOSE 0x00010000UL
#define IS_APPCONF_OPT_VERBOSE() ((sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_VERBOSE) != 0) //!< E_APPCONF_OPT_VERBOSE 判定

/**
 * パケット通信に暗号化を行う
 */
#define E_APPCONF_OPT_SECURE 0x00001000UL
#define IS_APPCONF_OPT_SECURE() ((sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_SECURE) != 0) //!< E_APPCONF_OPT_SECURE 判定

/**
 * ADXL345のACTIVE/INACTIVE検出モードでLINK(Active -> INACTIVE -> ACYIVE　-> ...)を無効にする。
 */
#define E_APPCONF_OPT_ADXL345_DISABLE_LINK 0x00002000UL
#define IS_APPCONF_OPT_ADXL345_DISABLE_LINK() ((sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_ADXL345_DISABLE_LINK) != 0) //!< E_APPCONF_OPT_ADXL345_DISABLE_LINK 判定

/**
 * PWMを線形に変化
 */
#define E_APPCONF_OPT_ADXL345_SHAKE_LINEAR 0x80000000UL
#define IS_APPCONF_OPT_ADXL345_SHAKE_LINEAR() ((sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_ADXL345_SHAKE_LINEAR) != 0) //!< E_APPCONF_OPT_ADXL345_DISABLE_LINK 判定

/**
 * 定義済みの場合、各ルータまたは親機宛に送信し、ルータから親機宛のパケットを
 * 送信する受信したルータすべての情報が親機に転送されることになる。
 * この場合、複数の受信パケットを分析する事で一番近くで受信したルータを特定
 * できる。
 *
 * 未定義の場合、直接親機宛の送信が行われる。原則として受信パケットを一つだけ
 * 親機が表示することになり、近接ルータの特定はできない。
 */
#define E_APPCONF_OPT_TO_ROUTER 0x00000001UL
#define IS_APPCONF_OPT_TO_ROUTER() ((sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_TO_ROUTER) != 0) //!< E_APPCONF_OPT_TO_ROUTER 判定

/**
 * 親機を標準アプリにする(2525A用)
 */
#define E_APPCONF_OPT_APP_TWELITE 0x00000010UL
#define IS_APPCONF_OPT_APP_TWELITE() ((sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_APP_TWELITE) != 0) //!< E_APPCONF_OPT_DOOR_TIMER 判定

/**
 * 親機のUART出力を SimpleTag v3 互換のセミコロン区切りにする
 */
#define E_APPCONF_OPT_SHT21 0x00000020UL
#define IS_APPCONF_OPT_SHT21() ((sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_SHT21) != 0) //!< E_APPCONF_OPT_SHT21 判定

/**
 * 起床時間を±12%の範囲でランダムにする
 */
#define E_APPCONF_OPT_WAKE_RANDOM 0x00000040UL
#define IS_APPCONF_OPT_WAKE_RANDOM() ((sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_WAKE_RANDOM) != 0) //!< E_APPCONF_OPT_WAKE_RANDOM 判定

/**
 * UARTアプリを有効にする
 */
#define E_APPCONF_OPT_UART 0x00000100UL
#define IS_APPCONF_OPT_UART() ((sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_UART) != 0) //!< E_APPCONF_OPT_UART 判定

/**
 * バイナリモード
 */
#define E_APPCONF_OPT_UART_BIN 0x00000200UL
#define IS_APPCONF_OPT_UART_BIN() ((sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_UART_BIN) != 0) //!< E_APPCONF_OPT_UART_BIN 判定

/**
 * 子機の電源投入時の設定モードへの遷移を飛ばす
 */
#define E_APPCONF_OPT_PASS_SETTINGS 0x00000400UL
#define IS_APPCONF_OPT_PASS_SETTINGS() ((sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_PASS_SETTINGS) != 0) //!< E_APPCONF_OPT_PASS_SETTINGS 判定

/**
 * Parentの毎秒表示をやめる
 */
#define E_APPCONF_OPT_PARENT_OUTPUT 0x00020000UL
#define IS_APPCONF_OPT_PARENT_OUTPUT() ((sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_PARENT_OUTPUT) != 0) //!< E_APPCONF_OPT_PARENT_OUTPUT 判定

/**
 * UART ボーレート設定の強制
 */
#define E_APPCONF_OPT_UART_FORCE_SETTINGS 0x00040000UL
#define IS_APPCONF_OPT_UART_FORCE_SETTINGS() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_UART_FORCE_SETTINGS) //!< E_APPCONF_OPT_UART_FORCE_SETTINGS 判定

/**
 * SNS アクティブピンの出力を反転させる (Hi Active)
 */
#define E_APPCONF_OPT_INVERSE_SNS_ACTIVE 0x00100000UL
#define IS_APPCONF_OPT_INVERSE_SNS_ACTIVE() ((sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_INVERSE_SNS_ACTIVE) != 0) //!< E_APPCONF_OPT_INVERSE_SNS_ACTIVE 判定

