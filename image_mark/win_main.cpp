#include <windows.h>
#include <assert.h>
#include <shlobj_core.h>
#include <string.h>

#include <vector>
#include <string>
#include <fstream>
#include <filesystem>

#include <d3d9.h>
#pragma comment(lib,"d3d9")

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"


typedef struct file_path
{
	bool valid;
	std::string path;
	std::vector<std::string> path_list;
	file_path() : valid(true) {}
}file_path;

typedef struct image_info
{
	bool valid;
	int width, height, channel;
	unsigned char* buffer;
	std::string path;
	LPDIRECT3DTEXTURE9 texture;
	image_info() :valid(true) {}
}image_info;

//全局变量
const char* g_name = "image_mark";
HWND g_hwnd;
IDirect3D9* g_idrect3d9;
LPDIRECT3DDEVICE9 g_direct3ddevice9;
D3DPRESENT_PARAMETERS g_present;
std::vector<image_info> g_image;
std::vector<file_path> g_path;

//前向声明
void initialize_wnd() noexcept;
void initialize_d3d9() noexcept;
void initialize_imgui() noexcept;
void message_handle() noexcept;
void render_handle() noexcept;
void reset_device() noexcept;
void clear_setting() noexcept;
void select_path() noexcept;
void show_file() noexcept;
void initialize_image(const char* path, const char* str) noexcept;
void set_image_texture() noexcept;
void render_image() noexcept;
void clear_image_texture() noexcept;
std::string utf8_to_string(const std::string& str) noexcept;
std::string string_to_utf8(const std::string& str) noexcept;
void traversing_files(const char* str) noexcept;
LRESULT _stdcall window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

int main(int argc, char* argv[])
{
	initialize_wnd();
	initialize_d3d9();
	initialize_imgui();
	message_handle();
	clear_setting();
	return 0;
}

//初始化窗口
void initialize_wnd() noexcept
{
	WNDCLASSEXA wc =
	{
		sizeof(WNDCLASSEX),CS_CLASSDC,window_proc,0L,0L,GetModuleHandle(NULL),
		NULL,NULL,NULL,NULL,g_name,NULL
	};
	assert(RegisterClassExA(&wc));

	g_hwnd = CreateWindow(g_name, g_name, WS_OVERLAPPEDWINDOW,
		100, 100, 800, 500, NULL, NULL, wc.hInstance, NULL);
	assert(g_hwnd);

	ShowWindow(g_hwnd, SW_SHOW);
	UpdateWindow(g_hwnd);
}

//初始化d3d9设备
void initialize_d3d9() noexcept
{
	g_idrect3d9 = Direct3DCreate9(D3D_SDK_VERSION);
	assert(g_idrect3d9);

	ZeroMemory(&g_present, sizeof(g_present));
	g_present.Windowed = TRUE;
	g_present.SwapEffect = D3DSWAPEFFECT_DISCARD;
	g_present.BackBufferFormat = D3DFMT_UNKNOWN;
	g_present.EnableAutoDepthStencil = TRUE;
	g_present.AutoDepthStencilFormat = D3DFMT_D16;
	g_present.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
	HRESULT state = g_idrect3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, g_hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_present, &g_direct3ddevice9);
	assert(state == S_OK);
}

//初始化界面库
void initialize_imgui() noexcept
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(g_hwnd);
	ImGui_ImplDX9_Init(g_direct3ddevice9);
	const char* font_path = "msyh.ttc";
	if (std::filesystem::exists(font_path))
		io.Fonts->AddFontFromFileTTF(font_path, 20.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());
}

//消息处理
void message_handle() noexcept
{
	MSG msg{ 0 };
	while (msg.message != WM_QUIT)
	{
		if (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
		else render_handle();
	}
}

//渲染处理
void render_handle() noexcept
{
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin(u8"控制窗口");
	if (ImGui::Button(u8"选择文件夹路径")) select_path();
	show_file();
	set_image_texture(); 
	render_image();
	ImGui::End();

	ImGui::EndFrame();
	g_direct3ddevice9->SetRenderState(D3DRS_ZENABLE, false);
	g_direct3ddevice9->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
	g_direct3ddevice9->SetRenderState(D3DRS_SCISSORTESTENABLE, false);
	g_direct3ddevice9->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(200, 200, 200, 200), 1.0f, 0);
	if (g_direct3ddevice9->BeginScene() >= 0)
	{
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		g_direct3ddevice9->EndScene();
	}
	if (g_direct3ddevice9->Present(NULL, NULL, NULL, NULL) == D3DERR_DEVICELOST
		&& g_direct3ddevice9->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) reset_device();

	clear_image_texture();
}

//处理设备丢失
void reset_device() noexcept
{
	if (g_direct3ddevice9)
	{
		ImGui_ImplDX9_InvalidateDeviceObjects();
		g_direct3ddevice9->Reset(&g_present);
		ImGui_ImplDX9_CreateDeviceObjects();
	}
}

//清理所有设置
void clear_setting() noexcept
{
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	if (g_idrect3d9) g_idrect3d9->Release();
	if (g_direct3ddevice9) g_direct3ddevice9->Release();

	DestroyWindow(g_hwnd);
	::UnregisterClass(g_name, GetModuleHandle(NULL));
}

//选择图像路径窗口
void select_path() noexcept
{
	BROWSEINFOA browser{ NULL,0,NULL,NULL,BIF_DONTGOBELOWDOMAIN
	| BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_EDITBOX ,NULL,0,0 };
	if (LPITEMIDLIST browser_info = SHBrowseForFolder(&browser))
	{
		char buffer[1024];
		if (SHGetPathFromIDListA(browser_info, buffer)) traversing_files(buffer);
		CoTaskMemFree(browser_info);
	}
}

//显示文件夹中的文件
void show_file() noexcept
{
	for (int i = 0; i < g_path.size(); i++)
	{
		if (g_path[i].valid)
		{
			ImGui::Begin(g_path[i].path.c_str(), &g_path[i].valid);
			for (int j = 0; j < g_path[i].path_list.size(); j++)
				if (ImGui::Button(g_path[i].path_list[j].c_str())) initialize_image(g_path[i].path.c_str(), g_path[i].path_list[j].c_str());
			ImGui::End();
		}
		else
		{
			g_path[i].path_list.clear();
			g_path.erase(g_path.begin() + (i--));
		}
	}
}

//初始化图像数据
void initialize_image(const char* path, const char* str) noexcept
{
	if (!str || !strlen(str)) return;
	image_info info;
	info.path = utf8_to_string(path) + "\\" + utf8_to_string(str);
	int channel;
	if (unsigned char* image_data = stbi_load(info.path.c_str(), &info.width, &info.height, &channel, 3))
	{
		info.channel = channel + 1;
		if (info.buffer = new unsigned char[info.width * info.height *(channel + 1)])
		{
			for (int i = 0; i < info.height; i++)
			{
				for (int j = 0; j < info.width; j++)
				{
					info.buffer[(i* info.width + j) * (channel + 1) + 0] = image_data[(i*info.width + j)*channel + 2];
					info.buffer[(i* info.width + j) * (channel + 1) + 1] = image_data[(i*info.width + j)*channel + 1];
					info.buffer[(i* info.width + j) * (channel + 1) + 2] = image_data[(i*info.width + j)*channel + 0];
					info.buffer[(i* info.width + j) * (channel + 1) + 3] = 0xff;
				}
			}
			g_image.push_back(std::move(info));
		}
	}
}

//设置图像像素到纹理
void set_image_texture() noexcept
{
	for (auto& info : g_image)
	{
		if (g_direct3ddevice9->CreateTexture(info.width, info.height, 1, D3DUSAGE_DYNAMIC, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &info.texture, NULL) == S_OK)
		{
			D3DLOCKED_RECT lock_rect;
			if (info.texture->LockRect(0, &lock_rect, NULL, 0) == S_OK)
			{
				for (int y = 0; y < info.height; y++)
					memcpy((unsigned char *)lock_rect.pBits + lock_rect.Pitch * y, info.buffer + (info.width * info.channel) * y, (info.width * info.channel));
				info.texture->UnlockRect(0);
			}
		}
	}
}

//渲染图片到窗口上
void render_image() noexcept
{
	for (int i = 0; i < g_image.size(); i++)
	{
		if (g_image[i].valid)
		{
			ImGui::Begin(string_to_utf8(g_image[i].path.c_str()).c_str(), &g_image[i].valid);
			if (g_image[i].texture) ImGui::Image(g_image[i].texture, ImVec2(g_image[i].width, g_image[i].height), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec4(1.0f, 1.0f, 1.0f, 0.5f));
			ImGui::End();
		}
		else
		{
			if (g_image[i].buffer) delete[] g_image[i].buffer;
			if (g_image[i].texture) g_image[i].texture->Release();
			g_image.erase(g_image.begin() + (i--));
		}
	}
}

//清理图片纹理
void clear_image_texture() noexcept
{
	for (auto& it : g_image)
	{
		if (it.texture)
		{
			it.texture->Release();
			it.texture = nullptr;
		}
	}
}

//将string转化为utf-8编码
std::string string_to_utf8(const std::string& str) noexcept
{
	int nwLen = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);
	wchar_t* pwBuf = new wchar_t[nwLen + 1];
	memset(pwBuf, 0, nwLen * 2 + 2);
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), str.length(), pwBuf, nwLen);

	int nLen = WideCharToMultiByte(CP_UTF8, 0, pwBuf, -1, NULL, NULL, NULL, NULL);
	char* pBuf = new char[nLen + 1];
	memset(pBuf, 0, nLen + 1);
	WideCharToMultiByte(CP_UTF8, 0, pwBuf, nwLen, pBuf, nLen, NULL, NULL);

	std::string ret = pBuf;
	delete[]pwBuf;
	delete[]pBuf;

	return ret;
}

std::string utf8_to_string(const std::string& str) noexcept
{
	int nwLen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
	wchar_t* pwBuf = new wchar_t[nwLen + 1];
	memset(pwBuf, 0, nwLen * 2 + 2);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), pwBuf, nwLen);

	int nLen = WideCharToMultiByte(CP_ACP, 0, pwBuf, -1, NULL, NULL, NULL, NULL);
	char* pBuf = new char[nLen + 1];
	memset(pBuf, 0, nLen + 1);
	WideCharToMultiByte(CP_ACP, 0, pwBuf, nwLen, pBuf, nLen, NULL, NULL);

	std::string ret = pBuf;
	delete[]pBuf;
	delete[]pwBuf;

	return ret;
}

//遍历文件
void traversing_files(const char* str) noexcept
{
	WIN32_FIND_DATAA file_data;
	char buffer[1024];
	sprintf(buffer, "%s\\*", str);
	if (HANDLE file = FindFirstFileA(buffer, &file_data); file != INVALID_HANDLE_VALUE)
	{
		file_path path_info;
		path_info.path = string_to_utf8(str);
		do 
		{ 
			if (file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
			if (int len = strlen(file_data.cFileName); strstr(file_data.cFileName, ".jpg")
				|| strstr(file_data.cFileName, ".png") || strstr(file_data.cFileName, ".bmp"))
				path_info.path_list.push_back(string_to_utf8(file_data.cFileName));
		}
		while (FindNextFileA(file, &file_data));
		g_path.push_back(std::move(path_info));
	}
}

//窗口过程
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT _stdcall window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) return true;

	static PAINTSTRUCT ps;
	static HDC hdc;
	switch (msg)
	{
	case  WM_SIZE:	
	{
		g_present.BackBufferWidth = LOWORD(lparam);
		g_present.BackBufferHeight = HIWORD(lparam);
		reset_device();
		break;
	}
	case WM_DESTROY: PostQuitMessage(0); break;
	}

	return DefWindowProcA(hwnd, msg, wparam, lparam);
}