#include "stdafx.h"

extern HMIDIOUT hMIDI;

namespace Sound {
	int count = 0;
	double border[3] = {
		0,0,0,
	};
	
	// ����I������ۂ̃C���f�b�N�X
	int toneIndex = 0;
	int scaleIndex = 0;

	// ���K�A�I�N�^�[�u���A���F�̐�
	const int NUM_OF_SCALES = 5;
	const int OCTAVE = 3;
	const int NUM_OF_INSTRUMENTS = 3;

	bool soundFlag = false;
	
	// ���K�̐ݒ�
	DWORD scales[NUM_OF_SCALES][OCTAVE] = {
		{0x7f3090, 0x7f3490, 0x7f3790},		//�h
		{0x7f3290, 0x7f3690, 0x7f3990},		//��
		{0x7f3490, 0x7f3890, 0x7f3b90},		//�~
		{0x7f3790, 0x7f3b90, 0x7f3e90},		//�\
		{0x7f3990, 0x7f3d90, 0x7f4090},		//��
	};
	CString scale_name[NUM_OF_SCALES] = {
		L"�h", L"��",L"�~",L"�t�@",L"�\",
	};

	// ���F�̐ݒ�
	DWORD instruments[NUM_OF_INSTRUMENTS] = {
		 0x1dc0,		//OD�M�^�[
		 0x18c0,		//�A�R�M
		 0x24c0,		//�X���b�v�x�[�X
		// 0x7fc0,			//�e��
	};
	CString instrument_name[NUM_OF_INSTRUMENTS] = { 
		L"�M�^�[", L"�A�R�M", L"�X���b�v�x�[�X",//L"�e��", 
	};

	int deg; // ey��x���ɕϊ��������̂��i�[����p�����[�^

	// -1~1�̒�`���0~180�x�ɕϊ�����
	void calcToDeg(const double ey) {
		deg = ((int)((180 / PI) * asin(ey)) + 90);
	}


	// �x�����特�K�̃C���f�b�N�X��I������
	void setScaleIndex() {
		int area = 180 / NUM_OF_SCALES;
		for (int i = 0; i < NUM_OF_SCALES; i++){
			if (deg >= (i * area) && deg < (i + 1) * area) scaleIndex = i;
		}
	}

	// �t���O
	bool local_flag = false;
	bool gxFlag,gyFlag = false;
	float gyUpperLimit = 40000.0;

	// �����o�͂���֐�
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
				Sleep(100);	// 0.1sec�炷
			}
			for (int i = 0; i < OCTAVE; i++) midiOutShortMsg(hMIDI, scales[scaleIndex][i] - 0x000010);
		}
		else if (gy <gyUpperLimit && !gyFlag) gyFlag = true;
	}

	// ���݂̉��F�Ɖ��K�̏���Ԃ��֐�
	CString getInfo(){
		CString str;
		str = _T("Tone:") + instrument_name[toneIndex] + _T("\r\n");
		str += _T("Scale:") + scale_name[scaleIndex] + _T("\r\n");
		return str;
	}
}
