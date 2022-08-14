#include "pch.h"

#include <wil/com.h>
#include <wchar.h>
#include <sstream>
#include <regex>
#include "WebView2.h"
#include "WebView2EnvironmentOptions.h"


namespace winrt {
	using namespace Windows::Foundation;
}

static wil::com_ptr<ICoreWebView2Controller> webviewController;
static winrt::com_ptr<ICoreWebView2> webview;


class MediaFileStream : public winrt::implements<MediaFileStream, IStream> {
public:
	MediaFileStream(LPCWSTR path) {
		_hFile = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		GetFileSizeEx(_hFile, (PLARGE_INTEGER)&_size);
	}

	~MediaFileStream() {
		CloseHandle(_hFile);
	}

	virtual HRESULT __stdcall Read(void* pv, ULONG cb, ULONG* pcbRead) override
	{
		auto bytesRemaining = _size.QuadPart - _bytesRead.QuadPart;
		auto bytesToRead = cb < bytesRemaining ? cb : bytesRemaining;
		BOOL rc = ReadFile(_hFile, pv, bytesToRead, pcbRead, NULL);
		_bytesRead.QuadPart += *pcbRead;
		return (rc) ? S_OK : HRESULT_FROM_WIN32(GetLastError());
	}
	virtual HRESULT __stdcall Write(const void* pv, ULONG cb, ULONG* pcbWritten) override
	{
		return E_NOTIMPL;
	}
	virtual HRESULT __stdcall Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition) override
	{
		DWORD dwMoveMethod;

		switch (dwOrigin)
		{
		case STREAM_SEEK_SET:
			dwMoveMethod = FILE_BEGIN;
			break;
		case STREAM_SEEK_CUR:
			dwMoveMethod = FILE_CURRENT;
			break;
		case STREAM_SEEK_END:
			dwMoveMethod = FILE_END;
			break;
		default:
			return STG_E_INVALIDFUNCTION;
			break;
		}

		if (SetFilePointerEx(_hFile, dlibMove, (PLARGE_INTEGER)plibNewPosition, dwMoveMethod) == 0)
			return HRESULT_FROM_WIN32(GetLastError());
		_bytesRead.QuadPart = 0;
		return S_OK;
	}
	virtual HRESULT __stdcall SetSize(ULARGE_INTEGER libNewSize) override
	{
		_size = libNewSize;
		return S_OK;
	}
	virtual HRESULT __stdcall CopyTo(IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten) override
	{
		return E_NOTIMPL;
	}
	virtual HRESULT __stdcall Commit(DWORD grfCommitFlags) override
	{
		return E_NOTIMPL;
	}
	virtual HRESULT __stdcall Revert(void) override
	{
		return E_NOTIMPL;
	}
	virtual HRESULT __stdcall LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) override
	{
		return E_NOTIMPL;
	}
	virtual HRESULT __stdcall UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) override
	{
		return E_NOTIMPL;
	}
	virtual HRESULT __stdcall Stat(STATSTG* pstatstg, DWORD grfStatFlag) override
	{
		pstatstg->cbSize = _size;
		return S_OK;
	}
	virtual HRESULT __stdcall Clone(IStream** ppstm) override
	{
		return E_NOTIMPL;
	}

private:
	HANDLE _hFile = NULL;
	ULARGE_INTEGER _size{};
	ULARGE_INTEGER _bytesRead{};
};

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_SIZE:
		if (webviewController != nullptr) {
			RECT bounds;
			GetClientRect(hWnd, &bounds);
			webviewController->put_Bounds(bounds);
		};
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	}

	return 0;
}


int __stdcall wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);
	WNDCLASSEX wcex{};

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = L"DesktopApp";
	wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

	RegisterClassEx(&wcex);

	HWND hWnd = CreateWindow(
		L"DesktopApp",
		L"WebView2Video",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		1200, 900,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
	options->put_AdditionalBrowserArguments(L"--ignore-certificate-errors");
	CreateCoreWebView2EnvironmentWithOptions(
		nullptr,
		nullptr,
		options.Get(),
		Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
			[hWnd](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
				env->CreateCoreWebView2Controller(hWnd, Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
					[hWnd, env](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
						if (controller != nullptr) {
							webviewController = controller;
							webviewController->get_CoreWebView2(webview.put());
						}

						ICoreWebView2Settings* Settings;
						webview->get_Settings(&Settings);
						Settings->put_IsScriptEnabled(TRUE);
						Settings->put_AreDefaultScriptDialogsEnabled(TRUE);
						Settings->put_IsWebMessageEnabled(TRUE);

						RECT bounds;
						GetClientRect(hWnd, &bounds);
						webviewController->put_Bounds(bounds);

						std::wstringstream ss;
						ss << L"<html><body>";
						ss << L"<div>";
						ss << L"<p>SetVirtualHostNameToFolderMapping 200 response</p>";
						ss << L"<p>Entire video loaded, cannot seek</p>";
						ss << L"<video height=\"240\" controls muted=\"muted\" src=\"http://video.example/local-video.mp4\"></video>";
						ss << L"</div>";
						ss << L"<div>";
						ss << L"<p>WebResourceRequested 200 response with content headers</p>";
						ss << L"<p>Entire video loaded, can seek</p>";
						ss << L"<video height=\"240\" controls muted=\"muted\" src=\"http://localhost:8888/local-video.mp4\"></video>";
						ss << L"</div>";
						ss << L"<div>";
						ss << L"<p>WebResourceRequested 206 response with content headers</p>";
						ss << L"<p>1MB of video loaded, cannot play video</p>";
						ss << L"<video height=\"240\" controls muted=\"muted\" src = \"http://localhost:8888/local-video-partial.mp4\"></video>";
						ss << L"</div>";
						ss << L"</body></html>";

						webview->NavigateToString(ss.str().c_str());
						webview->OpenDevToolsWindow();

						auto webView3 = webview.try_as<ICoreWebView2_3>();
						webView3->SetVirtualHostNameToFolderMapping(
							L"video.example",
							L".",
							COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);

						webview->AddWebResourceRequestedFilter(
							L"http://localhost:8888/local-video*.mp4", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
						webview->add_WebResourceRequested(
							Microsoft::WRL::Callback<ICoreWebView2WebResourceRequestedEventHandler>(
								[](
									ICoreWebView2* sender,
									ICoreWebView2WebResourceRequestedEventArgs* args) {
										wil::com_ptr<ICoreWebView2WebResourceRequest> request;
										args->get_Request(&request);
										wil::unique_cotaskmem_string uri;
										request->get_Uri(&uri);
										auto partial = std::wstring{ uri.get() }.find(L"partial") != std::string::npos;
										wil::com_ptr<ICoreWebView2HttpRequestHeaders> requestHeaders;
										request->get_Headers(&requestHeaders);
										wil::unique_cotaskmem_string range;
										requestHeaders->GetHeader(L"Range", &range);
										std::wcmatch matches{};
										auto results = std::regex_match(range.get(), matches, std::wregex(L"bytes=([0-9]*)-"));
										auto streamStart = std::stoul(matches[1].str());


										auto stream = winrt::make<MediaFileStream>(L".\\local-video.mp4");
										STATSTG stat{};
										stream->Stat(&stat, NULL);
										auto fullStreamSize = stat.cbSize.QuadPart;
										LARGE_INTEGER seek{ streamStart };
										ULARGE_INTEGER newPosition{ 0 };
										stream->Seek(seek, STREAM_SEEK_SET, &newPosition);
										auto remainingSize = fullStreamSize - newPosition.QuadPart;
										auto partialSize = remainingSize < 1000000 ? remainingSize : 1000000;
										ULARGE_INTEGER chunkSize{ partial ? partialSize : remainingSize };
										if (partial) {
											stream->SetSize(chunkSize);
										}

										wil::com_ptr<ICoreWebView2Environment> environment;
										wil::com_ptr<ICoreWebView2_2> webview2;
										webview->QueryInterface(IID_PPV_ARGS(&webview2));
										webview2->get_Environment(&environment);
										wil::com_ptr<ICoreWebView2WebResourceResponse> response;
										environment->CreateWebResourceResponse(
											stream.get(),
											partial ? 206 : 200,
											L"OK",
											L"Content-Type: video/mp4",
											&response);
										wil::com_ptr<ICoreWebView2HttpResponseHeaders> responseHeaders;
										response->get_Headers(&responseHeaders);

										std::wstringstream ss;
										ss << L"bytes " << newPosition.QuadPart << "-" << newPosition.QuadPart + chunkSize.QuadPart - 1 << L"/" << fullStreamSize;
										responseHeaders->AppendHeader(L"Content-Range", ss.str().c_str());
										responseHeaders->AppendHeader(L"Content-Length", std::to_wstring(chunkSize.QuadPart).c_str());
										responseHeaders->AppendHeader(L"Accept-Ranges", L"bytes");

										args->put_Response(response.get());
										return S_OK;
								}).Get(), nullptr);

						return S_OK;
					}).Get());
				return S_OK;
			}).Get());

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;

}
