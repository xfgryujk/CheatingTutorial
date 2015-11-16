
// C2ZDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "C2Z.h"
#include "C2ZDlg.h"
#include "afxdialogex.h"

#pragma region 不用管
#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CC2ZDlg 对话框



CC2ZDlg::CC2ZDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CC2ZDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CC2ZDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_BUTTON1, m_enableButton);
	DDX_Control(pDX, IDC_BUTTON2, m_disableButton);
}

BEGIN_MESSAGE_MAP(CC2ZDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON1, &CC2ZDlg::OnBnClickedButton1)
	ON_BN_CLICKED(IDC_BUTTON2, &CC2ZDlg::OnBnClickedButton2)
END_MESSAGE_MAP()


// CC2ZDlg 消息处理程序

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CC2ZDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CC2ZDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}
#pragma endregion

//////////////////////////////////////////////////////////////////////////////

// 初始化
BOOL CC2ZDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	OnBnClickedButton1();

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

// 开启
void CC2ZDlg::OnBnClickedButton1()
{
	m_enableButton.EnableWindow(FALSE);
	m_disableButton.EnableWindow(TRUE);
	SetTimer(1, 200, timerProc); // 每0.2s检测C键是否按下，并模拟Z键
}

// 关闭
void CC2ZDlg::OnBnClickedButton2()
{
	m_enableButton.EnableWindow(TRUE);
	m_disableButton.EnableWindow(FALSE);
	KillTimer(1);
}

//定时模拟按下Z
void CALLBACK CC2ZDlg::timerProc(HWND hWnd, UINT nMsg, UINT nTimerid, DWORD dwTime)
{
	if ((GetKeyState('C') & (1 << 15)) != 0) // C键按下
	{
		INPUT input;
		ZeroMemory(&input, sizeof(input));
		input.type = INPUT_KEYBOARD;
		input.ki.wVk = 'Z';
		input.ki.wScan = MapVirtualKey(input.ki.wVk, MAPVK_VK_TO_VSC);
		SendInput(1, &input, sizeof(INPUT)); // 按下Z键
		Sleep(100); // 可能东方是在处理逻辑时检测一下Z键是否按下才发弹幕，如果这时Z键刚好弹起就没有反应，所以要延迟一下
		input.ki.dwFlags = KEYEVENTF_KEYUP;
		SendInput(1, &input, sizeof(INPUT)); // 弹起Z键
	}
}
