# OpenGLをマルチスレッドでMFCのコンテキストに描画させるための手順

1. gl,glu,glutの読み込みの記述をstdafx.hに記述

2. ダイアログにPicture Controlを配置
	IDを"IDC_GLVIEWに変更する

3. ~Dlg.cppに変数（private CStatic型）m_glViewを記述
	※void C~Dlg::DoDataExchange(CDataExchange* pDX)内に下記の記述を手動にて記述
    ```cpp
	DDX_Control(pDX, IDC_GLVIEW, m_glView);
    ```

4. ~Dlg.cppにメンバ変数 (private CDC*型）m_pDCを追加
	※m_pDCはピクチャコントロールのコンテキストを指している

5. ~Dlg.cppにメンバ変数 (private HGLRC型) m_GLRCを追加
　※m_GLRCはOpenGLのコンテキストを指している

6. ~Dlg.cppにメンバ関数(private BOOL型）SetupPixelFormatを追加
	※ダイアログを通した仮引数の設定がVS2017ではできない？ため、以下の仮引数を手動にて記述
	また、関数の実装は以下の通りとなる
    ```cpp
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
    ```

7. OnInit関数内にてスレッド実行フラグであるglFlagをfalseに初期化

8. ~Dlg.cppにメンバ関数（public void型）glSetupを追加
	※この関数でOpenGLのコンテキストをセットアップする
	また、この関数の中でBox::setupを呼び出し、GLの初期化を行う
    ```cpp 
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
    ```
 
9. クラスビューからWM_DESTROYを選択し、OnDestoryを追加する

10. ~Dlg::OnDestroy関数内で、OpenGLのコンテキストを削除に関する記述を行う
    ```cpp
	void COpenGLDlg::OnDestroy()
	{
		CDialogEx::OnDestroy();
		glFlag = false; // threadを抜ける
		// glのコンテキストを消去する
		wglMakeCurrent(NULL, NULL);
		wglDeleteContext(m_GLRC);
		delete m_pDC;
	}
    ```

11. ~Dlg.cppにメンバ関数 (publib void型）glCallback関数を追加
	※この関数は、スレッド生成時にスレッドに渡す関数をまとめたコールバック関数である
	コールバック関数内では、glSetup(), Box::update(), Box::draw(), 
	SwapBuffers(m_pDC->m_hDC)を呼び出す。また必要に応じて、スリープを設ける。
    ```cpp
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
    }
    ```

12. 動作確認用のEdit Controlをダイアログに追加する。また、ダイアログにコントローラ変数を追加する。
	※変数名msgEDとする。

13. スレッドを開始するボタンをダイアログに追加する。また、以下の処理を記述。
	```cpp
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
    ```

14. スレッドを終了させるボタンをダイアログに追加する。また、以下の処理を記述。
	```cpp
	void ~Dlg::OnBnClickedButton2()
	{
		// TODO: ここにコントロール通知ハンドラー コードを追加します。
		glFlag = false;
		msgED.SetWindowTextW(_T("Killed thread"));
	}
    ```

15. ウィンドウ全体を再描画するために~Dlg.cpp::OnPain()内のelse内を以下の記述にする。
	```cpp
    else{
		CDialogEx::OnPaint();
	}
    ```

## ※OpenGLの初期化から描画までの処理は同一スレッド内で処理させなければならない。