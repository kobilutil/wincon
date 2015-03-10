#pragma once

#include "utils.h"
#include <vector>


class ConsoleHelper :
	NonCopyable
{
public:
	ConsoleHelper() = default;
	~ConsoleHelper() = default;

	void ReOpenHandles();
	bool RefreshInfo();

	size const& CellSize() const			{ return _cellSize; }
	size const& BufferSize() const			{ return _bufferSize; }
	rectangle const& BufferView() const		{ return _bufferView; }
	point const& CaretPos() const			{ return _caretPos; }
	size LargestViewSize() const;

	bool BufferSize(size const& v);
	bool BufferView(rectangle const& v);

	bool Resize(size const& v);
	bool Resize(rectangle const& v);

	DWORD InputMode() const;

	point MapPixelToCell(point const& p);
	point MapCellToPixel(point const& p);

	void Pause();
	void Resume();

	bool WriteInput(::INPUT_RECORD const& inp);
	bool WriteInput(LPCWSTR str);
	bool ReadOutput(std::vector<CHAR_INFO>& buffer, rectangle const& region);

	static bool IsRegularConsoleWindow(HWND hWnd);
	static std::vector<DWORD> GetProcessList();
	static DWORD GetRealThreadId();
	static void InstallDefaultCtrlHandler();

private:
	void InternalPauseResume(bool pause);

	scoped_handle	_hConIn;
	scoped_handle	_hConOut;

	size		_cellSize;

	size		_bufferSize;
	rectangle	_bufferView;
	point		_caretPos;
};
