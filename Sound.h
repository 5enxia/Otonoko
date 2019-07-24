#include "stdafx.h"

extern HMIDIOUT hMIDI;

namespace Sound {
	int count = 0;
	double border[3] = {0,0,0};
	
	int toneIndex = 0; // Selected Tone Index
	int scaleIndex = 0;	// Selected Scale Index

	const int NUM_OF_SCALES = 5;
	const int NUM_OF_OCTAVES = 3;
	const int NUM_OF_INSTRUMENTS = 6;

	
	// Scales Setting
	DWORD scales[NUM_OF_SCALES][NUM_OF_OCTAVES] = {
			{0x7f3790,0x7f3b90,0x7f3e90},			// G4
			{0x7f3a90, 0x7f3e90, 0x7f4190},		// A#4
			{0x7f3c90 ,0x7f4090, 0x7f4390},		// C5
			{0x7f3d90,0x7f4190 ,0x7f4490},		// C#5
			{0x7f3090 ,0x7f3490, 0x7f3790},		// C4
	};
	CString scale_name[NUM_OF_SCALES] = {
		L"G4：ソ", L"A#4：ラ#", L"C5：ド", L"C#5,ド#",L"C4：ド"
	};

	// Tones Setting
	DWORD instruments[NUM_OF_INSTRUMENTS] = {
			 0x1dc0,		//OD Guitars
			 0x38c0,		// Trumpet
			 0x2ec0,		// Harp
			 0x6bc0,		// Japanese Harp
			 0x13c0,		// Organ
			 0x7fc0,			// Gun
	};
	CString instrument_name[NUM_OF_INSTRUMENTS] = { 
		L"ODギター", L"トランペット", L"ハープ",L"琴",L"オルガン",L"銃声"
	// L"Old Guiter", L"Trunpet", L"Harp", L"Japanese Harp", L"Organ", L"Gun Sound"
	};


	int deg; // Container of deg (Converted for ey) 

	/*
	* Convert -1~1 to degrees
	*/
	void calcToDeg(const double ey) {
		deg = ((int)((180 / PI) * asin(ey)) + 90);
	}


	/*
	* Select Tone Index from degrees
	*/
	void setScaleIndex() {
		int area = 180 / NUM_OF_SCALES;
		for (int i = 0; i < NUM_OF_SCALES; i++){
			if (deg >= (i * area) && deg < (i + 1) * area) scaleIndex = i;
		}
	}

	// Flags
	bool soundFlag = false;
	bool local_flag = false;
	bool gxFlag,gyFlag = false;
	float gyUpperLimit = 40000.0;

	/*
	* Emit Sound
	*/
	void emit(const float gx, const float gy) {
		float gxUpperLimit = 16000.0;
		float gxLowerLimit = -12000.0;

		// Select Tone Index
		if (gx > gxUpperLimit && gxFlag) {
			gxFlag = false;
			if (toneIndex < NUM_OF_INSTRUMENTS) toneIndex++; 
			else toneIndex = 0;
		}
		else if (gx < gxLowerLimit && !gxFlag) gxFlag = true;
		// Emit MIDI Sound
		if (gy > gyUpperLimit && gyFlag) {
			gyFlag = false;
			midiOutShortMsg(hMIDI, instruments[toneIndex]);																			// Set Tone
			int tmp = scaleIndex;
			for (int i = 0; i < NUM_OF_OCTAVES; i++) midiOutShortMsg(hMIDI, scales[tmp][i]);							// Emit MIDI Sound
			soundFlag = true;
			while (soundFlag) Sleep(100);																											// Sleep 100ms
			for (int i = 0; i < NUM_OF_OCTAVES; i++) midiOutShortMsg(hMIDI, scales[tmp][i] - 0x000010);		// Stop MIDI Sound
		}
		else if (gy <gyUpperLimit && !gyFlag) gyFlag = true;
	}

	
	/*
	*　Return Sound Info (Tone & Sclae)
	*/
	CString getInfo(){
		CString str;
		str = _T("Tone:") + instrument_name[toneIndex] + _T("\r\n");
		str += _T("Scale:") + scale_name[scaleIndex] + _T("\r\n");
		return str;
	}
}
