module;
#include <fstream>
#include <memory>
#include <iostream>
#include <comdef.h>
#include <ctime>
export module AudioUtils;

using ErrorStr = std::shared_ptr<const char>;

inline const ErrorStr GetResultString(HRESULT result)
{
	// Should've probably just used a string for this whole thing...
	auto msg = "Assertion failed, result was: ";
	auto msgBuf = new char[256];

	char ch;
	int i = 0;
	while (ch = msg[i])
	{
		msgBuf[i] = ch;
		++i;
	}

	msgBuf[i++] = '"';
	_com_error err(result);
	auto errStr = err.ErrorMessage();
	constexpr int errCStrBufSize = 128;
	char errCStrBuf[errCStrBufSize];
	size_t convertCount;
	wcstombs_s(&convertCount, errCStrBuf, errStr, errCStrBufSize);
	int j = 0;
	while (ch = errCStrBuf[j])
	{
		if (j > convertCount)
			break;
		msgBuf[i] = ch;
		++j;
		++i;
	}
	msgBuf[i++] = '"';
	msgBuf[i++] = 0;

	return ErrorStr(msgBuf);
}

inline void WriteErrorFile(const ErrorStr& err)
{
	time_t t = time(0);
	constexpr int tStrSize = 50;
	char tStr[tStrSize];
	ctime_s(tStr, tStrSize, &t);
	auto tStrLen = strlen(tStr);
	tStr[tStrLen - 1] = 0;
	std::ofstream errFile("./error.txt", std::ios::in | std::ios::out | std::ios::app);
	errFile << tStr << " | " << err.get() << std::endl;
}

export inline void DebugAssertResult(HRESULT result)
{
	#if _DEBUG
	if (result != S_OK)
	{
		const auto err = GetResultString(result);
		WriteErrorFile(err);
		std::cout << "Encountered error. Result written to error.txt";
	}
	#endif
}