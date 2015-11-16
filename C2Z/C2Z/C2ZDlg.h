
// C2ZDlg.h : 头文件
//

#pragma once
#include "afxwin.h"


// CC2ZDlg 对话框
class CC2ZDlg : public CDialogEx
{
// 构造
public:
	CC2ZDlg(CWnd* pParent = NULL);	// 标准构造函数

// 对话框数据
	enum { IDD = IDD_C2Z_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedButton1();
	afx_msg void OnBnClickedButton2();

	static void CALLBACK timerProc(HWND hWnd, UINT nMsg, UINT nTimerid, DWORD dwTime);


public:
	CButton m_enableButton;
	CButton m_disableButton;
};
