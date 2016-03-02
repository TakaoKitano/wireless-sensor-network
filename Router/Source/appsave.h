/*
 * appsave.h
 *
 *  Created on: 2014/03/12
 *      Author: seigo13
 */

#ifndef APPSAVE_RO_H_
#define APPSAVE_RO_H_

#include <jendefs.h>

/** @ingroup FLASH
 * フラッシュ格納データ構造体
 */
typedef struct _tsFlashApp {
	uint32 u32appkey;		//!<
	uint32 u32ver;			//!<

	uint32 u32appid;		//!< アプリケーションID
	uint32 u32chmask;		//!< 使用チャネルマスク（３つまで）
	uint8 u8id;				//!< 論理ＩＤ (子機 1～100まで指定)
	uint8 u8ch;				//!< チャネル（未使用、チャネルマスクに指定したチャネルから選ばれる）
	uint8 u8pow;			//!< 出力パワー (0-3)
	uint8 u8layer;			//!< レイヤ

	uint32 u32EncKey;		//!< 暗号化キー(128bitだけど、32bitの値から鍵を生成)
	uint32 u32Opt;			//!< 色々オプション
} tsFlashApp;

#endif /* APPSAVE_H_ */
