
// TH14Cheat.h : PROJECT_NAME 应用程序的主头文件
//

#pragma once

#ifndef __AFXWIN_H__
	#error "在包含此文件之前包含“stdafx.h”以生成 PCH 文件"
#endif

#include "resource.h"		// 主符号


// CTH14CheatApp: 
// 有关此类的实现，请参阅 TH14Cheat.cpp
//

class CTH14CheatApp : public CWinApp
{
public:
	CTH14CheatApp();

// 重写
public:
	virtual BOOL InitInstance();

// 实现

	DECLARE_MESSAGE_MAP()
};

extern CTH14CheatApp theApp;