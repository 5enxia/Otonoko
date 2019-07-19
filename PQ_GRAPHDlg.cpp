// PQ_GRAPHDlg.cpp : 実装ファイル
//

#include "stdafx.h"
#include "PQ_GRAPH.h"
#include "PQ_GRAPHDlg.h"

// ------------------------------OpneGL------------------------------
#include "Viewer.h"
// ------------------------------MIDI------------------------------
#include "Sound.h"
static HMIDIOUT hMIDI;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include <process.h>
// process.hはマルチスレッドＡＰＩを使用するために必要

#define WM_RCV (WM_USER+1)

#define MaxComPort	99							// 識別できるシリアルポート番号の限界
#define DefaultPort 4

int RScomport = DefaultPort;					// default COM port #4
CString ParamFileName = _T("PQ_GRAPH.txt");		// テキストファイルにて下記の情報を入れておく
												// シリアル通信ポートの番号
												// データファイル書き出し先のディレクトリ名
												// ※テキストエディタにて内容を変更できる

CString DefaultDir = _T("D:\\");				// データファイルを書き出す際のデフォルトディレクトリ
CString CurrentDir = _T("D:\\");				// Parameter Fileの内容で上書きされる

HANDLE	hCOMnd = INVALID_HANDLE_VALUE;			// 非同期シリアル通信用ファイルハンドル
OVERLAPPED rovl, wovl;							// 非同期シリアル通信用オーバーラップド構造体

HWND hDlg;										// ダイアログ自体のウィンドウハンドルを格納する変数
HANDLE serialh;									// 通信用スレッドのハンドル

int rf;											// 通信用スレッドの走行状態制御用変数
int datasize;									// 正しく受信したデータのサンプル数

#define DATASORT 11								// データの種類は11種類
												// 16-b int Ax, Ay, Az, Gx, Gy, Gz, Temperature,
												// float e4x, e4y, e4z
												// float alpha

												// databuf[SORT][i];
												// 1,2,3 -> ax, ay, az
												// 4,5,6 -> gx, gy, gz
												// 7 -> temperature
												// 8, 9, 10 -> e4x, e4y, e4z
												// 11 -> alpha

#define PACKETSIZE 33							// ワイヤレス通信における１パケットあたりのデータバイト数
												// PREAMBLE 1
												// SEQ 1
												// Ax, Ay, Az, Gx, Gy, Gz, Temp	 2 bytes each (14)
												// e4x, e4y, e4z 4bytes each (12)
												// alpha 4bytes (4)
												// checksum 1
												// Total 33

#define PREAMBLE 0x65							// パケットの先頭を示すデータ（プリアンブル）

#define SAMPLING 100							// サンプリング周波数 [Hz]
#define MAXDURATION 3600						// データの記録は１時間（３６００秒）までとする
#define MAXDATASIZE (SAMPLING*DATASORT*MAXDURATION)

float databuf[DATASORT+1][MAXDATASIZE];			// グローバル変数配列にセンサデータを格納する
int errcnt;										// 通信エラーの回数を数える変数

int xaxis;										// グラフ描画時の時間幅　（単位：秒）
int yaxis;										// グラブ描画を行う軸の番号（1～DATASORT）
int gain;										// グラフの縦軸の拡大倍率

static void CloseComPort(void)
// オープンしているシリアルポートをクローズする
{
	if( hCOMnd == INVALID_HANDLE_VALUE)
		return;
	CloseHandle( hCOMnd);
	hCOMnd = INVALID_HANDLE_VALUE;
}

static DWORD OpenComPort(int port)
// portにて指定した番号のシリアルポートをオープンする（非同期モード）
{
	CString ComPortNum;
	COMMPROP	myComProp;
	DCB	myDCB;
	COMSTAT	myComStat;

	// 非同期ＩＯモードなのでタイムアウトは無効
//	_COMMTIMEOUTS myTimeOut;

	if( (port < 0) || (port > MaxComPort))
		return -1;
	if( port < 10){
		ComPortNum.Format(_T("COM%d"), port);
	}else{
		ComPortNum.Format(_T("\\\\.\\COM%d"), port);	// Bill Gates' Magic ...
	}

	ZeroMemory( &rovl, sizeof(rovl));
	ZeroMemory( &wovl, sizeof(wovl));
	rovl.Offset = 0;
	wovl.Offset = 0;
	rovl.OffsetHigh = 0;
	wovl.OffsetHigh = 0;
	rovl.hEvent = NULL;
	wovl.hEvent = NULL;

	hCOMnd = CreateFile( (LPCTSTR)ComPortNum, GENERIC_READ | GENERIC_WRITE, 0, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
	// COMポートをオーバラップドモード（非同期通信モード）にてオープンしている

	if( hCOMnd == INVALID_HANDLE_VALUE){
		return -1;
	}

	GetCommProperties( hCOMnd, &myComProp);
	GetCommState(hCOMnd, &myDCB);
//	if( myComProp.dwSettableBaud & BAUD_128000)
//		myDCB.BaudRate = CBR_128000;
//	else
	myDCB.BaudRate = CBR_115200;		// 115.2KbpsモードをWindowsAPIは正しく認識しない
//	myDCB.BaudRate = CBR_9600;
	myDCB.fDtrControl = DTR_CONTROL_DISABLE;
	myDCB.Parity = NOPARITY;
	myDCB.ByteSize = 8;
	myDCB.StopBits = ONESTOPBIT;
	myDCB.fDsrSensitivity = FALSE;
	SetCommState(hCOMnd, &myDCB);
	DWORD	d;

	d = myComProp.dwMaxBaud;

	DWORD	myErrorMask;
	char	rbuf[32];
	DWORD	length;

// オーバーラップドモードでは、タイムアウト値は意味をなさない

//	GetCommTimeouts( hCOMnd, &myTimeOut);
//	myTimeOut.ReadTotalTimeoutConstant = 10;	// 10 msec
//	myTimeOut.ReadIntervalTimeout = 200;	// 200 msec
//	SetCommTimeouts( hCOMnd, &myTimeOut);
//	GetCommTimeouts( hCOMnd, &myTimeOut);
//	ReadTimeOut = (int)myTimeOut.ReadTotalTimeoutConstant;

	ClearCommError( hCOMnd, &myErrorMask, &myComStat);

	if( myComStat.cbInQue > 0){
		int	cnt;
		cnt = (int)myComStat.cbInQue;
		for( int i = 0; i < cnt; i++){
// Synchronous IO
//			ReadFile( hCOMnd, rbuf, 1, &length, NULL);		
//

// オーバーラップドモードで、同期通信的なことを行うためのパッチコード
			ReadFile( hCOMnd, rbuf, 1, &length, &rovl);
			while(1){
				if( HasOverlappedIoCompleted(&rovl)) break;
			}
		}
	}

	return d;
}


// コールバック関数によるシリアル通信状況の通知処理

int rcomp;

VOID CALLBACK FileIOCompletionRoutine( DWORD err, DWORD n, LPOVERLAPPED ovl)
{
	rcomp = n;			// 読み込んだバイト数をそのままグローバル変数に返す
}

// 無線パケットを受信するためのスレッド

#define RINGBUFSIZE 1024		// 受信用リングバッファのサイズ（バイト数）

unsigned __stdcall serialchk( VOID * dummy)
{
	DWORD myErrorMask;						// ClearCommError を使用するための変数
	COMSTAT	 myComStat;						// 受信バッファのデータバイト数を取得するために使用する
	unsigned char buf[RINGBUFSIZE];			// 無線パケットを受信するための一時バッファ

	unsigned char ringbuffer[RINGBUFSIZE];	// パケット解析用リングバッファ
	int rpos, wpos, rlen;					// リングバッファ用ポインタ（read, write)、データバイト数
	int rpos_tmp;							// データ解析用read位置ポインタ
	int rest;								// ClearCommErrorにて得られるデータバイト数を記録する

	int seq, expected_seq;					// モーションセンサから受信するシーケンス番号（８ビット）
											// と、その期待値（エラーがなければ受信する値）

	unsigned char packet[PACKETSIZE];		// パケット解析用バッファ （１パケット分）
	int i, chksum, tmp;
	float *ftmp;

	expected_seq = 0;						// 最初に受信されるべきSEQの値はゼロ
	rpos = wpos = 0;						// リングバッファの読み出し及び書き込みポインタを初期化
	rlen = 0;								// リングバッファに残っている有効なデータ数（バイト）

	while(rf){
		rcomp = 0;					// FileIOCompletionRoutineが返す受信バイト数をクリアしておく
		// まずは無線パケットの先頭の１バイトを読み出しに行く
		ReadFileEx( hCOMnd, buf, 1, &rovl, FileIOCompletionRoutine);

		while(1){
			SleepEx( 100, TRUE);	// 最大で100ミリ秒 = 0.1秒の間スリープするが、その間に
			if(rcomp == 1) break;	// I/O完了コールバック関数が実行されるとスリープを解除する

			if(!rf){				// 外部プログラムからスレッドの停止を指示された時の処理
				CancelIo(hCOMnd);	// 発行済みのReadFileEx操作を取り消しておく
				break;
			}
									// データが送られてこない時間帯では、受信スレッド内のこの部分
									// が延々と処理されているが、大半がSleepExの実行に費やされる
									// ことで、システムに与える負荷を軽減している。
		}

		if(!rf) break;				// 外部プログラムからスレッドの停止を指示された

		ringbuffer[wpos] = buf[0];	// 最初の１バイトが受信された
		wpos++;						// リングバッファの書き込み位置ポインタを更新
		wpos = (wpos >= RINGBUFSIZE)?0:wpos;	// １周していたらポインタをゼロに戻す（なのでRING）
		rlen++;						// リングバッファ内の有効なデータ数を＋１する

		ClearCommError( hCOMnd, &myErrorMask, &myComStat);	// 受信バッファの状況を調べるＡＰＩ

		rest = myComStat.cbInQue;	// 受信バッファに入っているデータバイト数が得られる

		if(rest == 0) continue;		// 何も入っていなかったので次の１バイトを待ちにいく

		rcomp = 0;
		ReadFileEx( hCOMnd, buf, rest, &rovl, FileIOCompletionRoutine);
									// 受信バッファに入っているデータを読み出す
									// 原理的にはrestで示される数のデータを受信することができるが、
									// 万一に備えてデータが不足してしまった時の処理を考える。

		SleepEx(16, TRUE);			// Windowsにおけるシリアルポートの標準レイテンシ（16msec）だけ待つ
		if( rcomp != rest){
			CancelIo(hCOMnd);		// ClearCommErrorで取得したデータバイト数に満たない
		}							// データしか受信されなかったので、先に発行したReadFileEx
									// をキャンセルしている

		i = 0;
		while(rcomp > 0){			// rcompには読み出すことのできたデータのバイト数が入っている
			ringbuffer[wpos] = buf[i];	// リングバッファに受信データを転送する
			wpos++;
			wpos = (wpos >= RINGBUFSIZE)?0:wpos;
			rlen++;
			i++;
			rcomp--;
		}

		// ここからパケット解析に入る

		while(1){					// 有効なパケットである限り解析を継続する
			while( rlen > 0){
				if( ringbuffer[rpos] == PREAMBLE) break;
				rpos++;								// 先頭がPREAMBLEではなかった
				rpos = (rpos >= RINGBUFSIZE)?0:rpos;
				rlen--;								// 有効なデータ数を１つ減らして再度先頭を調べる
			}

			if(rlen < PACKETSIZE) break;	// 解析に必要なデータバイト数に達していなかったので
											// 最初の１バイトを待つ処理に戻る

			rpos_tmp = rpos;	// リングバッファを検証するための仮ポインタ
								// まだリングバッファ上にあるデータが有効であると分かったわけではない

			for( i = 0, chksum = 0; i < (PACKETSIZE-1); i++){
				packet[i] = ringbuffer[rpos_tmp];	// とりあえず解析用バッファにデータを整列させる
				chksum += packet[i];
				rpos_tmp++;
				rpos_tmp = (rpos_tmp >= RINGBUFSIZE)?0:rpos_tmp;
			}

			if( (chksum & 0xff) != ringbuffer[rpos_tmp]){	// チェックサムエラーなのでパケットは無効
				rpos++;										// 先頭の１バイトを放棄する
				rpos = (rpos >= RINGBUFSIZE)?0:rpos;
				rlen--;
				continue;	// 次のPREAMBLEを探しにいく
			}

			// PREAMBLE、チェックサムの値が正しいPACKETSIZ長のデータがpacket[]に入っている

			seq = packet[1];
			databuf[0][datasize] = (float)seq;		// seq は８ビットにて0～255の間を1ずつカウントアップしていく数値

			if(seq != expected_seq){		// 受信されたseqが、１つ前のseqに+1したものであることをチェックする
				errcnt += (seq + 256 - expected_seq)%256;	// パケットエラー数を更新する
			}
			expected_seq = (seq + 1)%256;					// 次のseqの期待値をexpected_seqに入れる

			for (i = 0; i < 7; i++) {
				// acc x 3, gyro x 3, temperature x 1
				tmp = (packet[i * 2 + 2] << 8) & 0xff00 | packet[i * 2 + 3];
				databuf[i + 1][datasize] = (float)((tmp > 32767) ? (tmp - 65536) : tmp);
			}

			for (i = 0; i < 4; i++) {
				ftmp = (float *)&packet[i * 4 + 16];
				databuf[i + 8][datasize] = *ftmp;
			}
			
			datasize++;
			datasize = (datasize >= MAXDATASIZE)?(MAXDATASIZE-1):datasize;	// データ数が限界に到達した際の処理

			if( datasize%1 == 0){								// 10サンプルに1回の割合でメッセージを発生させる
				PostMessage( hDlg, WM_RCV, (WPARAM)1, NULL);	// 100Hzサンプリングであれば0.1秒に1回画面が更新される
			}													// PostMessage()自体では処理時間は極めて短い

			rpos = (rpos + PACKETSIZE)%RINGBUFSIZE;			// 正しく読み出せたデータをリングバッファから除去する
			rlen -= PACKETSIZE;								// バッファの残り容量を更新する
		}
	}
	_endthreadex(0);	// スレッドを消滅させる
	return 0;
}

// アプリケーションのバージョン情報に使われる CAboutDlg ダイアログ

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// ダイアログ データ
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV サポート

// 実装
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()


// CPQ_GRAPHDlg ダイアログ


CPQ_GRAPHDlg::CPQ_GRAPHDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CPQ_GRAPHDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CPQ_GRAPHDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	// ------------------------------Font------------------------------
	DDX_Control(pDX, IDC_EDIT1, msgED1);
	// ------------------------------General------------------------------
	DDX_Control(pDX, IDC_EDIT2, msgED2);
	// ------------------------------DataReciver------------------------------
	DDX_Control(pDX, IDC_EDIT3, msgED3);
	// ------------------------------OpneGL-----------------------------
	DDX_Control(pDX, IDC_GRAPH, m_glView);
}

BEGIN_MESSAGE_MAP(CPQ_GRAPHDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_BUTTON1, &CPQ_GRAPHDlg::OnBnClickedButton1)
	ON_BN_CLICKED(IDC_BUTTON2, &CPQ_GRAPHDlg::OnBnClickedButton2)
	ON_BN_CLICKED(IDC_BUTTON3, &CPQ_GRAPHDlg::OnBnClickedButton3)
	ON_MESSAGE( WM_RCV, &CPQ_GRAPHDlg::OnMessageRCV)
	ON_WM_DESTROY()
END_MESSAGE_MAP()
// メッセージマップの最後のON_MESSAGEの行は手入力している


// CPQ_GRAPHDlg メッセージ ハンドラ

BOOL CPQ_GRAPHDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// "バージョン情報..." メニューをシステム メニューに追加します。

	hDlg = this->m_hWnd;		// このダイアログへのハンドルを取得する
								// このコードは手入力している

	// IDM_ABOUTBOX は、システム コマンドの範囲内になければなりません。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// このダイアログのアイコンを設定します。アプリケーションのメイン ウィンドウがダイアログでない場合、
	//  Framework は、この設定を自動的に行います。
	SetIcon(m_hIcon, TRUE);			// 大きいアイコンの設定
	SetIcon(m_hIcon, FALSE);		// 小さいアイコンの設定

	// TODO: 初期化をここに追加します。

	// 初期化コードを手入力にて追加している
	rf = 0;			// 無線パケット受信スレッドは停止
	datasize = 0;	// モーションデータの数を０に初期化


	// 設定ファイルを読み込み、シリアル通信用ポート番号とファイル保存先フォルダを設定する
	CStdioFile pFile;
	CString buf;
	char pbuf[256], dirbuf[_MAX_PATH];
	char pathname[256];
	int i, comport, dirlen;

	for( i = 0; i < 256; i++){
		pbuf[i] = pathname[i] = 0x00;	// 安全のためNULLで埋めておく
	}

	if(!pFile.Open( ParamFileName, CFile::modeRead)){
		msgED3.SetWindowTextW(_T("Parameter file not found..."));
		RScomport = DefaultPort;
		dirlen = GetCurrentDirectory( _MAX_PATH, (LPTSTR)dirbuf);
		if( dirlen != 0){
		CurrentDir.Format(_T("%s\\"), dirbuf);
		}
	}else{
		pFile.ReadString((LPTSTR)pbuf, 32);
		dirlen = GetCurrentDirectory( _MAX_PATH, (LPTSTR)dirbuf);

		if( dirlen != 0){
		CurrentDir.Format(_T("%s\\"), dirbuf);
		}

		pbuf[1] = pbuf[2];	// Unicodeに対応するためのパッチコード
		pbuf[2] = 0x00;

		comport = atoi(pbuf);	// ポート番号をテキストファイルから取得している

		CString cpath;
		pFile.ReadString(cpath);

		if( cpath.GetLength() != 0){
			DefaultDir = cpath;
		}

		if( (comport > 0) && (comport <= MaxComPort)){
			RScomport = comport;
			buf.Format(_T("Parameter File Found. RS channel %d"), comport);
			msgED3.SetWindowTextW(buf);
		}else{
			buf.Format(_T("Invalid Comport number! [COM1-COM%d are valid]"), MaxComPort);
			msgED3.SetWindowTextW(buf);
			RScomport = DefaultPort;	// default port number
		}
		pFile.Close();
	}

	// ------------------------------Font------------------------------
	fontFlag = false;
	setFontStyle();

	// ------------------------------OpneGL------------------------------
	glFlag = false;
	// ------------------------------MIDI------------------------------
	midiOutOpen(&hMIDI, MIDI_MAPPER, 0, 0, CALLBACK_THREAD); // ここでMIDIデバイスを初期化することで遅延を軽減する

	return TRUE;  // フォーカスをコントロールに設定した場合を除き、TRUE を返します。
}

void CPQ_GRAPHDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// ダイアログに最小化ボタンを追加する場合、アイコンを描画するための
//  下のコードが必要です。ドキュメント/ビュー モデルを使う MFC アプリケーションの場合、
//  これは、Framework によって自動的に設定されます。

void CPQ_GRAPHDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 描画のデバイス コンテキスト

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// クライアントの四角形領域内の中央
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// アイコンの描画
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// ユーザーが最小化したウィンドウをドラッグしているときに表示するカーソルを取得するために、
//  システムがこの関数を呼び出します。
HCURSOR CPQ_GRAPHDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

int clipping(int y, int ymin, int ymax)
{
	if (ymin >= ymax) return 0; // fatal error!

	y = (y < ymin) ? ymin : y;
	y = (y > ymax) ? ymax : y;
	return y;
}

LRESULT CPQ_GRAPHDlg::OnMessageRCV( WPARAM wParam, LPARAM lParam)
// グラフを描画するためのイベント駆動型コード（システムからコールバックされる）
{
	// -------------------------------General------------------------------
	// ジャイロ
	double gx = databuf[4][datasize - 1];
	double gy = databuf[5][datasize - 1];
	double gz = databuf[6][datasize - 1];

	// 姿勢角
	double ex = databuf[8][datasize - 1];
	double ey = databuf[9][datasize - 1];
	double ez = databuf[10][datasize - 1];

	CString gs, es;
	gs.Format(_T("gx:%lf %lfgy:%lf %lfgz:%lf\r\n"), gx, gy, gz);
	es.Format(_T("ex:%lf %lfey:%lf %lfez:%lf\r\n"), ex, ey, ez);
	msgED3.SetWindowTextW(gs + es);

	// -------------------------------MIDI------------------------------
	std::thread midiThread(&CPQ_GRAPHDlg::midiCallback, this, gx, gy, ey);
	midiThread.detach();
	// -------------------------------FONT------------------------------
	fontFlag = true;
	std::thread fontThread(&CPQ_GRAPHDlg::fontCallback, this, Sound::getInfo());
	fontThread.detach();
	fontFlag = false;
	// -------------------------------OpenGL------------------------------
	Viewer::update(ex, ey, ez);

	return TRUE;
}




void CPQ_GRAPHDlg::OnBnClickedButton1()
// STARTボタンが押されると、ワイヤレスデータの受信スレッドを起動する
{
	// START

	DWORD d;

	if(rf){
		msgED3.SetWindowTextW(_T("Thread is already running..."));
		return;
	}

	d = OpenComPort(RScomport);

	if( d < 0){
		msgED3.SetWindowTextW(_T("can't initialize COM port"));
		Invalidate();
		UpdateWindow();
		return;
	}

	rf = 1;
	errcnt = 0;

	unsigned int tid;
	serialh = (HANDLE)_beginthreadex( NULL, 0, serialchk, NULL, 0, &tid);

	if(serialh != NULL){
		msgED3.SetWindowTextW(_T("Start Recording"));
		// ------------------------------OpenGl------------------------------
		glFlag = true;
		std::thread glThread(&CPQ_GRAPHDlg::glCallback, this);
		glThread.detach();
		// ------------------------------OpenGl------------------------------
	}else{
		rf = 0;
		CloseComPort();
		msgED3.SetWindowTextW(_T("Thread is not running..."));
	}
}

void CPQ_GRAPHDlg::OnBnClickedButton2()
// STOPボタンが押されると、ワイヤレスデータの受信スレッドを停止させてオブジェクトを破棄する
{
	// STOP
	if(rf){
		rf = 0;

		DWORD dwExitCode;
		while(1){
			GetExitCodeThread( serialh, &dwExitCode);
			if( dwExitCode != STILL_ACTIVE) break;
		}
		CloseHandle(serialh);
		serialh = NULL;
		CloseComPort();
		msgED3.SetWindowTextW(_T("Stop Recording"));
		// ------------------------------OpenGl------------------------------
		glFlag = false;
		// ------------------------------OpenGl------------------------------
	}else{
		msgED3.SetWindowTextW(_T("Recording is not running..."));
	}
}

void CPQ_GRAPHDlg::OnBnClickedButton3()
{
	// FILE OUT

	DWORD		dwFlags;
	LPCTSTR		lpszFilter = _T("CSV File(*.csv)|*.csv|");
	CString		fn, pathn;
	int			i, j, k;
	CString		rbuf;
	CString		writebuffer;

	if(rf){
		msgED3.SetWindowTextW(_T("Recording thread is still running..."));
		return;
	}

	if(datasize == 0){
		msgED3.SetWindowTextW(_T("There is no data..."));	
		return;
	}

	dwFlags = OFN_OVERWRITEPROMPT | OFN_CREATEPROMPT;

	CFileDialog myDLG( FALSE, _T("csv"), NULL, dwFlags, lpszFilter, NULL);
	myDLG.m_ofn.lpstrInitialDir = DefaultDir;


	if( myDLG.DoModal() != IDOK) return;


	CStdioFile fout(myDLG.GetPathName(), CFile::modeWrite | CFile::typeText | CFile::modeCreate);

	pathn = myDLG.GetPathName();
	fn = myDLG.GetFileName();
	j = pathn.GetLength();
	k = fn.GetLength();
	DefaultDir = pathn.Left(j-k);

	msgED3.SetWindowTextW(_T("Writing the Data File..."));

	Invalidate();
	UpdateWindow();

	// RAW DATA FORMAT
	for( i = 0; i < datasize; i++){
		writebuffer.Format(_T("%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\n"), 
			databuf[0][i],								 // sequence counter
			databuf[1][i], databuf[2][i], databuf[3][i], // ax, ay, az
			databuf[4][i], databuf[5][i], databuf[6][i], // wx, wy, wz;
			databuf[7][i],							     // temperature
			databuf[8][i], databuf[9][i], databuf[10][i], // e4x, e4y, e4z;
			databuf[11][i]								 // confidence factor alpha
		);
		fout.WriteString((LPCTSTR)writebuffer);
	}

	fout.Close();

	datasize = 0;

	msgED3.SetWindowTextW(_T("Motion Data File Writing Succeeded"));
}

void CPQ_GRAPHDlg::OnDestroy()
{
	CDialog::OnDestroy();
	// TODO: ここにメッセージ ハンドラー コードを追加します。

	// ------------------------------Font------------------------------
	fontFlag = false;
	m_newFont->DeleteObject(); // m_newFont内のオブジェクトを破棄
	delete m_newFont; // ポインタを破棄
	// ------------------------------OpneGL------------------------------
	glFlag = false;
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(m_GLRC);
	delete m_pDC;
	// ------------------------------MIDI------------------------------
	midiOutReset(hMIDI); // MIDIデバイスをリセットする
	midiOutClose(hMIDI); // MIDIデバイスを終了する
}


/**********************************************

以下、各処理に特化した関数を手動にて記述

***********************************************/

BOOL CPQ_GRAPHDlg::SetUpPixelFormat(HDC hdc)
{
	// TODO: ここに実装コードを追加します.
	PIXELFORMATDESCRIPTOR pfd = {
	sizeof(PIXELFORMATDESCRIPTOR),  // PFD のサイズ
	1,                              // バージョン
	PFD_DRAW_TO_WINDOW |            // ウィンドウに描画する
	PFD_SUPPORT_OPENGL |            // OpenGL を使う
	PFD_DOUBLEBUFFER,               // ダブルバッファリングする
	PFD_TYPE_RGBA,                  // RGBA モード
	24,                             // カラーバッファは 24 ビット
	0, 0, 0, 0, 0, 0,               //  (各チャンネルのビット数は指定しない) 
	0, 0,                           // アルファバッファは使わない
	0, 0, 0, 0, 0,                  // アキュムレーションバッファは使わない
	32,                             // デプスバッファは 32 ビット
	0,                              // ステンシルバッファは使わない
	0,                              // 補助バッファは使わない
	PFD_MAIN_PLANE,                 // メインレイヤー
	0,                              //  (予約) 
	0, 0, 0                         // レイヤーマスクは無視する
	};

	int pf = ChoosePixelFormat(hdc, &pfd);
	if (pf != 0) return SetPixelFormat(hdc, pf, &pfd);
	return FALSE;
}

void CPQ_GRAPHDlg::fontCallback(CString soundInfo)
{
	while (fontFlag) msgED1.SetWindowTextW(soundInfo);
}

void CPQ_GRAPHDlg::setFontStyle()
{
	CFont *curFont;	// CFontのポインタを準備
	curFont = msgED1.GetFont();	// Edit Controlから現在のフォントを取得
	LOGFONTW mylf; // 一時的なマルチバイト文字列用のフォント変数を準備
	curFont->GetLogFont(&mylf); // 現在のフォントをmylfに反映
	mylf.lfHeight = 30; //文字の高さを変更
	mylf.lfWidth = 15; // 文字の幅を変更
	m_newFont = new CFont; // フォントを扱うメンバ変数にアドレスを割り当てる
	m_newFont->CreateFontIndirectW(&mylf); // 論理フォントをm_newFontに格納
	msgED1.SetFont(m_newFont); // コントローラ変数にフォントをセット
}

void CPQ_GRAPHDlg::glCallback()
{
	// TODO: ここに実装コードを追加します.
	glSetup();
	while (glFlag) {
		Viewer::draw();
		SwapBuffers(m_pDC->m_hDC);
	}
}

void CPQ_GRAPHDlg::glSetup()
{
	m_pDC = new CClientDC(&m_glView);

	if (SetUpPixelFormat(m_pDC->m_hDC) != FALSE) {
		m_GLRC = wglCreateContext(m_pDC->m_hDC);
		wglMakeCurrent(m_pDC->m_hDC, m_GLRC);

		CRect rc; // 描画領域を保存するための変数を用意
		m_glView.GetClientRect(&rc);
		GLint width = rc.Width();
		GLint height = rc.Height();

		Viewer::setup(width, height);
	}
}

void CPQ_GRAPHDlg::midiCallback(const double gx, const double gy, const double ey)
{
	Sound::calcToDeg(ey);
	Sound::setScaleIndex();

	if (Sound::count < 3) {
		if (gy > 10000.0) {
			Sound::border[Sound::count] = gy;
			Sound::local_flag = true;
		}
		else if (gy < 0 && Sound::local_flag) {
			Sound::count += 1;
			Sound::local_flag = false;
		}
	}
	else if (Sound::count == 3) {
		Sound::gyUpperLimit = (Sound::border[0] + Sound::border[1] + Sound::border[2]) / Sound::count -  3000.0;
		Sound::count += 1;
	}
	else {
		if (gy < -10000) {
			Sound::soundFlag = false;
		}
		Sound::emit(gx, gy);
	}
}
