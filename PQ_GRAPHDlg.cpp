// PQ_GRAPHDlg.cpp

#include "stdafx.h"
#include "PQ_GRAPH.h"
#include "PQ_GRAPHDlg.h"

// ------------------------------OpneGL------------------------------
#include "Viewer.h"
// ------------------------------MIDI------------------------------
#include "Sound.h"
static HMIDIOUT hMIDI;											// MIDI Handler


#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define WM_RCV (WM_USER+1)

#define MaxComPort	99											// Upper limit No. that recognizable com port
#define DefaultPort 4

int RScomport = DefaultPort;									// default COM port #4

																			// Contain below Infomation in text file(PQ_GRAPH.txt) before run
																			// Serial Communication Port No.
																			// Destination directory name to write data
CString ParamFileName = _T("PQ_GRAPH.txt");		



CString DefaultDir = _T("D:\\");								// Default Directory when wittring data
CString CurrentDir = _T("D:\\");							// Rewrite Directory in Parameter File 
HANDLE	hCOMnd = INVALID_HANDLE_VALUE;		// File Handle for Async Serial Communication
OVERLAPPED rovl, wovl;											// Overlaped Struct for Async Serial Communication

HWND hDlg;															// Variable that contain Window Handle, Dialog itself 
HANDLE serialh;														// Thread Handle for Communication

int rf;																	// Running State Controler of Communication Thread
int datasize;															// Size of Sample data corectly Recieved

#define DATASORT 11											// Data types
// 16-b int Ax, Ay, Az, Gx, Gy, Gz, Temperature,
// float e4x, e4y, e4z
// float alpha

// databuf[SORT][i];
// 1,2,3		-> ax, ay, az 
// 4,5,6		-> gx, gy, gz 
// 7			-> temperature 
// 8,9,10	-> e4x, e4y, e4z
// 11			-> alpha


#define PACKETSIZE 33	// Byte size per packet
// PREAMBLE																1
// SEQ																		1
// Ax, Ay, Az, Gx, Gy, Gz, Temperature (2bytes each)	14
// e4x, e4y, e4z (4bytes each)										12
// alpha (4bytes)														4
// checksum																1
// Total																		33


#define PREAMBLE 0x65																// Indicate Head of Packet Data（PREAMBLE）

#define SAMPLING 100																// Samping frequency [Hz]
#define MAXDURATION 3600														// Recoding Time Limit: 1hour
#define MAXDATASIZE (SAMPLING*DATASORT*MAXDURATION)

float databuf[DATASORT+1][MAXDATASIZE];								// Store Sensor Data to Array Type Variable(Global, databuf)
int errcnt;																					// Error Counter

/*
* Close Opened Serial Port
*/
static void CloseComPort(void)
{
	if( hCOMnd == INVALID_HANDLE_VALUE) return;
	CloseHandle( hCOMnd);
	hCOMnd = INVALID_HANDLE_VALUE;
}

/*
*  Open Serial Port that Selected in port (Async Mode)
*/
static DWORD OpenComPort(int port)
{
	CString ComPortNum;
	COMMPROP	myComProp;
	DCB	myDCB;
	COMSTAT	myComStat;

// Async I/O Mode, so TimeOut is N/A
//	_COMMTIMEOUTS myTimeOut;

	if( (port < 0) || (port > MaxComPort)) return -1;
	if( port < 10)	ComPortNum.Format(_T("COM%d"), port);
	else ComPortNum.Format(_T("\\\\.\\COM%d"), port);	// Bill Gates' Magic ...

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
	// Open COM Port with Overlaped Mode(Async Communication Mode)

	if( hCOMnd == INVALID_HANDLE_VALUE) return -1;

	GetCommProperties( hCOMnd, &myComProp);
	GetCommState(hCOMnd, &myDCB);
//	if( myComProp.dwSettableBaud & BAUD_128000)
//		myDCB.BaudRate = CBR_128000;
//	else
	myDCB.BaudRate = CBR_115200;	// Windows API disable to recognize 115.3Kbps Mode Correctly
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

// TimeOut Value is N/A when Overlaped Mode

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

// Patch Code that to do Sync Communication like in Overlaped Mode
			ReadFile( hCOMnd, rbuf, 1, &length, &rovl);
			while(1){
				if( HasOverlappedIoCompleted(&rovl)) break;
			}
		}
	}

	return d;
}


/*
* Notify Serial Communication State by Callback Function
*/

int rcomp;

VOID CALLBACK FileIOCompletionRoutine( DWORD err, DWORD n, LPOVERLAPPED ovl)
{
	rcomp = n;																			// Return read Byte Size as it is to Global
}

																								// Thread that to recieve Wireless Communication Packet

#define RINGBUFSIZE 1024														// Ring Buffer Size (for Recieving) (byte)

unsigned __stdcall serialchk( VOID * dummy)
{
	DWORD myErrorMask;															// Variable for ClearCommError 
	COMSTAT	 myComStat;															// Use for Getting Recieved Buffer Data Byte Size
	unsigned char buf[RINGBUFSIZE];											// temporal buffer for Recieving Wireless Packet

	unsigned char ringbuffer[RINGBUFSIZE];									// Ring Buffer for Analising Packet
	int rpos, wpos, rlen;																// Pointer for Ring buffer (read, write), Data Byte size
	int rpos_tmp;																			// Read Position Pointer for Data Analising
	int rest;																					// Record data byte size that can get in ClearCommError

	int seq, expected_seq;															// Seaquence No. that Received from Motion Sensor (8 bit)
																								// And the expectational value (Recieved Value when No error）

	unsigned char packet[PACKETSIZE];										// Buffer for Packet Analising (Equivalent to 1 packet)
	int i, chksum, tmp;
	float *ftmp;

	expected_seq = 0;																	// Shoud be recieved SEQ value = 0
	rpos = wpos = 0;																	// Initialize Read and Write Pointer of Ring Buffer
	rlen = 0;																				// Rest of Ring Buffer Data(byte)

	while(rf){
		rcomp = 0;																		// Clear Recived Data Byte Size (File I/O CompletionRoutin return)
																								// First, Read head of Wireless Communication Packet.
		ReadFileEx( hCOMnd, buf, 1, &rovl, FileIOCompletionRoutine);

		while(1){
			SleepEx( 100, TRUE);														// Sleep(Max 100ms)
			if(rcomp == 1) break;													// Unsleep when run I/O Complete Callback Function while Sleep

			if(!rf){																			// Processing when Out ot this programe command to stop thread
				CancelIo(hCOMnd);													// Cancel pulished ReadFileEx Operation
				break;
			}
																								// When don't Recive data, this part run endlessly. 
																								//But most part of process use to run SleepEx, reduce system load.
		}

		if(!rf) break;																		// Out ot this programe command to stop thread

		ringbuffer[wpos] = buf[0];													// Firt byte Recived
		wpos++;																			// Update write position pointer of ring buffer 
		wpos = (wpos >= RINGBUFSIZE)?0:wpos;							// Set pointer 0 when 1 laped（Because of ring buffer)
		rlen++;																				// increase(+1) valid data size in ring buffer
		ClearCommError( hCOMnd, &myErrorMask, &myComStat);	// API that check state of Recieve buffer

		rest = myComStat.cbInQue;												// Get data byte size in Recieved buffer

		if(rest == 0) continue;														// Recieved buffer was empty, so wait next byte.

		rcomp = 0;
		ReadFileEx( hCOMnd, buf, rest, &rovl, FileIOCompletionRoutine);
																								// Read Data in Recived buffer
																								// able to recieve num of data that rest indicates in principle.
																								// But ready in case shortage data, we prepare other process.

		SleepEx(16, TRUE);															// Wait Staradard latency (16ms) on Windows Serial Port
		if (rcomp != rest) {
			CancelIo(hCOMnd);														// Because don't reach data byte size on ClearCommError, Cancel ReadFileEx
		}
		i = 0;
		while(rcomp > 0){																// rcomp has num of data bytes that able to read
			ringbuffer[wpos] = buf[i];												// tranfer Recieved data to ring buffer
			wpos++;
			wpos = (wpos >= RINGBUFSIZE)?0:wpos;
			rlen++;
			i++;
			rcomp--;
		}

		// Begin Packet Analysing
		while(1){																			// Cotinue Analysing while available packet 
			while( rlen > 0){
				if( ringbuffer[rpos] == PREAMBLE) break;
				rpos++;																	// Head is not PREAMBLE
				rpos = (rpos >= RINGBUFSIZE)?0:rpos;
				rlen--;																		// increase Available data size and check head again
			}

			if(rlen < PACKETSIZE) break;											// Becase din't reach needed num of data bytes for analysing,
																								// go back to Process that wait fisrt 1 byte 

			rpos_tmp = rpos;	
																								// tmp Pointer for checking ring buffer
																								// Don't  find data on ring buffer are valid yet

			for( i = 0, chksum = 0; i < (PACKETSIZE-1); i++){
				packet[i] = ringbuffer[rpos_tmp];								// For now, align data on buffer
				chksum += packet[i];
				rpos_tmp++;
				rpos_tmp = (rpos_tmp >= RINGBUFSIZE)?0:rpos_tmp;
			}

			if( (chksum & 0xff) != ringbuffer[rpos_tmp]){					// Because Check-sum-Error, Packet is Invalid
				rpos++;																	// release head(1byte) 
				rpos = (rpos >= RINGBUFSIZE)?0:rpos;
				rlen--;
				continue;																	// Search next PREAMBLE
			}

																								// packet[] has data that length of data is PACKETSIZE
																								// And it hascorrectlry PREAMBLE and Check-sum

			seq = packet[1];
			databuf[0][datasize] = (float)seq;									// seq is variable of countup(+1) between 0-255 (8bit)

			if(seq != expected_seq){												// Check recieved seq == previous seq +1 ?
				errcnt += (seq + 256 - expected_seq)%256;				// Update Packet Error counter
			}
			expected_seq = (seq + 1)%256;										// Put next expected seq in expected_seq

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
			datasize = (datasize >= MAXDATASIZE)?(MAXDATASIZE-1):datasize;		// Process when Data size reach LIMIT

			// Post Message(100mesage/sec)
			// PostMessage itself, Processing time is very short
			if( datasize%1 == 0) PostMessage( hDlg, WM_RCV, (WPARAM)1, NULL);	

			rpos = (rpos + PACKETSIZE)%RINGBUFSIZE;											// leave correclty read data from ring buffer
			rlen -= PACKETSIZE;																				// Upadate rest of buffer volume
		}
	}
	_endthreadex(0);																							// Destroy Thread 
	return 0;
}


/*
* CAboutDlg Dialog : Use for Application Version Information
*/
class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV Support 

// Implemntation
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


// CPQ_GRAPHDlg Dialog 


CPQ_GRAPHDlg::CPQ_GRAPHDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CPQ_GRAPHDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CPQ_GRAPHDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	// ------------------------------Font--------------------------------------
	DDX_Control(pDX, IDC_EDIT1, msgED1);
	// ------------------------------General----------------------------------
	DDX_Control(pDX, IDC_EDIT2, msgED2);
	// ------------------------------DataReciver-----------------------------
	DDX_Control(pDX, IDC_EDIT3, msgED3);
	// ------------------------------OpneGL---------------------------------
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
	ON_MESSAGE( WM_RCV, &CPQ_GRAPHDlg::OnMessageRCV) // Non Automatically Generation
	ON_WM_DESTROY()
END_MESSAGE_MAP()


// CPQ_GRAPHDlg Message Handler

BOOL CPQ_GRAPHDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Version Information : Add Menu to System Menu

	hDlg = this->m_hWnd;		// Get this Dialog Handle. This code non Automatically generation

	// IDM_ABOUTBOX shoud be in range of system command
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

	// Set this dialog icon. Case tha Application Main Window is not Dialog. Framework set this setting automatically.
	SetIcon(m_hIcon, TRUE);			// Setting Large Icon 
	SetIcon(m_hIcon, FALSE);		// Setting Small Icon 

	// TODO: Add Initialize Code Below

	// Add Initialized Code by Manual input
	rf = 0; // Stop Wireless Packet Reciving Thread
	datasize = 0;	// Initialize datasize 0

	// Read Setting File. Set Serial Communication Port No. and Save Directory. 
	CStdioFile pFile;
	CString buf;
	char pbuf[256], dirbuf[_MAX_PATH];
	char pathname[256];
	int i, comport, dirlen;

	for( i = 0; i < 256; i++){
		pbuf[i] = pathname[i] = 0x00;	// Put NULL for safety
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

		pbuf[1] = pbuf[2];	//Unicode Support Patch Code
		pbuf[2] = 0x00;

		comport = atoi(pbuf);	 // Get Port No. from Textfile

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

	// ------------------------------Font----------------------------------
	fontFlag = false;
	setFontStyle();

	// ------------------------------OpneGL------------------------------
	glFlag = false;
	// ------------------------------MIDI---------------------------------
	midiOutOpen(&hMIDI, MIDI_MAPPER, 0, 0, CALLBACK_THREAD); // Open MIDI Device 

	return TRUE;  // Return TRUE, except set Focus Control
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


/*
* If add minimize button on dialog, need below code for rendering icon.
* In MFC Application that using Document/View Model case,  Set Automatically by Framework
*/
void CPQ_GRAPHDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // Rendering Device Context

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center of Client's Rect Area
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw Icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

/*
* To get cursor when druged window that minimaized by user, System Call this Function
*/
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

/*
************************************************************
* System Callback Fucntion
************************************************************
* Open MIDI and Font Thread
* Update MIDI, Font and OpenGL
*/
LRESULT CPQ_GRAPHDlg::OnMessageRCV( WPARAM wParam, LPARAM lParam)
{
	// -------------------------------General------------------------------
	//  Gyro
	double gx = databuf[4][datasize - 1];
	double gy = databuf[5][datasize - 1];
	double gz = databuf[6][datasize - 1];

	// Attitude angle
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


/*
* When Start Button is Pressed,  Open Wireless DataReciver thread 
*/
void CPQ_GRAPHDlg::OnBnClickedButton1()
{
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
	}else{
		rf = 0;
		CloseComPort();
		msgED3.SetWindowTextW(_T("Thread is not running..."));
	}
}


/*
* When Stop Button is Pressed, Stop Wireless DataReciver thread and Delete Objet
*/
void CPQ_GRAPHDlg::OnBnClickedButton2()
{
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
		// ------------------------------OpenGL------------------------------
		glFlag = false;
	}else{
		msgED3.SetWindowTextW(_T("Recording is not running..."));
	}
}


/*
* File Out 
*/
void CPQ_GRAPHDlg::OnBnClickedButton3()
{
	DWORD	dwFlags;
	LPCTSTR lpszFilter = _T("CSV File(*.csv)|*.csv|");
	CString fn, pathn;
	int	 i, j, k;
	CString	rbuf;
	CString	writebuffer;

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
			databuf[0][i],												// sequence counter
			databuf[1][i], databuf[2][i], databuf[3][i],	// ax, ay, az
			databuf[4][i], databuf[5][i], databuf[6][i],	// wx, wy, wz;
			databuf[7][i],												// temperature
			databuf[8][i], databuf[9][i], databuf[10][i], // e4x, e4y, e4z;
			databuf[11][i]											// confidence factor alpha
		);
		fout.WriteString((LPCTSTR)writebuffer);
	}

	fout.Close();

	datasize = 0;

	msgED3.SetWindowTextW(_T("Motion Data File Writing Succeeded"));
}


/*
* Run when Window is destroied
*/
void CPQ_GRAPHDlg::OnDestroy()
{
	CDialog::OnDestroy();
	// ------------------------------Font------------------------------
	fontFlag = false;
	m_newFont->DeleteObject(); // Destroy m_newFont Object
	delete m_newFont; // Delelte m_newFont 
	// ------------------------------OpneGL------------------------------
	glFlag = false;
	wglMakeCurrent(NULL, NULL); // Disable Windows OpenGL Handler
	wglDeleteContext(m_GLRC); // Delete Winddows OpenGL Context 
	delete m_pDC;
	// ------------------------------MIDI------------------------------
	midiOutReset(hMIDI); // Reset MIDI Device 
	midiOutClose(hMIDI); // Close MIDI Device
}


/**********************************************

Wrote specialized function below

***********************************************/

BOOL CPQ_GRAPHDlg::SetUpPixelFormat(HDC hdc)
{
	PIXELFORMATDESCRIPTOR pfd = {
	sizeof(PIXELFORMATDESCRIPTOR),		// PFD Size
	1,														//	Version 
	PFD_DRAW_TO_WINDOW |				// Render to Window
	PFD_SUPPORT_OPENGL |					// Use OpenGL
	PFD_DOUBLEBUFFER,							// Do Double Buffering 
	PFD_TYPE_RGBA,								// RGBA mode
	24,													// Color Buffer 24bit 
	0, 0, 0, 0, 0, 0,									// Don't specyfied bit (each channel)
	0, 0,													// Don't use Alpha buffer
	0, 0, 0, 0, 0,										// Don't user Accmulation Buffer
	32,													// Depth buffer 32bit 
	0,														// Don't use  stencil buffer
	0,														// Don't use sub buffer
	PFD_MAIN_PLANE,								// Main Layer
	0,														// Preserved 
	0, 0, 0												// ignore layer mask
	};

	int pf = ChoosePixelFormat(hdc, &pfd);
	if (pf != 0) return SetPixelFormat(hdc, pf, &pfd);
	return FALSE;
}


/*
* Font Callback Fucntion
*/
void CPQ_GRAPHDlg::fontCallback(CString soundInfo)
{
	while (fontFlag) msgED1.SetWindowTextW(soundInfo);
}


/*
* Set Font Style
*/
void CPQ_GRAPHDlg::setFontStyle()
{
	CFont *curFont;											// Prepare Pointer of CFont
	curFont = msgED1.GetFont();						// Get Current FOnt from Edit Control
	LOGFONTW mylf;										// Prepare variable for Multi Byte String
	curFont->GetLogFont(&mylf);						// Reflect Current FOnt to nylf
	mylf.lfHeight = 30;										// Change Font height
	mylf.lfWidth = 15;										// Change Font width 
	m_newFont = new CFont;							// Store Adress in Member variable that handling font
	m_newFont->CreateFontIndirectW(&mylf);	// Store Logical Font in m_newFont
	msgED1.SetFont(m_newFont);					// Set Font to Controller variablel
}


/*
* OpenGL Callback Function
*/
void CPQ_GRAPHDlg::glCallback()
{
	glSetup();
	while (glFlag) {
		Viewer::draw();
		SwapBuffers(m_pDC->m_hDC);
	}
}


/*
* Setup OpenGL Context & GLUT
*/
void CPQ_GRAPHDlg::glSetup()
{
	m_pDC = new CClientDC(&m_glView);

	if (SetUpPixelFormat(m_pDC->m_hDC) != FALSE) {
		m_GLRC = wglCreateContext(m_pDC->m_hDC);
		wglMakeCurrent(m_pDC->m_hDC, m_GLRC);

		CRect rc; // Get Rendering Context Size 
		m_glView.GetClientRect(&rc);
		GLint width = rc.Width();
		GLint height = rc.Height();

		Viewer::setup(width, height);
	}
}

/*
* Sound Callback Function
*/
void CPQ_GRAPHDlg::midiCallback(const double gx, const double gy, const double ey)
{
	Sound::calcToDeg(ey);
	Sound::setScaleIndex();

	/*
	* Check User twist speed
	*/
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
		if (gy < -10000) Sound::soundFlag = false;
		Sound::emit(gx, gy);
	}
}
