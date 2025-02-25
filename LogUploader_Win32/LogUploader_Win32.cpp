// LogUploader_Win32.cpp : Defines the entry point for the application.
//
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' " \
"version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma comment( lib, "Msimg32.lib")

#include "stdafx.h"
#include "LogUploader_Win32.h"
#include <shlobj.h>
#include <stdio.h>
#include <sys\timeb.h> 

#include <iostream>
#include <stdlib.h>

#include <wininet.h>

#include <sys/types.h>
#include <sys/stat.h>
#include "miniz.c"

#include "jsmn.c"

#include "dirent.h"

#ifndef WIN32
#include <unistd.h>
#endif

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

BOOL FolderCompressionInProgress = FALSE;
BOOL FolderCompressionSuccess = FALSE;
BOOL FolderFound = FALSE;
BOOL UploadInProgress = FALSE;
BOOL UploadSuccess = FALSE;
HWND hWnd;
HWND hwndPB;
HWND hwndLabelStatus;
HWND hwndLabel;
HWND hwndBTN_Yes;
HWND hwndBTN_No;
HWND hwndBTN_Close;
HWND hwndBTN_Retry;
RECT clientRect;
//char *targetZIPFile = NULL;
char *targetZIPFileSizeDesc = NULL;
char* uploadReference = NULL;
char* uploadStatus = NULL;
char* zipArchiveData = NULL;
long zipArchiveDataSize = 0;
int httpUploadResponseCode = 0;

HBITMAP bmpStep1;
HBITMAP bmpStep2;
HBITMAP bmpStep3;
HBITMAP bmpStep4;

int windowWidth = 400;
int windowHeight = 200;

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

void ProcessZipAction(HWND hwndParent, HINSTANCE hInstance);
//mz_bool ZipFolder(const char* folderToZip, const char *targetZIPFile);
mz_bool ZipFolder(const char* folderToZip);
mz_bool AddFolderToZipArchive(mz_zip_archive* zipArchive, const char *path, const char *relativePath);
DWORD WINAPI ThreadRoutine_Zip(LPVOID lpArg);
DWORD WINAPI ThreadRoutine_Upload(LPVOID lpArg);
void ProcessUploadAction(HWND hwndParent, HINSTANCE hInstance);
void SetStep(int stepNum);
void sleepcp(int milliseconds);

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
	if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
			strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
		return 0;
	}
	return -1;
}

void sleepcp(int milliseconds) // cross-platform sleep function
{
    #ifdef WIN32
    Sleep(milliseconds);
    #else
    usleep(milliseconds * 1000);
    #endif // win32
}

void ProcessUploadAction(HWND hwndParent, HINSTANCE hInstance)
{
	UploadInProgress = TRUE;
	SetTimer(hWnd, 2, 100, NULL);	

	//SetWindowText(hwndLabelStatus, _T("Step 3 of 3"));
	SetStep(3);

	RECT client_rectangle;
	GetClientRect(hwndParent, &client_rectangle);
	int width = client_rectangle.right - client_rectangle.left;
	int height = client_rectangle.bottom - client_rectangle.top;

	//HWND hwndPB;    // Handle of progress bar.
	
	hwndPB = CreateWindowEx(WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE,    
                                   PROGRESS_CLASS, NULL,   
                                   WS_CHILD | WS_VISIBLE,   
                                   (width-200)/2, ((height-10)/2)+7, 200,
                                   20, hwndParent, NULL,   
                                   hInstance, NULL);

	SendMessage(hwndPB, PBM_SETRANGE, 0, (LPARAM) MAKELPARAM(0, 100));
	SendMessage(hwndPB, PBM_SETPOS, 0, 0);
	//SendMessage(hwndPB, PBM_SETMARQUEE, 1, 0);

	uploadStatus = (char*)malloc(1024);
	uploadStatus[0] = NULL;
	sprintf(uploadStatus, "Sending logs to PrintNode...\n\n\n%s","");

	hwndLabel = CreateWindow(
                        TEXT("STATIC"),                   /*The name of the static control's class*/
                        TEXT("Sending logs to PrintNode..."),                  /*Label's Text*/
                        WS_CHILD | WS_VISIBLE | SS_CENTER,  /*Styles (continued)*/
                        0,                                /*X co-ordinates*/
                        (height/2)-20,                                /*Y co-ordinates*/
                        width,                               /*Width*/
                        100,                               /*Height*/
                        hwndParent,                             /*Parent HWND*/
                        NULL,              /*The Label's ID*/
                        hInstance,                        /*The HINSTANCE of your program*/ 
                        NULL);                            /*Parameters for main window*/

	hwndBTN_Close = CreateWindowEx(NULL, 
							L"BUTTON",
							L"Cancel",
							WS_TABSTOP|WS_VISIBLE|WS_CHILD,
							((clientRect.right-clientRect.left)/2)-50,
							((clientRect.bottom-clientRect.top)/2)+55,
							100,
							24,
							hWnd,
							(HMENU)IDC_CLOSE,
							hInst,
							NULL);

	DWORD ThreadId;
	CreateThread(NULL,0,ThreadRoutine_Upload,NULL,0,&ThreadId);
}

DWORD WINAPI ThreadRoutine_Upload(LPVOID lpArg)
{
	//char postDataHead[] = "-----------------------------7d82751e2bc0858\nContent-Disposition: form-data; name=\"uploadedfile\"; filename=\".printnodeArchive.zip\"\nContent-Type: application/binary\n\n";
	//char postDataTail[] = "\n-----------------------------7d82751e2bc0858--"; 
    //static TCHAR hdrs[] = L"Content-Type: multipart/form-data; boundary=---------------------------7d82751e2bc0858"; 
	httpUploadResponseCode = 0;
	BOOL success = TRUE;

    HINTERNET hSession = NULL;
	HINTERNET hConnect = NULL;
	HINTERNET hRequest = NULL;
	//char* buffer = NULL;

	hSession = InternetOpen(L"dev prototype - ignore",INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if(hSession==NULL)
    {
		//cout<<"Error: InternetOpen";  
		success = FALSE;
    }

	if (success)
	{
		//hConnect = InternetConnect(hSession, L"localhost",50302, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 1);
		hConnect = InternetConnect(hSession, L"client.printnode.com",INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 1);
		//hConnect = InternetConnect(hSession, L"www.shirtsinbulk.com",INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 1);

		if(hConnect==NULL)
		{
			//cout<<"Error: InternetConnect";  
			success = FALSE;
		}

		if (success)
		{
			hRequest = HttpOpenRequest(hConnect, L"POST",L"/logs/submit", HTTP_VERSION, NULL, NULL, INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID, 1);
			//hRequest = HttpOpenRequest(hConnect, L"POST",L"/Checkout/CheckoutLogin.aspx", HTTP_VERSION, NULL, NULL, INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID, 1);
			if(hRequest==NULL)
			{
				//cout<<"Error: HttpOpenRequest";  
				success = FALSE;
			}

			if (success)
			{
				//FILE *f = fopen(targetZIPFile, "rb");
				//if (f)
				if (TRUE)
				{
					long fileSize = zipArchiveDataSize;
					static wchar_t hdrs[1024];
					swprintf(hdrs,L"Content-Type: application/binary\r\nContent-Length: %d\r\n", fileSize);
					/*
					long fileSize = 0;
					fseek(f, 0x00, SEEK_END);
					fileSize = ftell(f);
					fseek(f, 0x00, SEEK_SET);

					buffer = (char*)malloc(fileSize);
					fread(buffer, sizeof(char), fileSize, f);
					fclose(f);

					static wchar_t hdrs[1024];
					swprintf(hdrs,L"Content-Type: application/binary\r\nContent-Length: %d\r\n", fileSize);
					*/

					// prepare headers
					success = HttpAddRequestHeaders(hRequest, hdrs, -1, HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD); 

					if (success)
					{
						// send the specified request to the HTTP server and allows chunked transfers
						INTERNET_BUFFERS bufferIn;

						DWORD bytesWritten;

						memset(&bufferIn, 0, sizeof(INTERNET_BUFFERS));

						bufferIn.dwStructSize  = sizeof(INTERNET_BUFFERS);
						bufferIn.dwBufferTotal = fileSize; //strlen(postDataHead) + fileSize + strlen(postDataTail);

						success = HttpSendRequestEx(hRequest, &bufferIn, NULL, HSR_INITIATE, 0);

						if (success)
						{
							// 1. stream header
							//success = InternetWriteFile(hRequest, (const void*)postDataHead, strlen(postDataHead), &bytesWritten);
						}

						if (success)
						{
							SendMessage(hwndPB, PBM_SETPOS, 0, 0);
							long maxChunkSize = 4096;
							long bytesSent = 0;
							char* bufferPtr = zipArchiveData;
							struct timeb start, end;
							ftime(&start);
							int curPercent = 0;
							while (bytesSent < fileSize)
							{
								//ftime(&start);
								long chunkSize = min(maxChunkSize,fileSize - bytesSent);
								// 2. stream attachment
								success = InternetWriteFile(hRequest, (const void*)bufferPtr, chunkSize, &bytesWritten);
								bytesSent += chunkSize;

								ftime(&end);
								double curTimeElapsedMs = max((1000.0 * (end.time - start.time) + (end.millitm - start.millitm)),1.0);
								//if (curTimeElapsedMs > 2000)
								//{
									double curTimeSec = curTimeElapsedMs/1000.0;
									double bytesPerSec = ((double)bytesSent / curTimeSec);
									//double kbytesPerSec = ((double)bytesSent / curTimeSec) / 1024.0;
									long remainingBytes = fileSize - bytesSent;
									int remainingSec = (int)((double)remainingBytes / bytesPerSec);
									int remainingMin = (int)((double)remainingSec / 60.0);
									int remainingHr = (int)((double)remainingMin / 60.0);
									if (remainingHr > 0) sprintf(uploadStatus, "Sending logs to PrintNode...\n\n\n      Time left: %i hour(s)      ",remainingHr);
									else if (remainingMin > 0) sprintf(uploadStatus, "Sending logs to PrintNode...\n\n\n      Time left: %i min(s)      ",remainingMin);
									else sprintf(uploadStatus, "Sending logs to PrintNode...\n\n\n      Time left: %i sec      ",remainingSec);

									wchar_t wtext[1024];
									mbstowcs(wtext, uploadStatus, strlen(uploadStatus)+1);
									SetWindowText(hwndLabel, wtext);
								//}

								int percent = (int)(((double)bytesSent / (double)fileSize) * 100.0);
								if (percent != curPercent)
								{
									SendMessage(hwndPB, PBM_SETPOS, percent, 0);
									curPercent = percent;
								}
								
								if (bytesSent < fileSize) bufferPtr += chunkSize;
							}
							SendMessage(hwndPB, PBM_SETPOS, 100, 0);
						}

						if (success)
						{
							// 3. stream tailer
							//success = InternetWriteFile(hRequest, (const void*)postDataTail, strlen(postDataTail), &bytesWritten);
						}

						if (success)
						{
							// end a HTTP request (initiated by HttpSendRequestEx)
							success = HttpEndRequest(hRequest, NULL, HSR_INITIATE, 0);
						}

						if(success)
						{
							uploadReference = (char*)malloc(4096);
							DWORD dwRead;
							success = InternetReadFile(hRequest, uploadReference, 4095, &dwRead);
							uploadReference[dwRead] = 0;

							LPVOID lpOutBuffer = NULL;
							DWORD dwSize = 0;
							while (!HttpQueryInfo(hRequest,HTTP_QUERY_STATUS_CODE,(LPVOID)lpOutBuffer,&dwSize,NULL))
							{
								DWORD dwError = GetLastError();
								if (dwError == ERROR_INSUFFICIENT_BUFFER)
								{
									lpOutBuffer = new wchar_t[dwSize];  
								}
								else
								{
									//fprintf(stderr, "HttpQueryInfo failed, error = %d (0x%x)\n",GetLastError(), GetLastError());
									break;
								}
							}
							wchar_t* outBuffer = (wchar_t*)lpOutBuffer;
							httpUploadResponseCode = _wtoi(outBuffer);
							delete[] lpOutBuffer;

							if (dwRead == 0) success = FALSE;
							else
							{
								char* pos = strstr(uploadReference,"<html");
								if (pos != NULL) success = FALSE;
								else
								{
									pos = strstr(uploadReference,"{");
									if (pos != NULL) success = FALSE;
								}
							}
							/*
							// USE IF RESPONSE NEEDS TO BE PARSED AS JSON TOKENS
							char* responseBuffer = (char*)malloc(4096);
							DWORD dwRead;
							InternetReadFile(hRequest, responseBuffer, 4096, &dwRead);
							responseBuffer[dwRead] = 0;
							jsmn_parser p;
							jsmntok_t tokens[128];

							jsmn_init(&p);
							int r = jsmn_parse(&p, responseBuffer, strlen(responseBuffer), tokens, sizeof(tokens)/sizeof(tokens[0]));
							if (r >= 0) {
								for (int i = 1; i < r; i++) {
									if (jsoneq(responseBuffer, &tokens[i], "Reference") == 0) {
										uploadReference = (char*)malloc(1024);
										uploadReference[0] = NULL;
										sprintf(uploadReference,"%.*s", tokens[i+1].end-tokens[i+1].start,responseBuffer + tokens[i+1].start);
										i++;
									}
								}
							}
							else success = FALSE;

							free(responseBuffer);
							*/							
						}
					}
				}
				else success = FALSE;
			}
		}
	}
	
	//if (buffer != NULL) free(buffer);

    //close any valid internet-handles
    if (hSession != NULL) InternetCloseHandle(hSession);
    if (hConnect != NULL) InternetCloseHandle(hConnect);
    if (hRequest != NULL) InternetCloseHandle(hRequest);

	UploadSuccess = success;
	UploadInProgress = FALSE;

	if (UploadSuccess && zipArchiveData != NULL) free(zipArchiveData);

	return NULL;
}

void ProcessZipAction(HWND hwndParent, HINSTANCE hInstance)
{
	FolderCompressionInProgress = TRUE;
	SetTimer(hWnd, 1, 100, NULL);

	RECT client_rectangle;
	GetClientRect(hwndParent, &client_rectangle);
	int width = client_rectangle.right - client_rectangle.left;
	int height = client_rectangle.bottom - client_rectangle.top;

	//HWND hwndPB;    // Handle of progress bar.
	
	hwndPB = CreateWindowEx(WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE,    
                                   PROGRESS_CLASS, NULL,   
                                   WS_CHILD | WS_VISIBLE|PBS_MARQUEE,   
                                   (width-200)/2, ((height-10)/2)+7, 200,
                                   20, hwndParent, NULL,   
                                   hInstance, NULL);

	//SendMessage(hwndPB, PBM_SETRANGE, 0, (LPARAM) MAKELPARAM(0, 599));
	//SendMessage(hwndPB, PBM_SETPOS, 0, 0);
	SendMessage(hwndPB, PBM_SETMARQUEE, 1, 0);

	hwndLabel = CreateWindow(
                        TEXT("STATIC"),                   /*The name of the static control's class*/
                        TEXT("Building log files.\n\n\nPlease wait ..."),                  /*Label's Text*/
                        WS_CHILD | WS_VISIBLE | SS_CENTER,  /*Styles (continued)*/
                        0,                                /*X co-ordinates*/
                        (height/2)-20,                                /*Y co-ordinates*/
                        width,                               /*Width*/
                        100,                               /*Height*/
                        hwndParent,                             /*Parent HWND*/
                        NULL,              /*The Label's ID*/
                        hInstance,                        /*The HINSTANCE of your program*/ 
                        NULL);                            /*Parameters for main window*/
	
	
	//SetBkColor

	DWORD ThreadId;
	CreateThread(NULL,0,ThreadRoutine_Zip,NULL,0,&ThreadId);
	
	
	//DestroyWindow(hwndPB);
}

DWORD WINAPI ThreadRoutine_Zip(LPVOID lpArg)
{
    WCHAR userFolder[MAX_PATH];
	if (SUCCEED(SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, userFolder))) { // Get user home folder
		char *folderToCompress = (char *)malloc( 1024 );
		wcstombs(folderToCompress, userFolder, 1024);
		strcat(folderToCompress, "\\.printnode");

		struct stat sb;
		if (stat(folderToCompress, &sb) == 0 && S_ISDIR(sb.st_mode)) // Check if target folder exists to zip up
		{
			FolderFound = TRUE;
			//targetZIPFile = (char *)malloc( 1024 );
			//wcstombs(targetZIPFile, userFolder, 1024);
			//strcat(targetZIPFile, "\\.printnodeArchive.zip");

			//FolderCompressionSuccess = ZipFolder(folderToCompress, targetZIPFile); // Zip up target folder
			FolderCompressionSuccess = ZipFolder(folderToCompress); // Zip up target folder
			if (FolderCompressionSuccess)
			{
				long fileSizeBytes = zipArchiveDataSize;
				double fileSizeKB = (double)fileSizeBytes / 1024.0;
				targetZIPFileSizeDesc = (char *)malloc( 255 );
				targetZIPFileSizeDesc[0] = NULL;
				if (fileSizeBytes < 1024) sprintf(targetZIPFileSizeDesc, "%i bytes", fileSizeBytes);
				else if (fileSizeKB < 1024) sprintf(targetZIPFileSizeDesc, "%.0f KB", fileSizeKB);
				else
				{
					double fileSizeMB = fileSizeKB / 1024.0;
					sprintf(targetZIPFileSizeDesc, "%.1f MB", fileSizeMB);
				}
				
				/*
				if (stat(targetZIPFile, &sb) == 0)
				{
					long fileSizeBytes = sb.st_size;
					double fileSizeKB = (double)fileSizeBytes / 1024.0;
					targetZIPFileSizeDesc = (char *)malloc( 255 );
					targetZIPFileSizeDesc[0] = NULL;
					if (fileSizeBytes < 1024) sprintf(targetZIPFileSizeDesc, "%i bytes", fileSizeBytes);
					else if (fileSizeKB < 1024) sprintf(targetZIPFileSizeDesc, "%.0f KB", fileSizeKB);
					else
					{
						double fileSizeMB = fileSizeKB / 1024.0;
						sprintf(targetZIPFileSizeDesc, "%.1f MB", fileSizeMB);
					}
				}
				*/
			}
			//free(targetZIPFile);
		}
		else
		{
			// folder does not exist
		}
		free(folderToCompress);
	}
	Sleep(3000);
	FolderCompressionInProgress = FALSE;

	return NULL;
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{

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

//mz_bool ZipFolder(const char* folderToZip, const char *targetZIPFile)
mz_bool ZipFolder(const char* folderToZip)
{
	//remove(targetZIPFile);

	mz_zip_archive zip;
	memset(&zip, 0, sizeof(zip));

	mz_bool success = MZ_TRUE;

	//success &= mz_zip_writer_init_file(&zip, targetZIPFile, 65537);
	success &= mz_zip_writer_init_heap(&zip, 65537, 65537);
	if (!success)
	{
		//print_error("Failed creating zip archive \"%s\" (1)!\n", pZip_filename);
		return success;
	}

	

	success &= AddFolderToZipArchive(&zip, folderToZip,"");
	if (!success)
	{
		mz_zip_writer_end(&zip);
		//remove(targetZIPFile);
		//print_error("Failed creating zip archive \"%s\" (2)!\n", pZip_filename);
		return success;
	}

	success &= mz_zip_writer_finalize_archive(&zip);
	if (!success)
	{
		mz_zip_writer_end(&zip);
		//remove(targetZIPFile);
		//print_error("Failed creating zip archive \"%s\" (3)!\n", pZip_filename);
		return success;
	}

	zipArchiveDataSize = zip.m_pState->m_mem_size;
	zipArchiveData = (char*)malloc(zipArchiveDataSize);
	memcpy(zipArchiveData, zip.m_pState->m_pMem, zipArchiveDataSize);
	/*
	FILE *f = fopen(targetZIPFile, "wb");
	if (f)
	{
		fwrite(zip.m_pState->m_pMem, 1, zip.m_pState->m_mem_size, f);
		fclose(f);
	}
	*/
	//zip.m_pState->

	success &= mz_zip_writer_end(&zip);
	if (!success)
	{
		//remove(targetZIPFile);
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
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= 0;//MAKEINTRESOURCE(IDC_LOGUPLOADER_WIN32);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_ICON1));

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
	
   hInst = hInstance; // Store instance handle in our global variable

   const int windowXPos = (GetSystemMetrics(SM_CXSCREEN) - windowWidth) / 2;
   const int windowYPos = (GetSystemMetrics(SM_CYSCREEN) - windowHeight) / 2;

   hWnd = CreateWindow(szWindowClass, L"PrintNode Crash Reporter v1.0", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
      windowXPos, windowYPos, windowWidth, windowHeight, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   GetClientRect(hWnd, &clientRect);
   /*
	hwndLabelStatus = CreateWindow(
						TEXT("STATIC"),        
						TEXT("Step 1 of 3"),   
						WS_CHILD | WS_VISIBLE | SS_CENTER,
						0,                                
						10,                               
						clientRect.right-clientRect.left, 
						25,                               
						hWnd,                             
						NULL,  
						hInstance,
						NULL); 
						*/
	/*
	CreateWindow(
		TEXT("STATIC"),               
		TEXT("Step 1 of 3"),          
		WS_CHILD | WS_VISIBLE | SS_LEFT | SS_GRAYFRAME,  
		5,                                
		5,                                
		clientRect.right-clientRect.left-10,
		28,                               
		hWnd,                             
		NULL,
		hInstance,
		NULL);  
*/
	//HWND hImage = LoadImage(NULL, file, IMAGE_BITMAP, w, h, LR_LOADFROMFILE);
	//SendMessage(hwnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hImage);

   HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
   SetClassLongPtr(hWnd, GCLP_HBRBACKGROUND, (LONG)brush);

   //hbSymbol = LoadBitmap( GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_BITMAP1) );
   
   
	HWND hwndBitmapSteps = CreateWindowEx(
               0, 
               TEXT("STATIC"), 
               NULL, 
               WS_CHILD|WS_VISIBLE|SS_BITMAP,
               ((clientRect.right-clientRect.left)-182)/2, 30, 0, 0,
               hWnd, 
               (HMENU)IDW_STEPS, 
               hInstance,
               NULL
               );

	bmpStep1 = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_BITMAP1));
	bmpStep2 = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_BITMAP2));
	bmpStep3 = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_BITMAP3));
	bmpStep4 = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_BITMAP4));
	SetStep(1);

   ProcessZipAction(hWnd, hInstance);

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

void SetStep(int stepNum)
{
	if (stepNum == 1) SendDlgItemMessage(hWnd, IDW_STEPS, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)bmpStep1);
	else if (stepNum == 2) SendDlgItemMessage(hWnd, IDW_STEPS, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)bmpStep2);
	else if (stepNum == 3) SendDlgItemMessage(hWnd, IDW_STEPS, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)bmpStep3);
	else if (stepNum == 4) SendDlgItemMessage(hWnd, IDW_STEPS, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)bmpStep4);
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
	case WM_CREATE:
		{

		
		}
		break;
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
		case IDC_CLOSE:
		case IDC_NO:
			PostQuitMessage(1);
			break;
		case IDC_YES:
			DestroyWindow(hwndBTN_Yes);
			DestroyWindow(hwndBTN_No);
			DestroyWindow(hwndLabel);
			ProcessUploadAction(hWnd, hInst);
			break;
		case IDC_RETRY:
			DestroyWindow(hwndBTN_Close);
			DestroyWindow(hwndBTN_Retry);
			DestroyWindow(hwndLabel);
			ProcessUploadAction(hWnd, hInst);
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
				KillTimer(hWnd, 1);
				if (FolderCompressionSuccess)
				{
					DestroyWindow(hwndPB);
					DestroyWindow(hwndLabel);

					//SetWindowText(hwndLabelStatus, _T("Step 2 of 3"));
					SetStep(2);

					char msg[1024];
					sprintf(msg,"Found %s log files.\nWould you like to send these to PrintNode?", targetZIPFileSizeDesc);

					wchar_t wtext[1024];
					mbstowcs(wtext, msg, strlen(msg)+1);

					int clientWidth = clientRect.right-clientRect.left;
					int clientHeight = clientRect.bottom-clientRect.top;

					hwndLabel = CreateWindow(
                        TEXT("STATIC"),                   /*The name of the static control's class*/
                        wtext,                  /*Label's Text*/
                        WS_CHILD | WS_VISIBLE | SS_CENTER,  /*Styles (continued)*/
                        0,                                /*X co-ordinates*/
						(clientHeight/2)-15,                                /*Y co-ordinates*/
						clientWidth,                               /*Width*/
                        50,                               /*Height*/
                        hWnd,                             /*Parent HWND*/
                        NULL,              /*The Label's ID*/
                        hInst,                        /*The HINSTANCE of your program*/ 
                        NULL);                            /*Parameters for main window*/

					hwndBTN_Yes = CreateWindowEx(NULL, 
							L"BUTTON",
							L"Yes",
							WS_TABSTOP|WS_VISIBLE|WS_CHILD,
							((clientRect.right-clientRect.left)/2)+2,
							((clientRect.bottom-clientRect.top)/2)+40,
							100,
							24,
							hWnd,
							(HMENU)IDC_YES,
							hInst,
							NULL);

				   hwndBTN_No = CreateWindowEx(NULL, 
							L"BUTTON",
							L"Not now",
							WS_TABSTOP|WS_VISIBLE|WS_CHILD,
							((clientRect.right-clientRect.left)/2)-102,
							((clientRect.bottom-clientRect.top)/2)+40,
							100,
							24,
							hWnd,
							(HMENU)IDC_NO,
							hInst,
							NULL);

				   SendMessage(hwndBTN_Yes, BM_SETSTATE, TRUE, 0) ;

					/*
					if (MessageBox(hWnd, wtext, L"Upload Confirmation", MB_YESNO | MB_ICONQUESTION) == IDYES)
					{
						DestroyWindow(hwndPB);
						DestroyWindow(hwndLabel);
						ProcessUploadAction(hWnd, hInst);
					}
					else PostQuitMessage(1);
					*/
				}
				else
				{
					DestroyWindow(hwndPB);
					DestroyWindow(hwndLabel);

					int clientWidth = clientRect.right-clientRect.left;
					int clientHeight = clientRect.bottom-clientRect.top;

					CreateWindow(
                        TEXT("STATIC"),                   /*The name of the static control's class*/
                        L"No logs were found.",                  /*Label's Text*/
                        WS_CHILD | WS_VISIBLE | SS_CENTER,  /*Styles (continued)*/
                        0,                                /*X co-ordinates*/
						(clientHeight/2)-7,                                /*Y co-ordinates*/
						clientWidth,                               /*Width*/
                        25,                               /*Height*/
                        hWnd,                             /*Parent HWND*/
                        NULL,              /*The Label's ID*/
                        hInst,                        /*The HINSTANCE of your program*/ 
                        NULL);                            /*Parameters for main window*/
					
					CreateWindowEx(NULL, 
							L"BUTTON",
							L"Exit",
							WS_TABSTOP|WS_VISIBLE|WS_CHILD,
							((clientRect.right-clientRect.left)/2)-50,
							((clientRect.bottom-clientRect.top)/2)+40,
							100,
							24,
							hWnd,
							(HMENU)IDC_CLOSE,
							hInst,
							NULL);
				}
			}
		}
		else if (wParam == 2) {
			if (!UploadInProgress) {
				KillTimer(hWnd, 2);
				SendMessage(hwndPB, PBM_SETPOS, 100, 0);
				if (UploadSuccess)
				{
					SetStep(4);

					DestroyWindow(hwndBTN_Close);
					DestroyWindow(hwndPB);
					DestroyWindow(hwndLabel);
					
					char msg[4096];
					wchar_t wtext[4096];
					sprintf(msg,"Thank you! Logs sent successfully.\nYour reference number is %s", uploadReference);
					mbstowcs(wtext, msg, strlen(msg)+1);

					int clientWidth = clientRect.right-clientRect.left;
					int clientHeight = clientRect.bottom-clientRect.top;

					hwndLabel = CreateWindow(
                        TEXT("STATIC"),                 
                        wtext,                  
                        WS_CHILD | WS_VISIBLE | SS_CENTER,  
                        0,                                
						(clientHeight/2)-15,              
						clientWidth,                      
                        50,                               
                        hWnd,                             
                        NULL,
                        hInst,
                        NULL);
					
					CreateWindowEx(NULL, 
							L"BUTTON",
							L"Exit",
							WS_TABSTOP|WS_VISIBLE|WS_CHILD,
							((clientRect.right-clientRect.left)/2)-50,
							((clientRect.bottom-clientRect.top)/2)+40,
							100,
							24,
							hWnd,
							(HMENU)IDC_CLOSE,
							hInst,
							NULL);
					//MessageBox(hWnd, wtext, L"Upload Complete", MB_OK);
					//PostQuitMessage(1);
				}
				else
				{
					DestroyWindow(hwndBTN_Close);
					DestroyWindow(hwndPB);
					DestroyWindow(hwndLabel);

					int clientWidth = clientRect.right-clientRect.left;
					int clientHeight = clientRect.bottom-clientRect.top;

					char msg[4096];
					wchar_t wtext[4096];
					if (httpUploadResponseCode == 0) sprintf(msg,"We're sorry, but there was a problem during your upload.\nThe connection was interrupted.", httpUploadResponseCode);
					else sprintf(msg,"We're sorry, but there was a problem during your upload.\nServer responded with status code %i.", httpUploadResponseCode);
					mbstowcs(wtext, msg, strlen(msg)+1);

					// httpUploadResponseCode

					hwndLabel = CreateWindow(
                        TEXT("STATIC"),                   /*The name of the static control's class*/
                        wtext,
                        WS_CHILD | WS_VISIBLE | SS_CENTER,  /*Styles (continued)*/
                        0,                                /*X co-ordinates*/
						(clientHeight/2)-15,                                /*Y co-ordinates*/
						clientWidth,                               /*Width*/
                        50,                               /*Height*/
                        hWnd,                             /*Parent HWND*/
                        NULL,              /*The Label's ID*/
                        hInst,                        /*The HINSTANCE of your program*/ 
                        NULL);

					hwndBTN_Retry = CreateWindowEx(NULL, 
							L"BUTTON",
							L"Retry",
							WS_TABSTOP|WS_VISIBLE|WS_CHILD,
							((clientRect.right-clientRect.left)/2)+2,
							((clientRect.bottom-clientRect.top)/2)+40,
							100,
							24,
							hWnd,
							(HMENU)IDC_RETRY,
							hInst,
							NULL);

					hwndBTN_Close = CreateWindowEx(NULL, 
							L"BUTTON",
							L"Exit",
							WS_TABSTOP|WS_VISIBLE|WS_CHILD,
							((clientRect.right-clientRect.left)/2)-102,
							((clientRect.bottom-clientRect.top)/2)+40,
							100,
							24,
							hWnd,
							(HMENU)IDC_CLOSE,
							hInst,
							NULL);
				}
			}
			else
			{
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
