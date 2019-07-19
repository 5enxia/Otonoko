// PQ_GRAPH.h : PROJECT_NAME アプリケーションのメイン ヘッダー ファイルです。
//

#pragma once

#ifndef __AFXWIN_H__
	#error "PCH に対してこのファイルをインクルードする前に 'stdafx.h' をインクルードしてください"
#endif

#include "resource.h"		// メイン シンボル


// CPQ_GRAPHApp:
// このクラスの実装については、PQ_GRAPH.cpp を参照してください。
//

class CPQ_GRAPHApp : public CWinApp
{
public:
	CPQ_GRAPHApp();

// オーバーライド
	public:
	virtual BOOL InitInstance();

// 実装

	DECLARE_MESSAGE_MAP()
};

extern CPQ_GRAPHApp theApp;