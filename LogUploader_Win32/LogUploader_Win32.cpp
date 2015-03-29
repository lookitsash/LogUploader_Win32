// LogUploader_Win32.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "LogUploader_Win32.h"
#include <shlobj.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include "miniz.c"

#include "dirent.h"


#define MAX_LOADSTRING 100

#ifndef S_ISDIR
#define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#endif

#ifndef S_ISREG
#define S_ISREG(mode)  (((mode) & S_IFMT) == S_IFREG)
#endif

#ifndef SUCCEED
#define SUCCEED(ret) (ret == S_OK)
#endif

inline char* pathSeparatorStr()
{
#ifdef _WIN32
    return "\\";
#else
    return "/";
#endif
}

struct ZipEntry
{
	char FilePath[512];
	char RelativePath[512];
};

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

void AddFolderToZipArchive(mz_zip_archive* zipArchive, const char *path, const char *relativePath);
void listdir (const char *path);

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	//ZipEntry* zipEntries = malloc(
	//AddFolderToZipArchive("C:\\Users\\akhan.COLLAGES\\.printnode","");
	//return FALSE;

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

 	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_LOGUPLOADER_WIN32, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_LOGUPLOADER_WIN32));

	WCHAR userFolder[MAX_PATH];
	if (SUCCEED(SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, userFolder))) {
		char *folderToCompress = (char *)malloc( 1024 );
		wcstombs(folderToCompress, userFolder, 1024);
		strcat(folderToCompress, "\\.printnode");

		struct stat sb;
		if (stat(folderToCompress, &sb) == 0 && S_ISDIR(sb.st_mode))
		{
			char *targetZIPFile = (char *)malloc( 1024 );
			wcstombs(targetZIPFile, userFolder, 1024);
			strcat(targetZIPFile, "\\.printnodeArchive.zip");
			remove(targetZIPFile);

			//BEGIN ZIP
			mz_zip_archive zip;
			memset(&zip, 0, sizeof(zip));

			if (!mz_zip_writer_init_file(&zip, targetZIPFile, 65537))
			{
			//print_error("Failed creating zip archive \"%s\" (1)!\n", pZip_filename);
				return FALSE;
			}

			mz_bool success = MZ_TRUE;

			//char* tmpFile = "C:\\Users\\akhan.COLLAGES\\.printnode\\Pic01.jpg";

			AddFolderToZipArchive(&zip, "E:\\Pictures\\test\\Subcategories","");
			//success &= mz_zip_writer_add_file(&zip, "test/sfds/Pic01.jpg", "C:\\Users\\akhan.COLLAGES\\.printnode\\Pic01.jpg", 0, 0, MZ_BEST_COMPRESSION);
			//success &= mz_zip_writer_add_file(&zip, "Pic02.jpg", "C:\\Users\\akhan.COLLAGES\\.printnode\\Pic02.jpg", 0, 0, MZ_BEST_COMPRESSION);

			if (!success)
			{
				mz_zip_writer_end(&zip);
				remove(targetZIPFile);
				//print_error("Failed creating zip archive \"%s\" (2)!\n", pZip_filename);
				return FALSE;
			}

			if (!mz_zip_writer_finalize_archive(&zip))
			{
				mz_zip_writer_end(&zip);
				remove(targetZIPFile);
				//print_error("Failed creating zip archive \"%s\" (3)!\n", pZip_filename);
				return FALSE;
			}

			mz_zip_writer_end(&zip);
			//END ZIP

			free(targetZIPFile);
		}
		else
		{
			// folder does not exist
		}

		free(folderToCompress);
	}

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_LOGUPLOADER_WIN32));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= 0;//MAKEINTRESOURCE(IDC_LOGUPLOADER_WIN32);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

void AddFolderToZipArchive(mz_zip_archive* zipArchive, const char *path, const char *relativePath)
{
	DIR *pdir = NULL;
	pdir = opendir (path);
	struct dirent *pent = NULL;
	if (pdir == NULL) return;

	while (pent = readdir (pdir)) // while there is still something in the directory to list
	{
		if (pent == NULL) return;
		if (pent->d_type == DT_DIR && strcmp(pent->d_name,".") != 0 && strcmp(pent->d_name,"..") != 0)
		{
			char* childPath = (char*)malloc(512);
			childPath[0] = NULL;
			strcat(childPath,path);
			strcat(childPath,pathSeparatorStr());
			strcat(childPath,pent->d_name);

			char* childRelativePath = (char*)malloc(512);
			childRelativePath[0] = NULL;
			strcat(childRelativePath, relativePath);
			strcat(childRelativePath,pent->d_name);
			strcat(childRelativePath,"/");

			mz_zip_writer_add_mem(zipArchive, (const char*)childRelativePath, NULL, 0, MZ_BEST_COMPRESSION);
			
			AddFolderToZipArchive(zipArchive,childPath,childRelativePath);
			
			free(childPath);
			free(childRelativePath);
		}
		else if (pent->d_type == DT_REG)
		{
			char* childPath = (char*)malloc(512);
			childPath[0] = NULL;
			strcat(childPath,path);
			strcat(childPath,pathSeparatorStr());
			strcat(childPath,pent->d_name);

			char* childRelativePath = (char*)malloc(512);
			childRelativePath[0] = NULL;
			strcat(childRelativePath, relativePath);
			strcat(childRelativePath,pent->d_name);

			mz_zip_writer_add_file(zipArchive, (const char*)childRelativePath, (const char*)childPath, 0, 0, MZ_BEST_COMPRESSION);
			
			free(childPath);
			free(childRelativePath);
		}
	}

	// finally, let's close the directory
	closedir (pdir);
}

/*
 * A function to list all contents of a given directory
 * author: Danny Battison
 * contact: gabehabe@hotmail.com
 */

void listdir (const char *path)
{
	// first off, we need to create a pointer to a directory
	DIR *pdir = NULL; // remember, it's good practice to initialise a pointer to NULL!
	pdir = opendir (path); // "." will refer to the current directory
	struct dirent *pent = NULL;
	if (pdir == NULL) // if pdir wasn't initialised correctly
	{ // print an error message and exit the program
		printf ("\nERROR! pdir could not be initialised correctly");
		return; // exit the function
	} // end if

	while (pent = readdir (pdir)) // while there is still something in the directory to list
	{
		if (pent == NULL) // if pent has not been initialised correctly
		{ // print an error message, and exit the program
			printf ("\nERROR! pent could not be initialised correctly");
			return; // exit the function
		}
		if (pent->d_type == DT_DIR)
		{
			printf ("%s\n", pent->d_name);
		}
		// otherwise, it was initialised correctly. let's print it on the console:
		printf ("%s\n", pent->d_name);
	}

	// finally, let's close the directory
	closedir (pdir);
}

/** EXAMPLE USAGE **/
int main ()
{
	listdir ("C:\\");
	return 0;
}


//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hInst = hInstance; // Store instance handle in our global variable

   hWnd = CreateWindow(szWindowClass, L"Log Uploader", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
      CW_USEDEFAULT, 0, 400, 300, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code here...
		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
