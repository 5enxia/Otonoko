#include "stdafx.h"

namespace Viewer {
	double degX,degY,degZ= 0;
	double bias = 45; // 度数

	void drawAxis(float len);

	// --------------------------setup---------------------------------------	
	void setup(double width, double height){
		GLdouble aspect = (GLdouble)width/(GLdouble) height;
		glClearColor(0, 0, 0, 1);
		glViewport(0, 0, width, height);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(60.0, aspect, 1.0, 20.0);

		//視点変更（z軸+2.0のところから見る）
		gluLookAt(0.0, 1.0, 2.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0); 
	}

	// -----------------------update--------------------------
	void update(double ex, double ey, double ez){
		degX =  (180/PI) * asin(ex);
		degY = (180 / PI) * asin(ey);
		degZ = (180 / PI) * asin(ez);
	}

	// ------------------------draw-------------------------
	void draw(){
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // OpenGLによる描画

		//axis
		glPushMatrix();
		glTranslatef(-1.2,-1.2,0);
		drawAxis(0.25);
		double div = 0.3;
		
		// x
		glRasterPos2f(0 + div ,0);
		glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24,'x');
		// y
		glRasterPos2f(0,0 + div);
		glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24,'y');
		// z
		glRasterPos2f(0 - div,0 - div);
		glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24,'z');
		glPopMatrix();

		//cube
		glPushMatrix();
		glRotatef(degX,1,0,0); // x
		glRotatef(degY,0,1,0); // y
		glRotatef(degZ,0,0,1); // z
		drawAxis(0.5);
		glutWireCube(1);
		//glutWireTeapot(1);
		glPopMatrix();
		
		glFlush();
	}

// -----------------------------drawAxis------------------------------------
	void drawAxis(float len){
		glLineWidth(3);

		// x
		glBegin(GL_LINES);
		glColor3f(1,0,0);
		glVertex3f(0,0,0);
		glVertex3f(len,0,0);
		glEnd();

		// y
		glBegin(GL_LINES);
		glColor3f(0,1,0);
		glVertex3f(0,0,0);
		glVertex3f(0,len,0);
		glEnd();
		
		// z
		glBegin(GL_LINES);
		glColor3f(0,0,1);
		glVertex3f(0,0,0);
		glVertex3f(0,0,len);
		glEnd();
		
		glColor3f(1.0, 1.0, 1.0);
	}
};


/****************************************************
OpenGLをマルチスレッドでMFCのコンテキストに描画させるための手順
*****************************************************

1. gl,glu,glutの読み込みの記述をstdafx.hに記述

2.ダイアログにPicture Controlを配置
	IDを"IDC_GLVIEWに変更する

3. ~Dlg.cppに変数（private CStatic型）m_glViewを記述
	※void C~Dlg::DoDataExchange(CDataExchange* pDX)内に下記の記述を手動にて記述
	DDX_Control(pDX, IDC_GLVIEW, m_glView);

4. ~Dlg.cppにメンバ変数 (private CDC*型）m_pDCを追加
	※m_pDCはピクチャコントロールのコンテキストを指している

5. ~Dlg.cppにメンバ変数 (private HGLRC型) m_GLRCを追加
　※m_GLRCはOpenGLのコンテキストを指している

6. ~Dlg.cppにメンバ関数(private BOOL型）SetupPixelFormatを追加
	※ダイアログを通した仮引数の設定がVS2017ではできない？ため、以下の仮引数を手動にて記述
	また、関数の実装は以下の通りとなる
	
	BOOL ~Dlg::SetUpPixelFormat(HDC hdc)
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

7. OnInit関数内にてスレッド実行フラグであるglFlagをfalseに初期化

8. ~Dlg.cppにメンバ関数（public void型）glSetupを追加
	※この関数でOpenGLのコンテキストをセットアップする
	また、この関数の中でBox::setupを呼び出し、GLの初期化を行う
	
	void ~Dlg::glSetup()
	{
		// TODO: ここに実装コードを追加します.
		m_pDC = new CClientDC(&m_glView);

		if (SetUpPixelFormat(m_pDC->m_hDC) != FALSE) {
			m_GLRC = wglCreateContext(m_pDC->m_hDC);
			wglMakeCurrent(m_pDC->m_hDC, m_GLRC);

			CRect rc; // 描画領域を保存するための変数を用意
			m_glView.GetClientRect(&rc);
			GLint width = rc.Width();
			GLint height = rc.Height();

			Viewer::setup(width,height);
		}
	}
 
9. クラスビューからWM_DESTROYを選択し、OnDestoryを追加する

10. ~Dlg::OnDestroy関数内で、OpenGLのコンテキストを削除に関する記述を行う
	void COpenGLDlg::OnDestroy()
	{
		CDialogEx::OnDestroy();
		glFlag = false; // threadを抜ける
		// glのコンテキストを消去する
		wglMakeCurrent(NULL, NULL);
		wglDeleteContext(m_GLRC);
		delete m_pDC;
	}

11. ~Dlg.cppにメンバ関数 (publib void型）glCallback関数を追加
	※この関数は、スレッド生成時にスレッドに渡す関数をまとめたコールバック関数である
	コールバック関数内では、glSetup(), Box::update(), Box::draw(), 
	SwapBuffers(m_pDC->m_hDC)を呼び出す。また必要に応じて、スリープを設ける。


	void COpenGLDlg::glCallback()
	{
		// TODO: ここに実装コードを追加します.
		glSetup();
		while (glFlag) {
			Viewer::update();
			Viewer::draw();
			SwapBuffers(m_pDC->m_hDC);
			//std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

12. 動作確認用のEdit Controlをダイアログに追加する。また、ダイアログにコントローラ変数を追加する。
	※変数名msgEDとする。

13. スレッドを開始するボタンをダイアログに追加する。また、以下の処理を記述。
	
	void ~Dlg::OnBnClickedButton1()
	{
		// TODO: ここにコントロール通知ハンドラー コードを追加します。
		if (glFlag) {
			msgED.SetWindowTextW(_T("Thread is Running...\n"));
		}
		else {
			msgED.SetWindowTextW(_T("Start thread\n"));
			glFlag = true;
			std::thread th(&COpenGLDlg::glCallback,this);
			th.detach();
		}
	}

13. スレッドを終了させるボタンをダイアログに追加する。また、以下の処理を記述。
	
	void ~Dlg::OnBnClickedButton2()
	{
		// TODO: ここにコントロール通知ハンドラー コードを追加します。
		glFlag = false;
		msgED.SetWindowTextW(_T("Killed thread"));
	}

15. ウィンドウ全体を再描画するために~Dlg.cpp::OnPain()内のelse内を以下の記述にする。
	else{
		CDialogEx::OnPaint();
	}

****************************************************
※OpenGLの初期化から描画までの処理は同一スレッド内で処理させなければならない。
****************************************************/
