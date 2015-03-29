// LogUploader_Win32.cpp : Defines the entry point for the application.
//
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' " \
"version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")



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

BOOL FolderCompressionInProgress;
HWND hwndPB;
HWND hwndPBLabel;

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

void ProcessZipAction(HWND hwndParent, HINSTANCE hInstance);
mz_bool ZipFolder(const char* folderToZip, const char *targetZIPFile);
mz_bool AddFolderToZipArchive(mz_zip_archive* zipArchive, const char *path, const char *relativePath);
void listdir (const char *path);
DWORD WINAPI ThreadRoutine(LPVOID lpArg);

void ProcessZipAction(HWND hwndParent, HINSTANCE hInstance)
{
	FolderCompressionInProgress = TRUE;

	RECT client_rectangle;
	GetClientRect(hwndParent, &client_rectangle);
	int width = client_rectangle.right - client_rectangle.left;
	int height = client_rectangle.bottom - client_rectangle.top;

	//HWND hwndPB;    // Handle of progress bar.
	
	hwndPB = CreateWindowEx(WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE,    
                                   PROGRESS_CLASS, NULL,   
                                   WS_CHILD | WS_VISIBLE|PBS_MARQUEE,   
                                   (width-200)/2, ((height-10)/2)+10, 200,
                                   20, hwndParent, NULL,   
                                   hInstance, NULL);

	//SendMessage(hwndPB, PBM_SETRANGE, 0, (LPARAM) MAKELPARAM(0, 599));
	//SendMessage(hwndPB, PBM_SETPOS, 0, 0);
	SendMessage(hwndPB, PBM_SETMARQUEE, 1, 0);

	hwndPBLabel = CreateWindow(
                        TEXT("STATIC"),                   /*The name of the static control's class*/
                        TEXT("Preparing files for upload..."),                  /*Label's Text*/
                        WS_CHILD | WS_VISIBLE | SS_LEFT,  /*Styles (continued)*/
                        (width-180)/2,                                /*X co-ordinates*/
                        (height/2)-20,                                /*Y co-ordinates*/
                        180,                               /*Width*/
                        25,                               /*Height*/
                        hwndParent,                             /*Parent HWND*/
                        NULL,              /*The Label's ID*/
                        hInstance,                        /*The HINSTANCE of your program*/ 
                        NULL);                            /*Parameters for main window*/
	
	
	//SetBkColor

	DWORD ThreadId;
	CreateThread(NULL,0,ThreadRoutine,NULL,0,&ThreadId);
	
	
	//DestroyWindow(hwndPB);
}

DWORD WINAPI ThreadRoutine(LPVOID lpArg)
{
    WCHAR userFolder[MAX_PATH];
	if (SUCCEED(SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, userFolder))) { // Get user home folder
		char *folderToCompress = (char *)malloc( 1024 );
		wcstombs(folderToCompress, userFolder, 1024);
		strcat(folderToCompress, "\\.printnode");

		struct stat sb;
		if (stat(folderToCompress, &sb) == 0 && S_ISDIR(sb.st_mode)) // Check if target folder exists to zip up
		{
			char *targetZIPFile = (char *)malloc( 1024 );
			wcstombs(targetZIPFile, userFolder, 1024);
			strcat(targetZIPFile, "\\.printnodeArchive.zip");
			
			//mz_bool zipSuccess = ZipFolder(folderToCompress, targetZIPFile); // Zip up target folder
			mz_bool zipSuccess = ZipFolder("E:\\Pictures\\test\\Subcategories", targetZIPFile); // Zip up target folder

			free(targetZIPFile);
		}
		else
		{
			// folder does not exist
		}
		free(folderToCompress);
	}
	FolderCompressionInProgress = FALSE;

	return NULL;
}

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

	/*
	
	*/
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

mz_bool ZipFolder(const char* folderToZip, const char *targetZIPFile)
{
	remove(targetZIPFile);

	mz_zip_archive zip;
	memset(&zip, 0, sizeof(zip));

	mz_bool success = MZ_TRUE;

	success &= mz_zip_writer_init_file(&zip, targetZIPFile, 65537);
	if (!success)
	{
		//print_error("Failed creating zip archive \"%s\" (1)!\n", pZip_filename);
		return success;
	}

	

	success &= AddFolderToZipArchive(&zip, folderToZip,"");
	if (!success)
	{
		mz_zip_writer_end(&zip);
		remove(targetZIPFile);
		//print_error("Failed creating zip archive \"%s\" (2)!\n", pZip_filename);
		return success;
	}

	success &= mz_zip_writer_finalize_archive(&zip);
	if (!success)
	{
		mz_zip_writer_end(&zip);
		remove(targetZIPFile);
		//print_error("Failed creating zip archive \"%s\" (3)!\n", pZip_filename);
		return success;
	}

	success &= mz_zip_writer_end(&zip);
	if (!success)
	{
		remove(targetZIPFile);
		//print_error("Failed creating zip archive \"%s\" (3)!\n", pZip_filename);
		return success;
	}
	
	return success;
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

mz_bool AddFolderToZipArchive(mz_zip_archive* zipArchive, const char *path, const char *relativePath)
{
	DIR *pdir = NULL;
	pdir = opendir (path);
	struct dirent *pent = NULL;
	if (pdir == NULL) return MZ_FALSE;

	while (pent = readdir (pdir)) // while there is still something in the directory to list
	{
		if (pent == NULL) return MZ_FALSE;
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

			mz_bool success = mz_zip_writer_add_mem(zipArchive, (const char*)childRelativePath, NULL, 0, MZ_BEST_COMPRESSION);
			if (success)
			{
				success &= AddFolderToZipArchive(zipArchive,childPath,childRelativePath);
			}
			free(childPath);
			free(childRelativePath);

			if (!success)
			{
				closedir (pdir);
				return MZ_FALSE;
			}
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

			mz_bool success = mz_zip_writer_add_file(zipArchive, (const char*)childRelativePath, (const char*)childPath, 0, 0, MZ_BEST_COMPRESSION);			
			free(childPath);
			free(childRelativePath);

			if (!success)
			{
				closedir (pdir);
				return MZ_FALSE;
			}
		}
	}

	// finally, let's close the directory
	closedir (pdir);
	return MZ_TRUE;
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
      CW_USEDEFAULT, 0, 400, 200, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   SetTimer(hWnd, 1, 100, NULL);


   ProcessZipAction(hWnd, hInstance);

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
	case WM_CTLCOLORSTATIC:
		SetBkColor((HDC)wParam, RGB(255,255,255));
		return (LRESULT)GetStockObject(HOLLOW_BRUSH);
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code here...
		EndPaint(hWnd, &ps);
		break;
	case WM_TIMER:
		if ( wParam == 1 ) {
			if (!FolderCompressionInProgress)
			{
				DestroyWindow(hwndPB);
				DestroyWindow(hwndPBLabel);
			}
		}
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
