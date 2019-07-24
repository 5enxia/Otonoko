// PQ_GRAPHDlg.h 

#pragma once
#include "afxwin.h"


// CPQ_GRAPHDlg Dialog 
class CPQ_GRAPHDlg : public CDialog
{
// Contruction
public:
	CPQ_GRAPHDlg(CWnd* pParent = NULL);	// Standard Constructor 

// Dialog data
	enum { IDD = IDD_PQ_GRAPH_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Automaitcally Generated Message Functions
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
	LRESULT CPQ_GRAPHDlg::OnMessageRCV( WPARAM wParam, LPARAM lParam);	// CallBack Function in DataReciver

private:
	// ------------------------------Font------------------------------
	CFont *m_newFont;
	bool fontFlag;										// Font Thread Flag
	int fontTime;										// Time Counter

	// ------------------------------OpneGL------------------------------
	CStatic m_glView; 
	bool glFlag;										// OpenGL Thread Flag
	CDC *m_pDC;									// Picture Control's Device Context
	HGLRC m_GLRC;								// OpenGL's Device Context
	BOOL SetUpPixelFormat(HDC hdc);		// Setup Pixel Format in Picture Control

public:
	// ------------------------------Font------------------------------
	void fontCallback(CString soundInfo);	// Font Callback Function
	void setFontStyle();							// Set Font-style
	CEdit msgED1;									// Edit Control 1

	// ------------------------------OpneGL------------------------------
	void glCallback();								// OpenGL Callback Function
	void glSetup(); 									// Setup OpenGL & OpenGL Context

	// ------------------------------MIDI------------------------------
	void midiCallback(const double gx, const double gy, const double ey);	// Sound Callback Function

	// ------------------------------DataReciever------------------------------
	CEdit msgED3;									// Edit Control 3
	afx_msg void OnDestroy();					// Run when Window is Destroied
};
