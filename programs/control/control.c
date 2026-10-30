/*
 *   Start a control panel applet or open the control panel window
 *
 *   Copyright (C) 1998 by Marcel Baur <mbaur@g26.ethz.ch>
 *   Copyright 2010 Detlef Riekenberg
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define WIN32_LEAN_AND_MEAN

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>


extern void WINAPI Control_RunDLLW(HWND hWnd, HINSTANCE hInst, LPCWSTR cmd, DWORD nCmdShow);

static void launch(LPCWSTR what)
{
  static const WCHAR suffix[] = L"\\rundll32.exe";
  WCHAR app[MAX_PATH], *cmdline;
  STARTUPINFOW si = {0};
  PROCESS_INFORMATION pi;
  UINT len;

  if (!*what)
  {
    Control_RunDLLW(GetDesktopWindow(), 0, what, SW_SHOW);
    ExitProcess(0);
  }

  len = GetSystemDirectoryW(app, ARRAY_SIZE(app));
  if (!len || len > ARRAY_SIZE(app) - ARRAY_SIZE(suffix)) ExitProcess(1);
  lstrcatW(app, suffix);
  len = lstrlenW(app) + lstrlenW(what) + ARRAY_SIZE(L"\"\" shell32.dll,Control_RunDLL ");
  if (!(cmdline = malloc(len * sizeof(*cmdline)))) ExitProcess(1);
  lstrcpyW(cmdline, L"\"");
  lstrcatW(cmdline, app);
  lstrcatW(cmdline, L"\" shell32.dll,Control_RunDLL ");
  lstrcatW(cmdline, what);
  si.cb = sizeof(si);
  if (!CreateProcessW(app, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
  {
    free(cmdline);
    ExitProcess(1);
  }
  free(cmdline);
  CloseHandle(pi.hThread);
  WaitForSingleObject(pi.hProcess, INFINITE);
  CloseHandle(pi.hProcess);
  ExitProcess(0);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR lpszCmdLine, INT nCmdShow)
{
    InitCommonControls();

    /* no parameters - pop up whole "Control Panel" by default */
    if (!*lpszCmdLine) {
        launch(lpszCmdLine);
    }

    /* check for optional parameter */
    if (!lstrcmpiW(lpszCmdLine, L"COLOR"))
        launch(L"desk.cpl,,2");
    if (!lstrcmpiW(lpszCmdLine, L"DATE/TIME"))
        launch(L"timedate.cpl");
    if (!lstrcmpiW(lpszCmdLine, L"DESKTOP"))
        launch(L"desk.cpl");
    if (!lstrcmpiW(lpszCmdLine, L"INTERNATIONAL"))
        launch(L"intl.cpl");
    if (!lstrcmpiW(lpszCmdLine, L"KEYBOARD"))
        launch(L"main.cpl @1");
    if (!lstrcmpiW(lpszCmdLine, L"MOUSE"))
        launch(L"main.cpl");
    if (!lstrcmpiW(lpszCmdLine, L"PORTS"))
        launch(L"sysdm.cpl,,1");
    if (!lstrcmpiW(lpszCmdLine, L"PRINTERS"))
        launch(L"main.cpl @2");

    /* try to launch if a .cpl file is given directly */
    launch(lpszCmdLine);
    return 0;
}
