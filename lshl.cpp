#define _WIN32_WINNT 0x500
#define WINVER _WIN32_WINNT

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <windows.h>
#include <memory>
#include <iostream>

namespace lshl
{
static const size_t MAX_PATH_BUF_LEN = 32768;
static const size_t MAX_PATH_STR_LEN = MAX_PATH_BUF_LEN - 1;

int ProcessDir(const wchar_t* pszSrcDir, const size_t nLenSrcDir);
int Run(const wchar_t* pszSrcDirArg);
} //end namespace lshl

int wmain(int argc, wchar_t* argv[])
{
	const wchar_t* pszSrcDirArg = NULL;

	if (argc == 1)
		;
	else if (argc == 2)
		pszSrcDirArg = argv[1];
	else
		return -1;
	
	wchar_t pszCurDir[lshl::MAX_PATH_BUF_LEN] = L"";

	if( !pszSrcDirArg ||
		(0 == wcscmp(pszSrcDirArg, L".")) ||
		(0 == wcscmp(pszSrcDirArg, L".\\")) ||
		(0 == wcscmp(pszSrcDirArg, L"./")))
	{
		const DWORD nRes = GetCurrentDirectoryW(lshl::MAX_PATH_BUF_LEN, pszCurDir);

		if((nRes == 0) || (nRes > lshl::MAX_PATH_BUF_LEN))
			return -3;

		pszSrcDirArg = pszCurDir;
	}

	return lshl::Run(pszSrcDirArg);
}

namespace lshl
{
int Run( const wchar_t* pszSrcDirArg )
{
	size_t nLenSrcDir = wcslen(pszSrcDirArg);

	if (0 == nLenSrcDir)
		return -3;

	const DWORD dwAttribs = GetFileAttributesW(pszSrcDirArg);

	if ((dwAttribs != INVALID_FILE_ATTRIBUTES) && (dwAttribs & FILE_ATTRIBUTE_DIRECTORY))
		;
	else
		return -2;

	wchar_t pszSrcDir[MAX_PATH_STR_LEN + 1];

	if ((nLenSrcDir >= 4) && (0 == wcsncmp(pszSrcDirArg, L"\\\\?\\", 4)))
	{
		if (nLenSrcDir > MAX_PATH_STR_LEN)
			return -3;
		
		wcsncpy(pszSrcDir, pszSrcDirArg, nLenSrcDir);
	}
	else
	{
		if (nLenSrcDir > (MAX_PATH_STR_LEN - 4))
			return -3;
		
		wcsncpy(pszSrcDir, L"\\\\?\\", 4);
		wcsncpy(pszSrcDir + 4, pszSrcDirArg, nLenSrcDir);

		nLenSrcDir += 4;
	}

	return ProcessDir(pszSrcDir, nLenSrcDir);
}

bool OnFileQueryError(const wchar_t* pszFile)
{
	std::wcerr << L"An error occurred while processing file '" << pszFile << L"'; GetLastError() returned " << GetLastError() << L". Do you want to continue? (Y/N)" << std::endl;

	wint_t cUserResponse = L' ';

	while (true)
	{
		cUserResponse = _getwch();

		if (tolower(cUserResponse) == 'y')
			return true;
		
		if (tolower(cUserResponse) != 'n')
			std::wcout << L"Do you want to continue? (Y/N)" << std::endl;
		else
			break;
	}

	return false;
}

int ProcessDir(const wchar_t* pszSrcDir, const size_t nLenSrcDir)
{
	size_t nLenPath = nLenSrcDir;

	std::unique_ptr<wchar_t> pPath(new wchar_t[MAX_PATH_STR_LEN + 1]);

	wchar_t* pszPath = pPath.get();

	wcsncpy(pszPath, pszSrcDir, nLenPath);
	pszPath[nLenPath] = L'\0';

	if ((nLenPath > 0) && (pszPath[nLenPath - 1] == L'\\'))
	{
		;
	}
	else
	{
		if (nLenPath > (MAX_PATH_STR_LEN - 1))
			return -11;

		pszPath[nLenPath++] = L'\\';
		pszPath[nLenPath] = L'\0';
	}

	if (nLenPath > (MAX_PATH_STR_LEN - 1))
		return -11;

	const size_t nLenTrailPathSep = nLenPath;

	pszPath[nLenPath++] = L'*';
	pszPath[nLenPath] = L'\0';

	bool bBadPath = false;

	WIN32_FIND_DATAW wfd = { 0 };
	BY_HANDLE_FILE_INFORMATION fi = { 0 };
	HANDLE hFirstFile = FindFirstFileW(pszPath, &wfd);

	if (hFirstFile == INVALID_HANDLE_VALUE)
		return (GetLastError() == ERROR_FILE_NOT_FOUND) ? 0 : -12;

	do
	{
		bBadPath = false;

		if ((nLenTrailPathSep + MAX_PATH - 1) <= MAX_PATH_STR_LEN)
		{
			wcsncpy(pszPath + nLenTrailPathSep, wfd.cFileName, MAX_PATH - 1);
			nLenPath = nLenTrailPathSep + min(wcslen(wfd.cFileName), MAX_PATH - 1);
			pszPath[nLenPath] = L'\0';
		}
		else
		{
			bBadPath = true;
		}

		if (wfd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
		{
			if (!bBadPath)
				std::wcerr << L"File is a reparse point, skipping: " << pszPath << std::endl;
		}
		else if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if ((wcscmp(wfd.cFileName, L".") == 0) || (wcscmp(wfd.cFileName, L"..") == 0))
			{
				;
			}
			else
			{
				if (bBadPath)
					return -13;

				const int nRetInner = ProcessDir(pszPath, nLenPath);

				if (0 != nRetInner)
					return nRetInner;
			}
		}
		else
		{
			if (bBadPath)
				return -13;
			
			HANDLE hFile = CreateFileW(pszPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);

			if (INVALID_HANDLE_VALUE == hFile)
			{
				if(!OnFileQueryError(pszPath))
					return -14;
			}
			else if (!GetFileInformationByHandle(hFile, &fi))
			{
				CloseHandle(hFile);

				if(!OnFileQueryError(pszPath))
					return -15;
			}
			else
			{
				CloseHandle(hFile);
				hFile = NULL;

				if (fi.nNumberOfLinks > 1)
				{
					if (!bBadPath)
						std::wcout << pszPath << std::endl;
				}
			}
		}
	} while (FindNextFileW(hFirstFile, &wfd) != 0);
	
	FindClose(hFirstFile);

	return 0;
}
} //end namespace lshl
