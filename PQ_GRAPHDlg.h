// PQ_GRAPHDlg.h : �w�b�_�[ �t�@�C��
//

#pragma once
#include "afxwin.h"


// CPQ_GRAPHDlg �_�C�A���O
class CPQ_GRAPHDlg : public CDialog
{
// �R���X�g���N�V����
public:
	CPQ_GRAPHDlg(CWnd* pParent = NULL);	// �W���R���X�g���N�^

// �_�C�A���O �f�[�^
	enum { IDD = IDD_PQ_GRAPH_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV �T�|�[�g


// ����
protected:
	HICON m_hIcon;

	// �������ꂽ�A���b�Z�[�W���蓖�Ċ֐�
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	CEdit msgED2;
	afx_msg void OnBnClickedButton1();
	afx_msg void OnBnClickedButton2();
	afx_msg void OnBnClickedButton3();
	LRESULT CPQ_GRAPHDlg::OnMessageRCV( WPARAM wParam, LPARAM lParam);	// ���̍s�͎���͂���K�v������

private:
	// ------------------------------Font------------------------------
	CFont *m_newFont;
	bool fontFlag;
	int fontTime;

	// ------------------------------OpneGL------------------------------
	CStatic m_glView;
	bool glFlag;
	CDC *m_pDC; // Picture Control�̃f�o�C�X�R���e�L�X�g��ۑ�����ϐ�
	HGLRC m_GLRC; // OpenGL�̃����_�����O�R���e�L�X�g�̕ϐ�
	BOOL SetUpPixelFormat(HDC hdc);

public:
	// ------------------------------Font------------------------------
	void fontCallback(CString soundInfo); // Font�X���b�h�p�̃R�[���o�b�N�֐�
	void setFontStyle();
	CEdit msgED1; 

	// ------------------------------OpneGL------------------------------
	void glCallback(); // OpenGL�X���b�h�p�̃R�[���o�b�N�֐�
	void glSetup(); 	// OpenGl�̃R���e�L�X�g�̏�����OpenGL�̏��������s���֐�

	// ------------------------------MIDI------------------------------
	void midiCallback(const double gx, const double gy, const double ey);

	// ------------------------------DataReciever------------------------------
	CEdit msgED3;
	afx_msg void OnDestroy();
};
