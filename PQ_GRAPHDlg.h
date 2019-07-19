// PQ_GRAPHDlg.h : ヘッダー ファイル
//

#pragma once
#include "afxwin.h"


// CPQ_GRAPHDlg ダイアログ
class CPQ_GRAPHDlg : public CDialog
{
// コンストラクション
public:
	CPQ_GRAPHDlg(CWnd* pParent = NULL);	// 標準コンストラクタ

// ダイアログ データ
	enum { IDD = IDD_PQ_GRAPH_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV サポート


// 実装
protected:
	HICON m_hIcon;

	// 生成された、メッセージ割り当て関数
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
	LRESULT CPQ_GRAPHDlg::OnMessageRCV( WPARAM wParam, LPARAM lParam);	// この行は手入力する必要がある

private:
	// ------------------------------Font------------------------------
	CFont *m_newFont;
	bool fontFlag;
	int fontTime;

	// ------------------------------OpneGL------------------------------
	CStatic m_glView;
	bool glFlag;
	CDC *m_pDC; // Picture Controlのデバイスコンテキストを保存する変数
	HGLRC m_GLRC; // OpenGLのレンダリングコンテキストの変数
	BOOL SetUpPixelFormat(HDC hdc);

public:
	// ------------------------------Font------------------------------
	void fontCallback(CString soundInfo); // Fontスレッド用のコールバック関数
	void setFontStyle();
	CEdit msgED1; 

	// ------------------------------OpneGL------------------------------
	void glCallback(); // OpenGLスレッド用のコールバック関数
	void glSetup(); 	// OpenGlのコンテキストの準備とOpenGLの初期化を行う関数

	// ------------------------------MIDI------------------------------
	void midiCallback(const double gx, const double gy, const double ey);

	// ------------------------------DataReciever------------------------------
	CEdit msgED3;
	afx_msg void OnDestroy();
};
