
// TH14CheatDlg.h : 头文件
//

#pragma once


// CTH14CheatDlg 对话框
class CTH14CheatDlg : public CDialogEx
{
// 构造
public:
	CTH14CheatDlg(CWnd* pParent = NULL);	// 标准构造函数

// 对话框数据
	enum { IDD = IDD_TH14CHEAT_DIALOG };

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
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDestroy();

	static LRESULT CALLBACK KbdProc(int code, WPARAM wParam, LPARAM lParam);

	void modifyHpCode();
	void modifyBombCode();
	void modifyPowerCode();


public:
	HHOOK m_hook;

	BOOL m_lockHp;
	BOOL m_lockBomb;
	BOOL m_lockPower;

	HANDLE m_process;
};
