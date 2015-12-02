
// TH14CheatDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "TH14Cheat.h"
#include "TH14CheatDlg.h"
#include "afxdialogex.h"

// 定义则使用修改代码实现，否则使用每秒写内存实现
#define MODIFY_CODE

#pragma region 不用管
#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CTH14CheatDlg 对话框



CTH14CheatDlg::CTH14CheatDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CTH14CheatDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	m_lockHp = m_lockBomb = m_lockPower = FALSE;
	m_process = NULL;
}

void CTH14CheatDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CTH14CheatDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_TIMER()
	ON_WM_DESTROY()
END_MESSAGE_MAP()


// CTH14CheatDlg 消息处理程序

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CTH14CheatDlg::OnPaint()
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
HCURSOR CTH14CheatDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}
#pragma endregion

//////////////////////////////////////////////////////////////////////////////

// 初始化
BOOL CTH14CheatDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// 注册键盘钩子监听热键
	m_hook = SetWindowsHookEx(WH_KEYBOARD_LL, KbdProc, GetModuleHandle(NULL), NULL);
	// 注册1s定时器
	SetTimer(0, 1000, NULL);

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

// 释放
void CTH14CheatDlg::OnDestroy()
{
	CDialogEx::OnDestroy();

	UnhookWindowsHookEx(m_hook);
	if (m_process != NULL)
	{
		// 释放句柄
		CloseHandle(m_process);
	}
}

// 处理热键
LRESULT CALLBACK CTH14CheatDlg::KbdProc(int code, WPARAM wParam, LPARAM lParam)
{
	CTH14CheatDlg* dlg = (CTH14CheatDlg*)AfxGetApp()->m_pMainWnd;
	if (code != HC_ACTION)
		return CallNextHookEx(dlg->m_hook, code, wParam, lParam);

	PKBDLLHOOKSTRUCT param = (PKBDLLHOOKSTRUCT)lParam;
	if ((param->flags & (1 << 7)) == 0) // 按下
	{
		switch (param->vkCode)
		{
		case VK_F1: // 无限残机
			dlg->m_lockHp = !dlg->m_lockHp;
#ifdef MODIFY_CODE
			dlg->modifyHpCode();
#endif
			return 1;
		case VK_F2: // 无限炸弹
			dlg->m_lockBomb = !dlg->m_lockBomb;
#ifdef MODIFY_CODE
			dlg->modifyBombCode();
#endif
			return 1;
		case VK_F3: // 满灵力
			dlg->m_lockPower = !dlg->m_lockPower;
#ifdef MODIFY_CODE
			dlg->modifyPowerCode();
#endif
			return 1;
		}
	}

	return CallNextHookEx(dlg->m_hook, code, wParam, lParam);
}

// 处理定时器
void CTH14CheatDlg::OnTimer(UINT_PTR nIDEvent)
{
	HWND hwnd = ::FindWindow(_T("BASE"), NULL);
	if (hwnd == NULL) // 进程已关闭
	{
		if (m_process != NULL)
		{
			// 释放句柄
			CloseHandle(m_process);
			m_process = NULL;
		}
	}
	else
	{
		// 打开进程
		if (m_process == NULL)
		{
			DWORD pid;
			GetWindowThreadProcessId(hwnd, &pid);
			m_process = OpenProcess(/*PROCESS_ALL_ACCESS*/ PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, pid);
			if (m_process == NULL)
			{
				CString msg;
				msg.Format(_T("打开进程失败，错误代码：%u"), GetLastError());
				MessageBox(msg, NULL, MB_ICONERROR);
				CDialogEx::OnTimer(nIDEvent);
				return;
			}

#ifdef MODIFY_CODE
			// 刚打开进程时修改代码
			modifyHpCode();
			modifyBombCode();
			modifyPowerCode();
#endif
		}

#ifndef MODIFY_CODE
		// 写内存
		DWORD buffer;
		if (m_lockHp)
		{
			WriteProcessMemory(m_process, (LPVOID)0x004F5864, &(buffer = 8), sizeof(DWORD), NULL);
		}
		if (m_lockBomb)
		{
			WriteProcessMemory(m_process, (LPVOID)0x004F5870, &(buffer = 8), sizeof(DWORD), NULL);
		}
		if (m_lockPower)
		{
			WriteProcessMemory(m_process, (LPVOID)0x004F5858, &(buffer = 400), sizeof(DWORD), NULL);
		}
#endif
	}

	CDialogEx::OnTimer(nIDEvent);
}

// 修改关于残机的代码
void CTH14CheatDlg::modifyHpCode()
{
	static const BYTE originalCode[] = { 0xA3, 0x64, 0x58, 0x4F, 0x00 };
	static const BYTE modifiedCode[] = { 0x90, 0x90, 0x90, 0x90, 0x90 };
	if (m_process != NULL)
	{
		WriteProcessMemory(m_process, (LPVOID)0x0044F618, m_lockHp ? modifiedCode : originalCode, sizeof(originalCode), NULL);
	}
}

// 修改关于炸弹的代码
void CTH14CheatDlg::modifyBombCode()
{
	static const BYTE originalCode1[] = { 0xA3, 0x70, 0x58, 0x4F, 0x00 };
	static const BYTE modifiedCode1[] = { 0x90, 0x90, 0x90, 0x90, 0x90 };
	static const BYTE originalCode2[] = { 0x7E, 0x0E };
	static const BYTE modifiedCode2[] = { 0x90, 0x90 };
	if (m_process != NULL)
	{
		WriteProcessMemory(m_process, (LPVOID)0x0041218A, m_lockBomb ? modifiedCode1 : originalCode1, sizeof(originalCode1), NULL);
		WriteProcessMemory(m_process, (LPVOID)0x0044DD68, m_lockBomb ? modifiedCode2 : originalCode2, sizeof(originalCode2), NULL);
	}
}

// 修改关于灵力的代码
void CTH14CheatDlg::modifyPowerCode()
{
	static const BYTE originalCode1[] = { 0xA3, 0x58, 0x58, 0x4F, 0x00 };
	static const BYTE modifiedCode1[] = { 0xB8, 0x90, 0x01, 0x00, 0x00 };
	static const BYTE originalCode2[] = { 0x03, 0xC8, 0x3B, 0xCE, 0x0F, 0x4C, 0xCD };
	static const BYTE modifiedCode2[] = { 0xB9, 0x90, 0x01, 0x00, 0x00, 0x90, 0x90 };
	if (m_process != NULL)
	{
		WriteProcessMemory(m_process, (LPVOID)0x00435DAF, m_lockPower ? modifiedCode1 : originalCode1, sizeof(originalCode1), NULL);
		WriteProcessMemory(m_process, (LPVOID)0x0044DDB8, m_lockPower ? modifiedCode2 : originalCode2, sizeof(originalCode2), NULL);
		if (m_lockPower)
		{
			DWORD buffer;
			WriteProcessMemory(m_process, (LPVOID)0x004F5858, &(buffer = 400), sizeof(DWORD), NULL);
		}
	}
}

