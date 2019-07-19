#include "stdafx.h"

extern HMIDIOUT hMIDI;

namespace Sound {
	int count = 0;
	double border[3] = {
		0,0,0,
	};
	
	// 音を選択する際のインデックス
	int toneIndex = 0;
	int scaleIndex = 0;

	// 音階、オクターブ数、音色の数
	const int NUM_OF_SCALES = 5;
	const int OCTAVE = 3;
	const int NUM_OF_INSTRUMENTS = 3;

	bool soundFlag = false;
	
	// 音階の設定
	DWORD scales[NUM_OF_SCALES][OCTAVE] = {
		{0x7f3090, 0x7f3490, 0x7f3790},		//ド
		{0x7f3290, 0x7f3690, 0x7f3990},		//レ
		{0x7f3490, 0x7f3890, 0x7f3b90},		//ミ
		{0x7f3790, 0x7f3b90, 0x7f3e90},		//ソ
		{0x7f3990, 0x7f3d90, 0x7f4090},		//ラ
	};
	CString scale_name[NUM_OF_SCALES] = {
		L"ド", L"レ",L"ミ",L"ファ",L"ソ",
	};

	// 音色の設定
	DWORD instruments[NUM_OF_INSTRUMENTS] = {
		 0x1dc0,		//ODギター
		 0x18c0,		//アコギ
		 0x24c0,		//スラップベース
		// 0x7fc0,			//銃声
	};
	CString instrument_name[NUM_OF_INSTRUMENTS] = { 
		L"ギター", L"アコギ", L"スラップベース",//L"銃声", 
	};

	int deg; // eyを度数に変換したものを格納するパラメータ

	// -1~1の定義域を0~180度に変換する
	void calcToDeg(const double ey) {
		deg = ((int)((180 / PI) * asin(ey)) + 90);
	}


	// 度数から音階のインデックスを選択する
	void setScaleIndex() {
		int area = 180 / NUM_OF_SCALES;
		for (int i = 0; i < NUM_OF_SCALES; i++){
			if (deg >= (i * area) && deg < (i + 1) * area) scaleIndex = i;
		}
	}

	// フラグ
	bool local_flag = false;
	bool gxFlag,gyFlag = false;
	float gyUpperLimit = 40000.0;

	// 音を出力する関数
	void emit(const float gx, const float gy) {
		float gxUpperLimit = 16000.0;
		float gxLowerLimit = -12000.0;

		if (gx > gxUpperLimit && gxFlag) {
			gxFlag = false;
			if (toneIndex < NUM_OF_INSTRUMENTS) toneIndex++; 
			else toneIndex = 0;
		}
		else if (gx < gxLowerLimit && !gxFlag) gxFlag = true;
		if (gy > gyUpperLimit && gyFlag) {
			gyFlag = false;
			midiOutShortMsg(hMIDI, instruments[toneIndex]);
			for (int i = 0; i < OCTAVE; i++) midiOutShortMsg(hMIDI, scales[scaleIndex][i]);
			soundFlag = true;
			while (soundFlag) {
				Sleep(100);	// 0.1sec鳴らす
			}
			for (int i = 0; i < OCTAVE; i++) midiOutShortMsg(hMIDI, scales[scaleIndex][i] - 0x000010);
		}
		else if (gy <gyUpperLimit && !gyFlag) gyFlag = true;
	}

	// 現在の音色と音階の情報を返す関数
	CString getInfo(){
		CString str;
		str = _T("Tone:") + instrument_name[toneIndex] + _T("\r\n");
		str += _T("Scale:") + scale_name[scaleIndex] + _T("\r\n");
		return str;
	}
}
