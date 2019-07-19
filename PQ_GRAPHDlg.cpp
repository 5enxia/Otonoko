// PQ_GRAPHDlg.cpp : �����t�@�C��
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
// process.h�̓}���`�X���b�h�`�o�h���g�p���邽�߂ɕK�v

#define WM_RCV (WM_USER+1)

#define MaxComPort	99							// ���ʂł���V���A���|�[�g�ԍ��̌��E
#define DefaultPort 4

int RScomport = DefaultPort;					// default COM port #4
CString ParamFileName = _T("PQ_GRAPH.txt");		// �e�L�X�g�t�@�C���ɂĉ��L�̏������Ă���
												// �V���A���ʐM�|�[�g�̔ԍ�
												// �f�[�^�t�@�C�������o����̃f�B���N�g����
												// ���e�L�X�g�G�f�B�^�ɂē��e��ύX�ł���

CString DefaultDir = _T("D:\\");				// �f�[�^�t�@�C���������o���ۂ̃f�t�H���g�f�B���N�g��
CString CurrentDir = _T("D:\\");				// Parameter File�̓��e�ŏ㏑�������

HANDLE	hCOMnd = INVALID_HANDLE_VALUE;			// �񓯊��V���A���ʐM�p�t�@�C���n���h��
OVERLAPPED rovl, wovl;							// �񓯊��V���A���ʐM�p�I�[�o�[���b�v�h�\����

HWND hDlg;										// �_�C�A���O���̂̃E�B���h�E�n���h�����i�[����ϐ�
HANDLE serialh;									// �ʐM�p�X���b�h�̃n���h��

int rf;											// �ʐM�p�X���b�h�̑��s��Ԑ���p�ϐ�
int datasize;									// ��������M�����f�[�^�̃T���v����

#define DATASORT 11								// �f�[�^�̎�ނ�11���
												// 16-b int Ax, Ay, Az, Gx, Gy, Gz, Temperature,
												// float e4x, e4y, e4z
												// float alpha

												// databuf[SORT][i];
												// 1,2,3 -> ax, ay, az
												// 4,5,6 -> gx, gy, gz
												// 7 -> temperature
												// 8, 9, 10 -> e4x, e4y, e4z
												// 11 -> alpha

#define PACKETSIZE 33							// ���C�����X�ʐM�ɂ�����P�p�P�b�g������̃f�[�^�o�C�g��
												// PREAMBLE 1
												// SEQ 1
												// Ax, Ay, Az, Gx, Gy, Gz, Temp	 2 bytes each (14)
												// e4x, e4y, e4z 4bytes each (12)
												// alpha 4bytes (4)
												// checksum 1
												// Total 33

#define PREAMBLE 0x65							// �p�P�b�g�̐擪�������f�[�^�i�v���A���u���j

#define SAMPLING 100							// �T���v�����O���g�� [Hz]
#define MAXDURATION 3600						// �f�[�^�̋L�^�͂P���ԁi�R�U�O�O�b�j�܂łƂ���
#define MAXDATASIZE (SAMPLING*DATASORT*MAXDURATION)

float databuf[DATASORT+1][MAXDATASIZE];			// �O���[�o���ϐ��z��ɃZ���T�f�[�^���i�[����
int errcnt;										// �ʐM�G���[�̉񐔂𐔂���ϐ�

int xaxis;										// �O���t�`�掞�̎��ԕ��@�i�P�ʁF�b�j
int yaxis;										// �O���u�`����s�����̔ԍ��i1�`DATASORT�j
int gain;										// �O���t�̏c���̊g��{��

static void CloseComPort(void)
// �I�[�v�����Ă���V���A���|�[�g���N���[�Y����
{
	if( hCOMnd == INVALID_HANDLE_VALUE)
		return;
	CloseHandle( hCOMnd);
	hCOMnd = INVALID_HANDLE_VALUE;
}

static DWORD OpenComPort(int port)
// port�ɂĎw�肵���ԍ��̃V���A���|�[�g���I�[�v������i�񓯊����[�h�j
{
	CString ComPortNum;
	COMMPROP	myComProp;
	DCB	myDCB;
	COMSTAT	myComStat;

	// �񓯊��h�n���[�h�Ȃ̂Ń^�C���A�E�g�͖���
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
	// COM�|�[�g���I�[�o���b�v�h���[�h�i�񓯊��ʐM���[�h�j�ɂăI�[�v�����Ă���

	if( hCOMnd == INVALID_HANDLE_VALUE){
		return -1;
	}

	GetCommProperties( hCOMnd, &myComProp);
	GetCommState(hCOMnd, &myDCB);
//	if( myComProp.dwSettableBaud & BAUD_128000)
//		myDCB.BaudRate = CBR_128000;
//	else
	myDCB.BaudRate = CBR_115200;		// 115.2Kbps���[�h��WindowsAPI�͐������F�����Ȃ�
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

// �I�[�o�[���b�v�h���[�h�ł́A�^�C���A�E�g�l�͈Ӗ����Ȃ��Ȃ�

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

// �I�[�o�[���b�v�h���[�h�ŁA�����ʐM�I�Ȃ��Ƃ��s�����߂̃p�b�`�R�[�h
			ReadFile( hCOMnd, rbuf, 1, &length, &rovl);
			while(1){
				if( HasOverlappedIoCompleted(&rovl)) break;
			}
		}
	}

	return d;
}


// �R�[���o�b�N�֐��ɂ��V���A���ʐM�󋵂̒ʒm����

int rcomp;

VOID CALLBACK FileIOCompletionRoutine( DWORD err, DWORD n, LPOVERLAPPED ovl)
{
	rcomp = n;			// �ǂݍ��񂾃o�C�g�������̂܂܃O���[�o���ϐ��ɕԂ�
}

// �����p�P�b�g����M���邽�߂̃X���b�h

#define RINGBUFSIZE 1024		// ��M�p�����O�o�b�t�@�̃T�C�Y�i�o�C�g���j

unsigned __stdcall serialchk( VOID * dummy)
{
	DWORD myErrorMask;						// ClearCommError ���g�p���邽�߂̕ϐ�
	COMSTAT	 myComStat;						// ��M�o�b�t�@�̃f�[�^�o�C�g�����擾���邽�߂Ɏg�p����
	unsigned char buf[RINGBUFSIZE];			// �����p�P�b�g����M���邽�߂̈ꎞ�o�b�t�@

	unsigned char ringbuffer[RINGBUFSIZE];	// �p�P�b�g��͗p�����O�o�b�t�@
	int rpos, wpos, rlen;					// �����O�o�b�t�@�p�|�C���^�iread, write)�A�f�[�^�o�C�g��
	int rpos_tmp;							// �f�[�^��͗pread�ʒu�|�C���^
	int rest;								// ClearCommError�ɂē�����f�[�^�o�C�g�����L�^����

	int seq, expected_seq;					// ���[�V�����Z���T�����M����V�[�P���X�ԍ��i�W�r�b�g�j
											// �ƁA���̊��Ғl�i�G���[���Ȃ���Ύ�M����l�j

	unsigned char packet[PACKETSIZE];		// �p�P�b�g��͗p�o�b�t�@ �i�P�p�P�b�g���j
	int i, chksum, tmp;
	float *ftmp;

	expected_seq = 0;						// �ŏ��Ɏ�M�����ׂ�SEQ�̒l�̓[��
	rpos = wpos = 0;						// �����O�o�b�t�@�̓ǂݏo���y�я������݃|�C���^��������
	rlen = 0;								// �����O�o�b�t�@�Ɏc���Ă���L���ȃf�[�^���i�o�C�g�j

	while(rf){
		rcomp = 0;					// FileIOCompletionRoutine���Ԃ���M�o�C�g�����N���A���Ă���
		// �܂��͖����p�P�b�g�̐擪�̂P�o�C�g��ǂݏo���ɍs��
		ReadFileEx( hCOMnd, buf, 1, &rovl, FileIOCompletionRoutine);

		while(1){
			SleepEx( 100, TRUE);	// �ő��100�~���b = 0.1�b�̊ԃX���[�v���邪�A���̊Ԃ�
			if(rcomp == 1) break;	// I/O�����R�[���o�b�N�֐������s�����ƃX���[�v����������

			if(!rf){				// �O���v���O��������X���b�h�̒�~���w�����ꂽ���̏���
				CancelIo(hCOMnd);	// ���s�ς݂�ReadFileEx������������Ă���
				break;
			}
									// �f�[�^�������Ă��Ȃ����ԑтł́A��M�X���b�h���̂��̕���
									// �����X�Ə�������Ă��邪�A�唼��SleepEx�̎��s�ɔ�₳���
									// ���ƂŁA�V�X�e���ɗ^���镉�ׂ��y�����Ă���B
		}

		if(!rf) break;				// �O���v���O��������X���b�h�̒�~���w�����ꂽ

		ringbuffer[wpos] = buf[0];	// �ŏ��̂P�o�C�g����M���ꂽ
		wpos++;						// �����O�o�b�t�@�̏������݈ʒu�|�C���^���X�V
		wpos = (wpos >= RINGBUFSIZE)?0:wpos;	// �P�����Ă�����|�C���^���[���ɖ߂��i�Ȃ̂�RING�j
		rlen++;						// �����O�o�b�t�@���̗L���ȃf�[�^�����{�P����

		ClearCommError( hCOMnd, &myErrorMask, &myComStat);	// ��M�o�b�t�@�̏󋵂𒲂ׂ�`�o�h

		rest = myComStat.cbInQue;	// ��M�o�b�t�@�ɓ����Ă���f�[�^�o�C�g����������

		if(rest == 0) continue;		// ���������Ă��Ȃ������̂Ŏ��̂P�o�C�g��҂��ɂ���

		rcomp = 0;
		ReadFileEx( hCOMnd, buf, rest, &rovl, FileIOCompletionRoutine);
									// ��M�o�b�t�@�ɓ����Ă���f�[�^��ǂݏo��
									// �����I�ɂ�rest�Ŏ�����鐔�̃f�[�^����M���邱�Ƃ��ł��邪�A
									// ����ɔ����ăf�[�^���s�����Ă��܂������̏������l����B

		SleepEx(16, TRUE);			// Windows�ɂ�����V���A���|�[�g�̕W�����C�e���V�i16msec�j�����҂�
		if( rcomp != rest){
			CancelIo(hCOMnd);		// ClearCommError�Ŏ擾�����f�[�^�o�C�g���ɖ����Ȃ�
		}							// �f�[�^������M����Ȃ������̂ŁA��ɔ��s����ReadFileEx
									// ���L�����Z�����Ă���

		i = 0;
		while(rcomp > 0){			// rcomp�ɂ͓ǂݏo�����Ƃ̂ł����f�[�^�̃o�C�g���������Ă���
			ringbuffer[wpos] = buf[i];	// �����O�o�b�t�@�Ɏ�M�f�[�^��]������
			wpos++;
			wpos = (wpos >= RINGBUFSIZE)?0:wpos;
			rlen++;
			i++;
			rcomp--;
		}

		// ��������p�P�b�g��͂ɓ���

		while(1){					// �L���ȃp�P�b�g�ł�������͂��p������
			while( rlen > 0){
				if( ringbuffer[rpos] == PREAMBLE) break;
				rpos++;								// �擪��PREAMBLE�ł͂Ȃ�����
				rpos = (rpos >= RINGBUFSIZE)?0:rpos;
				rlen--;								// �L���ȃf�[�^�����P���炵�čēx�擪�𒲂ׂ�
			}

			if(rlen < PACKETSIZE) break;	// ��͂ɕK�v�ȃf�[�^�o�C�g���ɒB���Ă��Ȃ������̂�
											// �ŏ��̂P�o�C�g��҂����ɖ߂�

			rpos_tmp = rpos;	// �����O�o�b�t�@�����؂��邽�߂̉��|�C���^
								// �܂������O�o�b�t�@��ɂ���f�[�^���L���ł���ƕ��������킯�ł͂Ȃ�

			for( i = 0, chksum = 0; i < (PACKETSIZE-1); i++){
				packet[i] = ringbuffer[rpos_tmp];	// �Ƃ肠������͗p�o�b�t�@�Ƀf�[�^�𐮗񂳂���
				chksum += packet[i];
				rpos_tmp++;
				rpos_tmp = (rpos_tmp >= RINGBUFSIZE)?0:rpos_tmp;
			}

			if( (chksum & 0xff) != ringbuffer[rpos_tmp]){	// �`�F�b�N�T���G���[�Ȃ̂Ńp�P�b�g�͖���
				rpos++;										// �擪�̂P�o�C�g���������
				rpos = (rpos >= RINGBUFSIZE)?0:rpos;
				rlen--;
				continue;	// ����PREAMBLE��T���ɂ���
			}

			// PREAMBLE�A�`�F�b�N�T���̒l��������PACKETSIZ���̃f�[�^��packet[]�ɓ����Ă���

			seq = packet[1];
			databuf[0][datasize] = (float)seq;		// seq �͂W�r�b�g�ɂ�0�`255�̊Ԃ�1���J�E���g�A�b�v���Ă������l

			if(seq != expected_seq){		// ��M���ꂽseq���A�P�O��seq��+1�������̂ł��邱�Ƃ��`�F�b�N����
				errcnt += (seq + 256 - expected_seq)%256;	// �p�P�b�g�G���[�����X�V����
			}
			expected_seq = (seq + 1)%256;					// ����seq�̊��Ғl��expected_seq�ɓ����

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
			datasize = (datasize >= MAXDATASIZE)?(MAXDATASIZE-1):datasize;	// �f�[�^�������E�ɓ��B�����ۂ̏���

			if( datasize%1 == 0){								// 10�T���v����1��̊����Ń��b�Z�[�W�𔭐�������
				PostMessage( hDlg, WM_RCV, (WPARAM)1, NULL);	// 100Hz�T���v�����O�ł����0.1�b��1���ʂ��X�V�����
			}													// PostMessage()���̂ł͏������Ԃ͋ɂ߂ĒZ��

			rpos = (rpos + PACKETSIZE)%RINGBUFSIZE;			// �������ǂݏo�����f�[�^�������O�o�b�t�@���珜������
			rlen -= PACKETSIZE;								// �o�b�t�@�̎c��e�ʂ��X�V����
		}
	}
	_endthreadex(0);	// �X���b�h�����ł�����
	return 0;
}

// �A�v���P�[�V�����̃o�[�W�������Ɏg���� CAboutDlg �_�C�A���O

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// �_�C�A���O �f�[�^
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV �T�|�[�g

// ����
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


// CPQ_GRAPHDlg �_�C�A���O


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
// ���b�Z�[�W�}�b�v�̍Ō��ON_MESSAGE�̍s�͎���͂��Ă���


// CPQ_GRAPHDlg ���b�Z�[�W �n���h��

BOOL CPQ_GRAPHDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// "�o�[�W�������..." ���j���[���V�X�e�� ���j���[�ɒǉ����܂��B

	hDlg = this->m_hWnd;		// ���̃_�C�A���O�ւ̃n���h�����擾����
								// ���̃R�[�h�͎���͂��Ă���

	// IDM_ABOUTBOX �́A�V�X�e�� �R�}���h�͈͓̔��ɂȂ���΂Ȃ�܂���B
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

	// ���̃_�C�A���O�̃A�C�R����ݒ肵�܂��B�A�v���P�[�V�����̃��C�� �E�B���h�E���_�C�A���O�łȂ��ꍇ�A
	//  Framework �́A���̐ݒ�������I�ɍs���܂��B
	SetIcon(m_hIcon, TRUE);			// �傫���A�C�R���̐ݒ�
	SetIcon(m_hIcon, FALSE);		// �������A�C�R���̐ݒ�

	// TODO: �������������ɒǉ����܂��B

	// �������R�[�h������͂ɂĒǉ����Ă���
	rf = 0;			// �����p�P�b�g��M�X���b�h�͒�~
	datasize = 0;	// ���[�V�����f�[�^�̐����O�ɏ�����


	// �ݒ�t�@�C����ǂݍ��݁A�V���A���ʐM�p�|�[�g�ԍ��ƃt�@�C���ۑ���t�H���_��ݒ肷��
	CStdioFile pFile;
	CString buf;
	char pbuf[256], dirbuf[_MAX_PATH];
	char pathname[256];
	int i, comport, dirlen;

	for( i = 0; i < 256; i++){
		pbuf[i] = pathname[i] = 0x00;	// ���S�̂���NULL�Ŗ��߂Ă���
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

		pbuf[1] = pbuf[2];	// Unicode�ɑΉ����邽�߂̃p�b�`�R�[�h
		pbuf[2] = 0x00;

		comport = atoi(pbuf);	// �|�[�g�ԍ����e�L�X�g�t�@�C������擾���Ă���

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
	midiOutOpen(&hMIDI, MIDI_MAPPER, 0, 0, CALLBACK_THREAD); // ������MIDI�f�o�C�X�����������邱�ƂŒx�����y������

	return TRUE;  // �t�H�[�J�X���R���g���[���ɐݒ肵���ꍇ�������ATRUE ��Ԃ��܂��B
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

// �_�C�A���O�ɍŏ����{�^����ǉ�����ꍇ�A�A�C�R����`�悷�邽�߂�
//  ���̃R�[�h���K�v�ł��B�h�L�������g/�r���[ ���f�����g�� MFC �A�v���P�[�V�����̏ꍇ�A
//  ����́AFramework �ɂ���Ď����I�ɐݒ肳��܂��B

void CPQ_GRAPHDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // �`��̃f�o�C�X �R���e�L�X�g

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// �N���C�A���g�̎l�p�`�̈���̒���
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// �A�C�R���̕`��
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// ���[�U�[���ŏ��������E�B���h�E���h���b�O���Ă���Ƃ��ɕ\������J�[�\�����擾���邽�߂ɁA
//  �V�X�e�������̊֐����Ăяo���܂��B
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
// �O���t��`�悷�邽�߂̃C�x���g�쓮�^�R�[�h�i�V�X�e������R�[���o�b�N�����j
{
	// -------------------------------General------------------------------
	// �W���C��
	double gx = databuf[4][datasize - 1];
	double gy = databuf[5][datasize - 1];
	double gz = databuf[6][datasize - 1];

	// �p���p
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
// START�{�^�����������ƁA���C�����X�f�[�^�̎�M�X���b�h���N������
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
// STOP�{�^�����������ƁA���C�����X�f�[�^�̎�M�X���b�h���~�����ăI�u�W�F�N�g��j������
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
	// TODO: �����Ƀ��b�Z�[�W �n���h���[ �R�[�h��ǉ����܂��B

	// ------------------------------Font------------------------------
	fontFlag = false;
	m_newFont->DeleteObject(); // m_newFont���̃I�u�W�F�N�g��j��
	delete m_newFont; // �|�C���^��j��
	// ------------------------------OpneGL------------------------------
	glFlag = false;
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(m_GLRC);
	delete m_pDC;
	// ------------------------------MIDI------------------------------
	midiOutReset(hMIDI); // MIDI�f�o�C�X�����Z�b�g����
	midiOutClose(hMIDI); // MIDI�f�o�C�X���I������
}


/**********************************************

�ȉ��A�e�����ɓ��������֐����蓮�ɂċL�q

***********************************************/

BOOL CPQ_GRAPHDlg::SetUpPixelFormat(HDC hdc)
{
	// TODO: �����Ɏ����R�[�h��ǉ����܂�.
	PIXELFORMATDESCRIPTOR pfd = {
	sizeof(PIXELFORMATDESCRIPTOR),  // PFD �̃T�C�Y
	1,                              // �o�[�W����
	PFD_DRAW_TO_WINDOW |            // �E�B���h�E�ɕ`�悷��
	PFD_SUPPORT_OPENGL |            // OpenGL ���g��
	PFD_DOUBLEBUFFER,               // �_�u���o�b�t�@�����O����
	PFD_TYPE_RGBA,                  // RGBA ���[�h
	24,                             // �J���[�o�b�t�@�� 24 �r�b�g
	0, 0, 0, 0, 0, 0,               //  (�e�`�����l���̃r�b�g���͎w�肵�Ȃ�) 
	0, 0,                           // �A���t�@�o�b�t�@�͎g��Ȃ�
	0, 0, 0, 0, 0,                  // �A�L�������[�V�����o�b�t�@�͎g��Ȃ�
	32,                             // �f�v�X�o�b�t�@�� 32 �r�b�g
	0,                              // �X�e���V���o�b�t�@�͎g��Ȃ�
	0,                              // �⏕�o�b�t�@�͎g��Ȃ�
	PFD_MAIN_PLANE,                 // ���C�����C���[
	0,                              //  (�\��) 
	0, 0, 0                         // ���C���[�}�X�N�͖�������
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
	CFont *curFont;	// CFont�̃|�C���^������
	curFont = msgED1.GetFont();	// Edit Control���猻�݂̃t�H���g���擾
	LOGFONTW mylf; // �ꎞ�I�ȃ}���`�o�C�g������p�̃t�H���g�ϐ�������
	curFont->GetLogFont(&mylf); // ���݂̃t�H���g��mylf�ɔ��f
	mylf.lfHeight = 30; //�����̍�����ύX
	mylf.lfWidth = 15; // �����̕���ύX
	m_newFont = new CFont; // �t�H���g�����������o�ϐ��ɃA�h���X�����蓖�Ă�
	m_newFont->CreateFontIndirectW(&mylf); // �_���t�H���g��m_newFont�Ɋi�[
	msgED1.SetFont(m_newFont); // �R���g���[���ϐ��Ƀt�H���g���Z�b�g
}

void CPQ_GRAPHDlg::glCallback()
{
	// TODO: �����Ɏ����R�[�h��ǉ����܂�.
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

		CRect rc; // �`��̈��ۑ����邽�߂̕ϐ���p��
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
