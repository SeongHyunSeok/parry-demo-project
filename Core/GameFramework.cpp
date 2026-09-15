#define WORLD_MAIN

#include "GameFramework.h"
#include "AudioManager.h"
#include "world.h"
#include "inputs.h"
#include <WindowsX.h>

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>


using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

namespace
{
	const XMFLOAT3 PlayerSpawnPosition{ 0.0f, 10.0f, -5.0f };
	const XMFLOAT3 EnemySpawnPosition{ 0.0f, 10.0f, 5.0f };
	const XMFLOAT3 CharacterSpawnRotation{ 0.0f, MathHelper::Pi, 0.0f };
}

static Camera& GetMainCamera() {
	auto& cameras = WORLD.GetComponents<Camera>();
	assert(!cameras.empty());
	return cameras[0];
}

GameFramework* GameFramework::mApp = nullptr;
GameFramework* GameFramework::GetApp()
{
	return mApp;
}

GameFramework::GameFramework(HINSTANCE hInstance) : mhAppInst(hInstance)
{
	// Only one GameFramework can be constructed.
	assert(mApp == nullptr);
	mApp = this;

	// Estimate the scene bounding sphere manually since we know how the scene was constructed.
	// The grid is the "widest object" with a width of 20 and depth of 30.0f, and centered at
	// the world space origin.  In general, you need to loop over every world space vertex
	// position and compute the bounding sphere.
	mSceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	mSceneBounds.Radius = sqrtf(50.0f * 50.0f + 50.0f * 50.0f);
}

GameFramework::~GameFramework()
{
	AudioManager::GetInstance().Shutdown();
	if (md3dDevice != nullptr)
		FlushCommandQueue();

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

HINSTANCE GameFramework::AppInst()const
{
	return mhAppInst;
}

HWND GameFramework::MainWnd()const
{
	return mhMainWnd;
}

float GameFramework::AspectRatio()const
{
	return static_cast<float>(mClientWidth) / mClientHeight;
}

bool GameFramework::Get4xMsaaState()const
{
	return m4xMsaaState;
}

void GameFramework::Set4xMsaaState(bool value)
{
	if (m4xMsaaState != value)
	{
		m4xMsaaState = value;

		// Recreate the swapchain and buffers with new multisample settings.
		CreateSwapChain();
		OnResize();
	}
}

int GameFramework::Run()
{
	MSG msg = { 0 };

	CPUClockPoint start = GLOBAL_TIMER.Now();

	while (msg.message != WM_QUIT)
	{
		// If there are Window messages then process them.
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		// Otherwise, do animation/game stuff.
		else
		{
			if (!mAppPaused)
			{
				float dt = mTimerView.GetElapsed<std::chrono::milliseconds>()->count() / 1000.f;
				// Force FPS to 15 when it is lower than 15 fps to make physics engine stable
				if (dt > (1.f / 15.f)) {
					dt = (1.f / 15.f);
				}
				CalculateFrameStats();
				Update(dt);
				Draw(dt);

				auto now = GLOBAL_TIMER.Now();
				GLOBAL_TIMER.RecordElapsed(start, now);
				start = now;
			}
			else
			{
				Sleep(10);
			}
			InputManager::GetInstance().ResetPreviousStates();
		}
	}

	return (int)msg.wParam;
}

void GameFramework::CalculateFrameStats()
{
	// Code computes the average frames per second, and also the 
	// average time it takes to render one frame.  These stats 
	// are appended to the window caption bar.

	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	timeElapsed += GLOBAL_TIMER.GetElapsed<std::chrono::duration<float>>()->count();

	// Compute averages over one second period.
	if (timeElapsed >= 1.0f)
	{
		float fps = (float)frameCnt; // fps = frameCnt / 1
		float mspf = 1000.0f / fps;

		wstring fpsStr = to_wstring(fps);
		wstring mspfStr = to_wstring(mspf);

		wstring windowText = mMainWndCaption +
			L"    fps: " + fpsStr +
			L"   mspf: " + mspfStr;

		SetWindowText(mhMainWnd, windowText.c_str());

		// Reset for next average.
		frameCnt = 0;
		timeElapsed = 0.0f;
	}
}

bool GameFramework::Initialize()
{
	if (!InitMainWindow())
		return false;

	if (!InitDirect3D())
		return false;

	if (!InitImGui())
		return false;

	// Initialization
	auto initTimePoint = GLOBAL_TIMER.Now();
	physics::InitPhysics();
	auto hCameraObj = WORLD.AddObject();
	Camera comCamera{ hCameraObj };
	comCamera.SetPosition(0.0f, 2.0f, -15.0f);
	WORLD.AttachComponent(hCameraObj, std::move(comCamera));
	WORLD.AttachComponent(hCameraObj, AttachedCamera{ hCameraObj, physics::Vec3{0.f, 2.f, -13.f}, 0.1f });

	// Do the initial resize code.
	OnResize();

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));


	mShadowMap = std::make_unique<ShadowMap>(md3dDevice.Get(),
		2048, 2048);

	mSsao = std::make_unique<Ssao>(
		md3dDevice.Get(),
		mCommandList.Get(),
		mClientWidth, mClientHeight);

	mParticleManager.Initialize(
		md3dDevice.Get(),
		mCommandList.Get(),
		mBackBufferFormat, mDepthStencilFormat,
		m4xMsaaState, m4xMsaaQuality
	);

	LoadSkinnedModel();
	LoadTextures();
	BuildRootSignature();
	BuildSsaoRootSignature();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();
	BuildShapeGeometry();
	BuildFbxModelGeometry();
	BuildMaterials();
	BuildRenderItems();
	BuildFrameResources();
	BuildPSOs();

	mSsao->SetPSOs(mPSOs["ssao"].Get(), mPSOs["ssaoBlur"].Get());

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	// TODO: Release unused upload buffer

	GLOBAL_TIMER.RecordElapsed(initTimePoint, GLOBAL_TIMER.Now());

	while (ShowCursor(FALSE) >= 0);

	// Audio failures are reported in the settings window; gameplay can continue.
	AudioManager::GetInstance().Initialize();
	AudioManager::GetInstance().PlayBgm();

	return true;
}

LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
		return true;

	// Forward hwnd on because we can get messages (e.g., WM_CREATE)
	// before CreateWindow returns, and thus before mhMainWnd is valid.
	return GameFramework::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

bool GameFramework::InitMainWindow()
{
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = mhAppInst;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"MainWnd";

	if (!RegisterClass(&wc))
	{
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return false;
	}

	// Compute window rectangle dimensions based on requested client area dimensions.
	RECT R = { 0, 0, mClientWidth, mClientHeight };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	mhMainWnd = CreateWindow(L"MainWnd", mMainWndCaption.c_str(),
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mhAppInst, 0);
	if (!mhMainWnd)
	{
		MessageBox(0, L"CreateWindow Failed.", 0, 0);
		return false;
	}

	ShowWindow(mhMainWnd, SW_SHOW);
	UpdateWindow(mhMainWnd);

	RAWINPUTDEVICE Rid;
	Rid.usUsagePage = 0x01;
	Rid.usUsage = 0x02;
	Rid.dwFlags = 0;
	Rid.hwndTarget = mhMainWnd;

	if (RegisterRawInputDevices(&Rid, 1, sizeof(Rid)) == FALSE) {
		MessageBox(0, L"RegisterRawInputDevices Failed.", 0, 0);
		return false;
	}

	return true;
}

LRESULT GameFramework::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		// WM_ACTIVATE is sent when the window is activated or deactivated.  
		// We pause the game when the window is deactivated and unpause it 
		// when it becomes active.  
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			mAppPaused = true;
			//ClipCursor(nullptr);
			ReleaseCapture();
			mTimerView.SetTimeMultiplier(0.0);
		}
		else
		{
			mAppPaused = false;
			RECT clientRect;
			GetClientRect(mhMainWnd, &clientRect);
			MapWindowPoints(mhMainWnd, NULL, (LPPOINT)(&clientRect), 2);
			//ClipCursor(&clientRect);
			SetCapture(mhMainWnd);
			mTimerView.SetTimeMultiplier(1.0);
		}
		return 0;

		// WM_SIZE is sent when the user resizes the window.  
	case WM_SIZE:
		// Save the new client area dimensions.
		mClientWidth = LOWORD(lParam);
		mClientHeight = HIWORD(lParam);
		if (md3dDevice)
		{
			if (wParam == SIZE_MINIMIZED)
			{
				mAppPaused = true;
				mMinimized = true;
				mMaximized = false;
			}
			else if (wParam == SIZE_MAXIMIZED)
			{
				mAppPaused = false;
				mMinimized = false;
				mMaximized = true;
				OnResize();
			}
			else if (wParam == SIZE_RESTORED)
			{

				// Restoring from minimized state?
				if (mMinimized)
				{
					mAppPaused = false;
					mMinimized = false;
					OnResize();
				}

				// Restoring from maximized state?
				else if (mMaximized)
				{
					mAppPaused = false;
					mMaximized = false;
					OnResize();
				}
				else if (mResizing)
				{
					// If user is dragging the resize bars, we do not resize 
					// the buffers here because as the user continuously 
					// drags the resize bars, a stream of WM_SIZE messages are
					// sent to the window, and it would be pointless (and slow)
					// to resize for each WM_SIZE message received from dragging
					// the resize bars.  So instead, we reset after the user is 
					// done resizing the window and releases the resize bars, which 
					// sends a WM_EXITSIZEMOVE message.
				}
				else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
				{
					OnResize();
				}
			}
		}
		return 0;

		// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
	case WM_ENTERSIZEMOVE:
		mAppPaused = true;
		mResizing = true;
		mTimerView.SetTimeMultiplier(0.0);
		return 0;

		// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
		// Here we reset everything based on the new window dimensions.
	case WM_EXITSIZEMOVE:
		mAppPaused = false;
		mResizing = false;
		mTimerView.SetTimeMultiplier(1.0);
		OnResize();
		return 0;

		// WM_DESTROY is sent when the window is being destroyed.
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

		// The WM_MENUCHAR message is sent when a menu is active and the user presses 
		// a key that does not correspond to any mnemonic or accelerator key. 
	case WM_MENUCHAR:
		// Don't beep when we alt-enter.
		return MAKELRESULT(0, MNC_CLOSE);

		// Catch this message so to prevent the window from becoming too small.
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;

	case WM_INPUT:
	{
		UINT dwSize;

		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
		LPBYTE lpb = new BYTE[dwSize];

		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));

		RAWINPUT* raw = (RAWINPUT*)lpb;

		if (raw->header.dwType == RIM_TYPEMOUSE)
		{
			auto& inputManager = InputManager::GetInstance();
			auto& mouseEvent = raw->data.mouse;
			if ((mouseEvent.usFlags & 1) == 0) {
				inputManager.HandleMouseMovement(mouseEvent.lLastX, mouseEvent.lLastY);
				if (!inputManager.IsMouseLocked()) {
					POINT currMousePoint;
					GetCursorPos(&currMousePoint);
					inputManager.HandleNewMousePosition(currMousePoint.x, currMousePoint.y);
				}
			}
			else {
				POINT currMousePoint;
				GetCursorPos(&currMousePoint);

				auto mousePosition = inputManager.GetMousePos();
				inputManager.HandleMouseMovement(currMousePoint.x - mousePosition.x, currMousePoint.y - mousePosition.y);
				inputManager.HandleNewMousePosition(currMousePoint.x, currMousePoint.y);
			}

			if (inputManager.IsMouseLocked()) {
				POINT center{ mClientWidth / 2, mClientHeight / 2 };
				ClientToScreen(mhMainWnd, &center);
				SetCursorPos(center.x, center.y);
			}

			if (mouseEvent.usButtonFlags & RI_MOUSE_BUTTON_1_DOWN) {
				inputManager.HandlePressed(VK_LBUTTON);
			}
			if (mouseEvent.usButtonFlags & RI_MOUSE_BUTTON_1_UP) {
				inputManager.HandleReleased(VK_LBUTTON);
			}
			if (mouseEvent.usButtonFlags & RI_MOUSE_BUTTON_2_DOWN) {
				inputManager.HandlePressed(VK_RBUTTON);
			}
			if (mouseEvent.usButtonFlags & RI_MOUSE_BUTTON_2_UP) {
				inputManager.HandleReleased(VK_RBUTTON);
			}
			if (mouseEvent.usButtonFlags & RI_MOUSE_BUTTON_3_DOWN) {
				inputManager.HandlePressed(VK_MBUTTON);
			}
			if (mouseEvent.usButtonFlags & RI_MOUSE_BUTTON_3_UP) {
				inputManager.HandleReleased(VK_MBUTTON);
			}
		}

		delete[] lpb;
		return 0;
	}

	case WM_KEYDOWN:
		// Treat as key is pressed only when repeat count is one
		if ((lParam & 0xFFFF) <= 1) {
			InputManager::GetInstance().HandlePressed(wParam);
		}
		return 0;
	case WM_KEYUP:
		if (wParam == VK_ESCAPE)
		{
			PostQuitMessage(0);
		}
		else if ((int)wParam == VK_F2)
			Set4xMsaaState(!m4xMsaaState);
		InputManager::GetInstance().HandleReleased(wParam);
		return 0;
	}


	return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool GameFramework::InitDirect3D()
{
#if defined(DEBUG) || defined(_DEBUG) 
	// Enable the D3D12 debug layer.
	{
		ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();
	}
#endif
#if defined(DEBUG) || defined(_DEBUG) 
ThrowIfFailed(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&mdxgiFactory)));
#else
ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory)));
#endif

//mdxgiFactory->MakeWindowAssociation(mhMainWnd, DXGI_MWA_NO_ALT_ENTER);

// Try to create hardware device.
HRESULT hardwareResult = D3D12CreateDevice(
	nullptr,             // default adapter
	D3D_FEATURE_LEVEL_11_0,
	IID_PPV_ARGS(&md3dDevice));

// Fallback to WARP device.
if (FAILED(hardwareResult))
{
	ComPtr<IDXGIAdapter> pWarpAdapter;
	ThrowIfFailed(mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));

	ThrowIfFailed(D3D12CreateDevice(
		pWarpAdapter.Get(),
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&md3dDevice)));
}

ThrowIfFailed(md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
	IID_PPV_ARGS(&mFence)));

mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

// Check 4X MSAA quality support for our back buffer format.
// All Direct3D 11 capable devices support 4X MSAA for all render 
// target formats, so we only need to check quality support.

D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
msQualityLevels.Format = mBackBufferFormat;
msQualityLevels.SampleCount = 4;
msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
msQualityLevels.NumQualityLevels = 0;
ThrowIfFailed(md3dDevice->CheckFeatureSupport(
	D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
	&msQualityLevels,
	sizeof(msQualityLevels)));

m4xMsaaQuality = msQualityLevels.NumQualityLevels;
assert(m4xMsaaQuality > 0 && "Unexpected MSAA quality level.");

#ifdef _DEBUG
LogAdapters();
#endif

CreateCommandObjects();
CreateSwapChain();
CreateRtvAndDsvDescriptorHeaps();

return true;
}

bool GameFramework::InitImGui()
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = 64;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&desc, IID_PPV_ARGS(mImGuiSrvHeap.GetAddressOf())));

	mImGuiSrvDescHeapAlloc.Create(md3dDevice.Get(), mImGuiSrvHeap.Get());

	// Make process DPI aware and obtain main monitor scale
	ImGui_ImplWin32_EnableDpiAwareness();
	float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

	// Setup scaling
	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
	style.FontScaleDpi = main_scale;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(mhMainWnd);

	ImGui_ImplDX12_InitInfo init_info = {};
	init_info.Device = md3dDevice.Get();
	init_info.CommandQueue = mCommandQueue.Get();
	init_info.NumFramesInFlight = gNumFrameResources;
	init_info.RTVFormat = mBackBufferFormat;
	init_info.DSVFormat = mDepthStencilFormat;
	// Allocating SRV descriptors (for textures) is up to the application, so we provide callbacks.
	// (current version of the backend will only allocate one descriptor, future versions will need to allocate more)
	init_info.SrvDescriptorHeap = mImGuiSrvHeap.Get();
	init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) { return mImGuiSrvDescHeapAlloc.Alloc(out_cpu_handle, out_gpu_handle); };
	init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) { return mImGuiSrvDescHeapAlloc.Free(cpu_handle, gpu_handle); };
	ImGui_ImplDX12_Init(&init_info);

	return true;
}

void GameFramework::CreateCommandObjects()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));

	ThrowIfFailed(md3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf())));

	ThrowIfFailed(md3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		mDirectCmdListAlloc.Get(), // Associated command allocator
		nullptr,                   // Initial PipelineStateObject
		IID_PPV_ARGS(mCommandList.GetAddressOf())));

	// Start off in a closed state.  This is because the first time we refer 
	// to the command list we will Reset it, and it needs to be closed before
	// calling Reset.
	mCommandList->Close();
}

void GameFramework::CreateSwapChain()
{
	// Release the previous swapchain we will be recreating.
	mSwapChain.Reset();

	DXGI_SWAP_CHAIN_DESC1 sd = {};
	sd.Width = mClientWidth;
	sd.Height = mClientHeight;
	sd.Format = mBackBufferFormat;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = SwapChainBufferCount;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = 0;
	sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	sd.Scaling = DXGI_SCALING_STRETCH;

	// Note: Swap chain uses queue to perform flush.
	ThrowIfFailed(mdxgiFactory->CreateSwapChainForHwnd(
		mCommandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
		MainWnd(),
		&sd,
		nullptr,
		nullptr,
		mSwapChain.GetAddressOf()
	));
}

ID3D12Resource* GameFramework::CurrentBackBuffer()const
{
	return mSwapChainBuffer[mCurrBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE GameFramework::CurrentBackBufferView()const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
		mCurrBackBuffer,
		mRtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE GameFramework::DepthStencilView()const
{
	return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void GameFramework::FlushCommandQueue()
{
	// Advance the fence value to mark commands up to this fence point.
	mCurrentFence++;

	// Add an instruction to the command queue to set a new fence point.  Because we 
	// are on the GPU timeline, the new fence point won't be set until the GPU finishes
	// processing all the commands prior to this Signal().
	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

	// Wait until the GPU has completed commands up to this fence point.
	if (mFence->GetCompletedValue() < mCurrentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);

		// Fire event when GPU hits current fence.  
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));

		// Wait until the GPU hits current fence event is fired.
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

void GameFramework::LogAdapters()
{
	UINT i = 0;
	IDXGIAdapter* adapter = nullptr;
	std::vector<IDXGIAdapter*> adapterList;
	while (mdxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);

		std::wstring text = L"***Adapter: ";
		text += desc.Description;
		text += L"\n";

		OutputDebugString(text.c_str());

		adapterList.push_back(adapter);

		++i;
	}

	for (size_t i = 0; i < adapterList.size(); ++i)
	{
		LogAdapterOutputs(adapterList[i]);
		ReleaseCom(adapterList[i]);
	}
}

void GameFramework::LogAdapterOutputs(IDXGIAdapter* adapter)
{
	UINT i = 0;
	IDXGIOutput* output = nullptr;
	while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_OUTPUT_DESC desc;
		output->GetDesc(&desc);

		std::wstring text = L"***Output: ";
		text += desc.DeviceName;
		text += L"\n";
		OutputDebugString(text.c_str());

		LogOutputDisplayModes(output, mBackBufferFormat);

		ReleaseCom(output);

		++i;
	}
}

void GameFramework::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
{
	UINT count = 0;
	UINT flags = 0;

	// Call with nullptr to get list count.
	output->GetDisplayModeList(format, flags, &count, nullptr);

	std::vector<DXGI_MODE_DESC> modeList(count);
	output->GetDisplayModeList(format, flags, &count, &modeList[0]);

	for (auto& x : modeList)
	{
		UINT n = x.RefreshRate.Numerator;
		UINT d = x.RefreshRate.Denominator;
		std::wstring text =
			L"Width = " + std::to_wstring(x.Width) + L" " +
			L"Height = " + std::to_wstring(x.Height) + L" " +
			L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
			L"\n";

		::OutputDebugString(text.c_str());
	}
}




/////////////////////

void GameFramework::OnResize()
{
	assert(md3dDevice);
	assert(mSwapChain);
	assert(mDirectCmdListAlloc);

	// Flush before changing any resources.
	FlushCommandQueue();

	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// Release the previous resources we will be recreating.
	for (int i = 0; i < SwapChainBufferCount; ++i)
		mSwapChainBuffer[i].Reset();
	mDepthStencilBuffer.Reset();

	// Resize the swap chain.
	ThrowIfFailed(mSwapChain->ResizeBuffers(
		SwapChainBufferCount,
		mClientWidth, mClientHeight,
		mBackBufferFormat,
		0));

	mCurrBackBuffer = 0;

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < SwapChainBufferCount; i++)
	{
		ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i])));
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = mBackBufferFormat;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		md3dDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), &rtvDesc, rtvHeapHandle);
		rtvHeapHandle.Offset(1, mRtvDescriptorSize);
	}

	// Create the depth/stencil buffer and view.
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = mClientWidth;
	depthStencilDesc.Height = mClientHeight;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;

	// Correction 11/12/2016: SSAO chapter requires an SRV to the depth buffer to read from 
	// the depth buffer.  Therefore, because we need to create two views to the same resource:
	//   1. SRV format: DXGI_FORMAT_R24_UNORM_X8_TYPELESS
	//   2. DSV Format: DXGI_FORMAT_D24_UNORM_S8_UINT
	// we need to create the depth buffer resource with a typeless format.  
	depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;

	depthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	depthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = mDepthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;
	CD3DX12_HEAP_PROPERTIES hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&hp,
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())));

	// Create descriptor to mip level 0 of entire resource using the format of the resource.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = mDepthStencilFormat;
	dsvDesc.Texture2D.MipSlice = 0;
	md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &dsvDesc, DepthStencilView());

	// Transition the resource from its initial state to be used as a depth buffer.
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	mCommandList->ResourceBarrier(1, &barrier);

	// Execute the resize commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until resize is complete.
	FlushCommandQueue();

	// Update the viewport transform to cover the client area.
	mScreenViewport.TopLeftX = 0;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width = static_cast<float>(mClientWidth);
	mScreenViewport.Height = static_cast<float>(mClientHeight);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;

	mScissorRect = { 0, 0, mClientWidth, mClientHeight };

	///////////

	auto& camera = GetMainCamera();
	camera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 0.01f, 1000.0f);

	if (mSsao != nullptr)
	{
		mSsao->OnResize(mClientWidth, mClientHeight);

		// Resources changed, so need to rebuild descriptors.
		mSsao->RebuildDescriptors(mDepthStencilBuffer.Get());
	}
}

void GameFramework::CreateRtvAndDsvDescriptorHeaps()
{
	// Add +1 for screen normal map, +2 for ambient maps.
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount + 3;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

	// Add +1 DSV for shadow map.
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 2;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));
}

void GameFramework::Update(float dt)
{
	// Cycle through the circular frame resource array.
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();
	// Has the GPU finished processing the commands of the current frame resource?
// If not, wait until the GPU has completed commands up to this fence point.
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	auto& camera = GetMainCamera();

	HandleKeyboardInput(camera, dt);
	HandleMouseInput(camera, dt);
	UpdateTargetLock();

	UpdateImGui();

	auto& player = WORLD.GetComponents<PlayerComponent>().front();
	auto hOwner = player.GetOwner();
	auto comEquipmentOwner = WORLD.GetComponent<EquipmentOwnerComponent>(hOwner);

	if (bOneHandMode &&
		comEquipmentOwner->equipments[(UINT)EquipmentType::TwoHandSword]) {
		auto& hTwohandObj = comEquipmentOwner->equipments[(UINT)EquipmentType::TwoHandSword];
		if (hTwohandObj.has_value())
		{
			WORLD.DetachComponent<EquippedComponent>(hTwohandObj.value());
			comEquipmentOwner->equipments[(UINT)EquipmentType::TwoHandSword] = std::nullopt;

			auto& hOneHandObj = comEquipmentOwner->equipmentStorage[(UINT)EquipmentType::OneHandSword];
			auto comEquipped = EquippedComponent(hOneHandObj.value());
			comEquipped.EquippedCBIndex = comEquipmentOwner->equippedCBIndices[(UINT)EquipmentType::OneHandSword];
			comEquipped.owner = hOwner;
			WORLD.AttachComponent(hOneHandObj.value(), std::move(comEquipped));
			comEquipmentOwner->equipments[(UINT)EquipmentType::OneHandSword] = hOneHandObj.value();

			auto& hShieldObj = comEquipmentOwner->equipmentStorage[(UINT)EquipmentType::Shield];
			auto comShieldEquipped = EquippedComponent(hShieldObj.value());
			comShieldEquipped.EquippedCBIndex = comEquipmentOwner->equippedCBIndices[(UINT)EquipmentType::Shield];
			comShieldEquipped.owner = hOwner;
			WORLD.AttachComponent(hShieldObj.value(), std::move(comShieldEquipped));
			comEquipmentOwner->equipments[(UINT)EquipmentType::Shield] = hShieldObj.value();
		}
	}
	else if (!bOneHandMode &&
		comEquipmentOwner->equipments[(UINT)EquipmentType::OneHandSword]) {
		auto& hOneHandObj = comEquipmentOwner->equipments[(UINT)EquipmentType::OneHandSword];
		WORLD.DetachComponent<EquippedComponent>(hOneHandObj.value());
		comEquipmentOwner->equipments[(UINT)EquipmentType::OneHandSword] = std::nullopt;

		auto& hShieldObj = comEquipmentOwner->equipments[(UINT)EquipmentType::Shield];
		WORLD.DetachComponent<EquippedComponent>(hShieldObj.value());
		comEquipmentOwner->equipments[(UINT)EquipmentType::Shield] = std::nullopt;

		auto& hEquipobj = comEquipmentOwner->equipmentStorage[(UINT)EquipmentType::TwoHandSword];

		auto comEquipped = EquippedComponent(hEquipobj.value());
		comEquipped.EquippedCBIndex = comEquipmentOwner->equippedCBIndices[(UINT)EquipmentType::TwoHandSword];
		comEquipped.owner = hOwner;
		WORLD.AttachComponent(hEquipobj.value(), std::move(comEquipped));
		comEquipmentOwner->equipments[(UINT)EquipmentType::TwoHandSword] = hEquipobj.value();
	}

	// Reset after applying the selected equipment and before combat/physics updates.
	if (mRestartRequested)
	{
		RestartGame();
		mRestartRequested = false;
	}

	//
	// Animate the lights (and hence shadows).
	//

	mLightRotationAngle += 0.05f * dt;

	XMMATRIX R = XMMatrixRotationY(mLightRotationAngle);
	for (int i = 0; i < 3; ++i)
	{
		XMVECTOR lightDir = XMLoadFloat3(&mBaseLightDirections[i]);
		lightDir = XMVector3TransformNormal(lightDir, R);
		XMStoreFloat3(&mRotatedLightDirections[i], lightDir);
	}


	// Debug--------------
	// Capture swing cues before a successful parry can change the attack state.
	for (auto& stateMachine : WORLD.GetComponents<StateMachineComponent>())
		stateMachine.UpdateCombatAudio();

	//auto& player = WORLD.GetComponents<PlayerComponent>().front();
	auto pComPlayerStateMachine = WORLD.GetComponent<StateMachineComponent>(player.GetOwner());
	auto pComPlayerMesh = WORLD.GetComponent<MeshComponent>(player.GetOwner());

	auto& comEnemyStateMachine = (WORLD.GetComponents<StateMachineComponent>())[1];
	auto pComEnemyMesh = WORLD.GetComponent<MeshComponent>(comEnemyStateMachine.GetOwner());

	if (bEnemyActive) {
		GameEvent ge;
		ge.enemyEventType = EnemyEventType::Active;
		comEnemyStateMachine.HandleGameEvent(ge);
	}
	else {
		GameEvent ge;
		ge.enemyEventType = EnemyEventType::Stop;
		comEnemyStateMachine.HandleGameEvent(ge);
	}

	auto pComEnemyEquipmentOwner = WORLD.GetComponent<EquipmentOwnerComponent>(comEnemyStateMachine.GetOwner());
	if (pComEnemyEquipmentOwner && pComEnemyEquipmentOwner->equipments[(UINT)EquipmentType::OneHandSword]) {
		auto pComEnemySwordMesh = WORLD.GetComponent<MeshComponent>(*pComEnemyEquipmentOwner->equipments[(UINT)EquipmentType::OneHandSword]);
		Material* curMat = mMaterials["equipment2"].get();
		if (showEnemyValidAttackFrame
			&& comEnemyStateMachine.GetCurrentSatateType() == StateType::Attack
			&& comEnemyStateMachine.IsInValidFrame()) {
			curMat = mMaterials["equipment3"].get();
		}
		if (pComEnemySwordMesh) {
			for (auto& drawItem : pComEnemySwordMesh->drawItems) {
				drawItem.Mat = curMat;
			}
		}
	}

	auto pComPlayerEquipmentOwner = WORLD.GetComponent<EquipmentOwnerComponent>(pComPlayerStateMachine->GetOwner());
	if (pComPlayerEquipmentOwner && pComPlayerEquipmentOwner->equipments[(UINT)EquipmentType::Shield]) {
		auto pComShieldMesh = WORLD.GetComponent<MeshComponent>(*pComPlayerEquipmentOwner->equipments[(UINT)EquipmentType::Shield]);
		Material* curMat = mMaterials["equipment"].get();
		if (showPlayerValidParryFrame
			&& pComPlayerStateMachine->GetCurrentSatateType() == StateType::Parry
			&& pComPlayerStateMachine->IsInValidFrame(parryJudgementFrameOffset)) {
			curMat = mMaterials["equipment4"].get();
		}
		if (pComShieldMesh) {
			for (auto& drawItem : pComShieldMesh->drawItems) {
				drawItem.Mat = curMat;
			}
		}
	}

	XMVECTOR distVec = XMLoadFloat3(&pComEnemyMesh->Translation) - XMLoadFloat3(&pComPlayerMesh->Translation);
	float distance = 0.0f;
	XMStoreFloat(&distance, XMVector3Length(distVec));

	if (distance < 3.0f &&
		pComPlayerStateMachine->GetCurrentSatateType() == StateType::Attack)
	{
		if (pComPlayerStateMachine->IsInValidFrame() && comEnemyStateMachine.GetCurrentSatateType() == StateType::Idle) {
			XMINT3 hitDir = { 0, 0, 1 };
			if (pComPlayerMesh->Translation.z > pComEnemyMesh->Translation.z) // TO DO
			{
				hitDir.z = -1;
			}

			GameEvent ge;
			ge.eventType = GameEventType::Hit;
			ge.dir = hitDir;
			ge.strength = 0.1f;
			comEnemyStateMachine.HandleGameEvent(ge);
		}
		else if (comEnemyStateMachine.GetCurrentSatateType() == StateType::Parried
			&& pComPlayerStateMachine->IsAnimationStarting()) {
			GameEvent ge;
			ge.eventType = GameEventType::ActiveFatalAttack;
			pComPlayerStateMachine->HandleGameEvent(ge);

			XMINT3 hitDir = { 0, 0, 1 };
			GameEvent enemyGe;
			enemyGe.eventType = GameEventType::Hit;
			enemyGe.dir = hitDir;
			enemyGe.strength = FATATL_HIT_STRENGTH;
			comEnemyStateMachine.HandleGameEvent(enemyGe);


			XMFLOAT2 myXZPosition = { pComPlayerMesh->Translation.x, pComPlayerMesh->Translation.z };
			XMFLOAT2 oppnentXZPosition = { pComEnemyMesh->Translation.x, pComEnemyMesh->Translation.z };
			XMVECTOR distXZVec = XMLoadFloat2(&oppnentXZPosition) - XMLoadFloat2(&myXZPosition);
			XMFLOAT2 defualtDir = XMFLOAT2(0, 1);
			XMVECTOR defaultDirVec = XMLoadFloat2(&defualtDir);
			XMVECTOR dirVec = XMVector2Normalize(distXZVec);

			float yAngle = XMVector2AngleBetweenNormalsEst(defaultDirVec, dirVec).m128_f32[0];

			if (distXZVec.m128_f32[0] < 0.0f)
			{
				yAngle = -yAngle;
			}

			pComPlayerMesh->Rotation.y = yAngle;
			directionlock = true;
			directionlockTime += dt;
		}
	}
	else if (distance < 3.0f &&
		pComPlayerStateMachine->GetCurrentSatateType() == StateType::Parry
		&& pComPlayerStateMachine->IsInValidFrame(parryJudgementFrameOffset))
	{
		if (comEnemyStateMachine.GetCurrentSatateType() == StateType::Attack
			&& comEnemyStateMachine.IsInValidFrame()) {
			GameEvent ge;
			ge.eventType = GameEventType::Parried;
			comEnemyStateMachine.HandleGameEvent(ge);
			AudioManager::GetInstance().PlaySfx(SoundId::SwordClash);
			auto parryPos = Vector3::Add(pComEnemyMesh->Translation, pComPlayerMesh->Translation);
			parryPos = Vector3::ScalarProduct(parryPos, 0.5f, false);
			parryPos.y = pComPlayerMesh->Translation.y + 3.5f;
			mParticleManager.CreateParticle(
				"effect_parry_flash",
				parryPos,
				XMFLOAT3(1.0f, 1.0f, 1.0f),
				XMFLOAT3(0.0f, 0.0f, 0.0f));
			mParticleManager.CreateParticle(
				"effect_parry_sparks",
				parryPos,
				XMFLOAT3(1.0f, 1.0f, 1.0f),
				XMFLOAT3(0.0f, 0.0f, 0.0f));
		}
	}
	else if (!playerNoHit && distance < 3.0f && comEnemyStateMachine.GetCurrentSatateType() == StateType::Attack && comEnemyStateMachine.IsInValidFrame()) {
		if (pComPlayerStateMachine->GetCurrentSatateType() == StateType::Idle
			|| pComPlayerStateMachine->GetCurrentSatateType() == StateType::Walk) {
			XMINT3 hitDir = { 0, 0, 1 };
			GameEvent ge;
			ge.eventType = GameEventType::Hit;
			ge.dir = hitDir;
			ge.strength = 0.2f;
			pComPlayerStateMachine->HandleGameEvent(ge);
		}
		else if (pComPlayerStateMachine->GetCurrentSatateType() == StateType::Roll
			&& !pComPlayerStateMachine->IsInValidFrame(rollJudgementFrameOffset) && showPlayerValidRollHit) {
			mPlayerHitTime = dt;
			Material* curMat = mMaterials["hit"].get();
			pComPlayerMesh->drawItems.front().Mat = curMat;
			for (int i = 0; i < pComPlayerMesh->drawItems.size(); ++i)
				pComPlayerMesh->drawItems[i].Mat = curMat;
		}
		
	}
	
	if (mPlayerHitTime > 0.0f) {
		if (mPlayerHitTime > 0.8f) {
			Material* curMat = mMaterials[mSkinnedMats[0].Name].get();
			for (int i = 0; i < pComPlayerMesh->drawItems.size(); ++i)
				pComPlayerMesh->drawItems[i].Mat = curMat;
			mPlayerHitTime = 0.0f;
		}
		else
			mPlayerHitTime += dt;
	}
	if (directionlockTime > 0.0f && directionlock) {
		directionlockTime += dt;
		if (directionlockTime > 3.66f) {
			directionlock = false;
			directionlockTime = 0.0f;
		}
	}

	//-----------------------

	for (auto& it : WORLD.GetComponents<StateMachineComponent>())
	{
		it.Update();
	}

	AnimateMaterials(dt);
	UpdateSkinnedCBs(dt);

	physics::UpdatePhysicsSystem(dt);
	physics::UpdateAttachedCamera();
	camera.UpdateViewMatrix();

	UpdateObjectCBs(dt);
	UpdateMaterialBuffer(dt);
	UpdateShadowTransform(dt);
	UpdateMainPassCB(dt);
	UpdateShadowPassCB(dt);
	UpdateSsaoCB(dt);

	mParticleManager.Update(dt, mCurrFrameResource, camera.GetUp3f(), camera.GetLook3f());
}

void GameFramework::Draw(float dt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	//
	// Shadow map pass.
	//

	// Bind all the materials used in this scene.  For structured buffers, we can bypass the heap and 
	// set as a root descriptor.
	auto matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
	mCommandList->SetGraphicsRootShaderResourceView(3, matBuffer->GetGPUVirtualAddress());

	// Bind null SRV for shadow map pass.
	mCommandList->SetGraphicsRootDescriptorTable(4, mNullSrv);

	// Bind all the textures used in this scene.  Observe
	// that we only have to specify the first descriptor in the table.  
	// The root signature knows how many descriptors are expected in the table.
	mCommandList->SetGraphicsRootDescriptorTable(5, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	DrawSceneToShadowMap();

	//
	// Normal/depth pass.
	//

	DrawNormalsAndDepth();

	//
	//
	// 

	mCommandList->SetGraphicsRootSignature(mSsaoRootSignature.Get());
	mSsao->ComputeSsao(mCommandList.Get(), mCurrFrameResource, 2);

	//
	// Main rendering pass.
	//

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	// Rebind state whenever graphics root signature changes.

	// Bind all the materials used in this scene.  For structured buffers, we can bypass the heap and 
	// set as a root descriptor.
	matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
	mCommandList->SetGraphicsRootShaderResourceView(3, matBuffer->GetGPUVirtualAddress());


	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Indicate a state transition on the resource usage.
	auto preToRt = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mCommandList->ResourceBarrier(1, &preToRt);

	// Clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	auto crtv = CurrentBackBufferView();
	auto dsv = DepthStencilView();
	mCommandList->OMSetRenderTargets(1, &crtv, true, &dsv);

	// Bind all the textures used in this scene.  Observe
	// that we only have to specify the first descriptor in the table.  
	// The root signature knows how many descriptors are expected in the table.
	mCommandList->SetGraphicsRootDescriptorTable(5, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

	// Bind the sky cube map.  For our demos, we just use one "world" cube map representing the environment
	// from far away, so all objects will use the same cube map and we only need to set it once per-frame.  
	// If we wanted to use "local" cube maps, we would have to change them per-object, or dynamically
	// index into an array of cube maps.

	auto& skyComp = WORLD.GetComponents<SkyComponent>().front();
	CD3DX12_GPU_DESCRIPTOR_HANDLE skyTexDescriptor(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	skyTexDescriptor.Offset(skyComp.mSkyTexHeapIndex, mCbvSrvUavDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(4, skyTexDescriptor);

	// Debug
	if (showStartMenu) {
		mCommandList->SetPipelineState(mPSOs["startPage"].Get());
		mCommandList->IASetVertexBuffers(0, 0, nullptr);
		mCommandList->IASetIndexBuffer(nullptr);
		mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		mCommandList->DrawInstanced(6, 1, 0, 0);
	}

	mCommandList->SetPipelineState(mPSOs["opaque"].Get());
	DrawRenderItems(mCommandList.Get(), WORLD.GetComponents<NonAnimatedComponent>());

	mCommandList->SetPipelineState(mPSOs["equippedOpaque"].Get());
	DrawRenderItems(mCommandList.Get(), WORLD.GetComponents<EquippedComponent>());

	mCommandList->SetPipelineState(mPSOs["skinnedOpaque"].Get());
	DrawRenderItems(mCommandList.Get(), WORLD.GetComponents<AnimationComponent>());

	// mCommandList->SetPipelineState(mPSOs["debug"].Get()); // TO DO

	mCommandList->SetPipelineState(mPSOs["sky"].Get());
	auto hSkyCompOwner = skyComp.GetOwner();
	auto pSkyMeshComp = WORLD.GetComponent<MeshComponent>(hSkyCompOwner);
	if (pSkyMeshComp) {
		DrawRenderItem(mCommandList.Get(), pSkyMeshComp);
	}

	mParticleManager.Render(mCommandList.Get(), passCB->GetGPUVirtualAddress());

	// Render Dear ImGui graphics
	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCommandList.Get());

	// Indicate a state transition on the resource usage.
	auto rtToPre = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	mCommandList->ResourceBarrier(1, &rtToPre);

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Advance the fence value to mark commands up to this fence point.
	mCurrFrameResource->Fence = ++mCurrentFence;

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void GameFramework::OnMouseDown(WPARAM btnState, int x, int y)
{
	SetCapture(mhMainWnd);
}

void GameFramework::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void GameFramework::HandleKeyboardInput(Camera& camera, float dt)
{
	using namespace physics;
	auto& inputManager = InputManager::GetInstance();

	if (showStartMenu) {
		if (inputManager.IsPressed(VK_SPACE)) {
			showStartMenu = false;
		}
		return;
	}

	if (inputManager.IsJustPressed(VK_CAPITAL)) {
		if (inputManager.IsMouseLocked()) {
			actionLock = true;
			while (ShowCursor(TRUE) < 0);
		}
		else {
			actionLock = false;
			while (ShowCursor(FALSE) >= 0);
		}
		inputManager.LockMouse(!inputManager.IsMouseLocked());
	}
	if (actionLock || ImGui::GetIO().WantCaptureKeyboard || ImGui::GetIO().WantCaptureMouse) return;

	if (inputManager.IsJustPressed(VK_MBUTTON)) {
		mTargetLocked = !mTargetLocked;
	}


	// '[' key pressed
	if (inputManager.IsJustReleased(VK_OEM_4))
		mTimerView.SetTimeMultiplier(max(mTimerView.GetTimerMultiplier() - 0.1, 0.1));
	// ']' key pressed
	if (inputManager.IsJustReleased(VK_OEM_6))
		mTimerView.SetTimeMultiplier(mTimerView.GetTimerMultiplier() + 0.1);

	auto& player = WORLD.GetComponents<PlayerComponent>().front();
	XMINT3 dir = { 0, 0, 0 };

	if (inputManager.IsPressed('W'))
		dir.z += 1;

	if (inputManager.IsPressed('S'))
		dir.z -= 1;

	if (inputManager.IsPressed('A'))
		dir.x += 1;

	if (inputManager.IsPressed('D'))
		dir.x -= 1;

	PlayerInupt playerInput;
	playerInput.dir = dir;

	if (inputManager.IsPressed(VK_SHIFT)) {
		playerInput.shield = true;
	}

	if (dir.x != 0 || dir.z != 0) {
		if (inputManager.IsPressed(VK_SPACE)) {
			playerInput.type = InputType::Roll;
		}
		else {
			playerInput.type = InputType::Walk;
		}
	}

	auto pComPlayerMesh = WORLD.GetComponent<MeshComponent>(player.GetOwner());

	if (inputManager.IsJustPressed(VK_LBUTTON) && !directionlock) {
		//mParticleManager.CreateParticle(
		//	"snow_slash_slash",
		//	(Vector3::Add( pComPlayerMesh->Translation, XMFLOAT3(0.0f, 0.2f, 0.0f))),
		//	XMFLOAT3(1.0f, 1.0f, 1.0f),
		//	XMFLOAT3(0.0f, 0.0f, 0.0f)
		//);
		//mParticleManager.CreateParticle(
		//	"snow_slash_snowflake",
		//	(Vector3::Add(pComPlayerMesh->Translation, XMFLOAT3(0.0f, 0.2f, 0.0f))),
		//	XMFLOAT3(1.0f, 1.0f, 1.0f),
		//	XMFLOAT3(0.0f, 0.0f, 0.0f)
		//);
		playerInput.type = InputType::Attack;
	}
	
	auto hOwner = player.GetOwner();
	const auto comEquipmentOwner = WORLD.GetComponent<EquipmentOwnerComponent>(hOwner);

	if (playerInput.shield && inputManager.IsJustPressed(VK_RBUTTON) && 
		!comEquipmentOwner->equipments[(UINT)EquipmentType::TwoHandSword]) {
		playerInput.type = InputType::Parray;
	}

	player.Input(playerInput);

	auto pPlayerMesh = WORLD.GetComponent<MeshComponent>(player.GetOwner());
	if (!mTargetLocked && inputManager.IsPressed('Q')) {
		pPlayerMesh->Rotation.y -= XMConvertToRadians(0.75);
	}
	if (!mTargetLocked && inputManager.IsPressed('E')) {
		pPlayerMesh->Rotation.y += XMConvertToRadians(0.75);
	}
	pPlayerMesh->Rotation.y -= ((int)(pPlayerMesh->Rotation.y / (2 * MathHelper::Pi))) * (2 * MathHelper::Pi);

	//auto playerController = WORLD.GetComponent<physics::CharacterBody>(player.GetOwner());
	//assert(playerController);
	//playerController->inJump = inputManager.IsJustPressed(VK_SPACE);
}

void GameFramework::HandleMouseInput(Camera& camera, float dt) {
	if (ImGui::GetIO().WantCaptureMouse)
		return;
	const auto& inputManager = InputManager::GetInstance();
	if (inputManager.IsMouseLocked() && !directionlock && !mTargetLocked) {
		auto mouseMoveVec = inputManager.GetMouseMotionVec();
		if (inputManager.IsPressed(VK_LBUTTON))
		{
			// Make each pixel correspond to a quarter of a degree.
			float dx = XMConvertToRadians(0.25f * static_cast<float>(mouseMoveVec.x));
			float dy = XMConvertToRadians(0.25f * static_cast<float>(mouseMoveVec.y));

			camera.Pitch(dy);
			camera.RotateY(dx);
		}
		auto& comPlayer = WORLD.GetComponents<PlayerComponent>().front();
		auto pPlayerMesh = WORLD.GetComponent<MeshComponent>(comPlayer.GetOwner());
		pPlayerMesh->Rotation.y += XMConvertToRadians(0.25f * static_cast<float>(mouseMoveVec.x));
		pPlayerMesh->Rotation.y -= ((int)(pPlayerMesh->Rotation.y / (2 * MathHelper::Pi))) * (2 * MathHelper::Pi);
	}
}

void GameFramework::UpdateTargetLock()
{
	if (!mTargetLocked) {
		return;
	}

	auto& players = WORLD.GetComponents<PlayerComponent>();
	if (players.empty()) {
		mTargetLocked = false;
		return;
	}

	const auto playerOwner = players.front().GetOwner();
	auto pPlayerMesh = WORLD.GetComponent<MeshComponent>(playerOwner);
	if (!pPlayerMesh) {
		mTargetLocked = false;
		return;
	}

	MeshComponent* pTargetMesh = nullptr;
	for (auto& stateMachine : WORLD.GetComponents<StateMachineComponent>()) {
		if (stateMachine.GetOwner() == playerOwner) {
			continue;
		}

		pTargetMesh = WORLD.GetComponent<MeshComponent>(stateMachine.GetOwner());
		if (pTargetMesh) {
			break;
		}
	}

	if (!pTargetMesh) {
		mTargetLocked = false;
		return;
	}

	const float deltaX = pTargetMesh->Translation.x - pPlayerMesh->Translation.x;
	const float deltaZ = pTargetMesh->Translation.z - pPlayerMesh->Translation.z;
	if (deltaX * deltaX + deltaZ * deltaZ <= FLT_EPSILON) {
		return;
	}

	pPlayerMesh->Rotation.y = atan2f(deltaX, deltaZ);
	pPlayerMesh->NumFramesDirty = gNumFrameResources;
}

void GameFramework::AnimateMaterials(float dt)
{

}

void GameFramework::UpdateObjectCBs(float dt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : WORLD.GetComponents<MeshComponent>())
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e.NumFramesDirty > 0)
		{
			e.generateWorldTransformMatrix();
			XMMATRIX world = XMLoadFloat4x4(&e.World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e.TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
			for (const auto& it : e.drawItems) {
				objConstants.MaterialIndex = it.Mat->MatCBIndex; // TO DO
			}
			currObjectCB->CopyData(e.ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e.NumFramesDirty--;
		}
	}

	auto currEquippedObjCB = mCurrFrameResource->EquippedObjectCB.get();
	for (auto& e : WORLD.GetComponents<EquippedComponent>())
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e.NumFramesDirty > 0)
		{
			XMMATRIX equippedBoneToRootTransfrom = XMLoadFloat4x4(&e.equippedBoneToRootTransfrom);

			EquippedObjectConstants equippedObjConstants;
			XMStoreFloat4x4(&equippedObjConstants.EquippedBoneFinalTransform, XMMatrixTranspose(equippedBoneToRootTransfrom));
			currEquippedObjCB->CopyData(e.EquippedCBIndex, equippedObjConstants);

			// Next FrameResource need to be updated too.
			e.NumFramesDirty--;
		}
	}
}

void GameFramework::UpdateSkinnedCBs(float dt)
{
	auto currSkinnedCB = mCurrFrameResource->SkinnedCB.get();

	// We only have one skinned model being animated.
	const auto& animationComponents = WORLD.GetComponents<AnimationComponent>();
	for (auto& comAni : animationComponents) {
		comAni.mSkinnedModelAnimationController->AdvanceTime(dt);

		SkinnedConstants skinnedConstants;
		std::copy(
			std::begin(comAni.mSkinnedModelAnimationController->finalTransforms),
			std::end(comAni.mSkinnedModelAnimationController->finalTransforms),
			&skinnedConstants.BoneTransforms[0]);

		currSkinnedCB->CopyData(comAni.SkinnedCBIndex, skinnedConstants);

		XMFLOAT3 rawTranslationDiff = comAni.mSkinnedModelAnimationController->GetRootBoneTranslationDiff();

		auto pComMesh = WORLD.GetComponent<MeshComponent>(comAni.GetOwner());
		if (pComMesh) {
			XMMATRIX transform = pComMesh->getScaleRotTransformOnlyMatrix();
			XMFLOAT3 translationDiff;
			XMStoreFloat3(
				&translationDiff,
				XMVector3Transform(XMLoadFloat3(&rawTranslationDiff), transform)
			);

			auto pComChar = WORLD.GetComponent<physics::CharacterBody>(comAni.GetOwner());
			if (pComChar) {
				// TODO: Handle animations marked as vertically movable
				physics::Vec3 offset{ translationDiff.x, 0, translationDiff.z };
				// We divide the offset by dt to make it a velocity
				pComChar->targetVelocity = offset / dt;
			}
			else {
				auto pComSimpleChar = WORLD.GetComponent<physics::SimpleCharacterBody>(comAni.GetOwner());
				if (pComSimpleChar) {
					// TODO: Handle animations marked as vertically movable
					physics::Vec3 offset{ translationDiff.x, 0, translationDiff.z };
					// We divide the offset by dt to make it a velocity
					pComSimpleChar->targetVelocity = offset / dt;
				}
				else {
					//XMFLOAT3 transformedPosition;
					//XMStoreFloat3(
					//	&transformedPosition,
					//	XMVector3Transform(XMLoadFloat3(&comAni.mSkinnedModelAnimationController->rootBoneTranslationResult), 
					//		transform)
					//);
					pComMesh->Translation = Vector3::Add(pComMesh->Translation, translationDiff);
					//pComMesh->Translation.y = transformedPosition.y;
					//pComMesh->Translation = Vector3::Add(pComMesh->Translation,
					//    XMFLOAT3(-pComMesh->Scale.x * translationDiff.x, 0, -pComMesh->Scale.z * translationDiff.z));
					pComMesh->NumFramesDirty = gNumFrameResources;
				}
			}
		}
	}

	for (auto& comEquipped : WORLD.GetComponents<EquippedComponent>()) {
		auto hEquippedObj = comEquipped.GetOwner();
		auto pEquippedObjMesh = WORLD.GetComponent<MeshComponent>(hEquippedObj);
		auto pEquipment = WORLD.GetComponent<EquipmentComponent>(hEquippedObj);

		auto pOwnerAnimation = WORLD.GetComponent<AnimationComponent>(comEquipped.owner);
		auto pOwnerMesh = WORLD.GetComponent<MeshComponent>(comEquipped.owner);
		if (pOwnerAnimation && pOwnerMesh) {
			const auto& equipmentLoc = pOwnerAnimation->mSkinnedModelAnimationController->animationSets->equipmentLocations[pEquipment->equipmentType];
			const auto& equipmentLocalTrans = equipmentLoc.localTransform;

			XMStoreFloat4x4(&comEquipped.equippedBoneToRootTransfrom, XMMatrixMultiply(
				XMLoadFloat4x4(&equipmentLocalTrans),
				XMLoadFloat4x4(&pOwnerAnimation->mSkinnedModelAnimationController->toRootTransforms[equipmentLoc.referenceBoneIndex])));
			comEquipped.NumFramesDirty = gNumFrameResources;
			pEquippedObjMesh->Scale = pOwnerMesh->Scale;
			pEquippedObjMesh->Rotation = pOwnerMesh->Rotation;
			pEquippedObjMesh->Translation = pOwnerMesh->Translation;
			pEquippedObjMesh->NumFramesDirty = gNumFrameResources;
		}
	}
}

void GameFramework::UpdateMaterialBuffer(float dt)
{
	auto currMaterialBuffer = mCurrFrameResource->MaterialBuffer.get();
	for (auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialData matData;
			matData.DiffuseAlbedo = mat->DiffuseAlbedo;
			matData.FresnelR0 = mat->FresnelR0;
			matData.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matData.MatTransform, XMMatrixTranspose(matTransform));
			matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;
			matData.NormalMapIndex = mat->NormalSrvHeapIndex;

			currMaterialBuffer->CopyData(mat->MatCBIndex, matData);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void GameFramework::UpdateShadowTransform(float dt)
{
	// Only the first "main" light casts a shadow.
	XMVECTOR lightDir = XMLoadFloat3(&mRotatedLightDirections[0]);
	XMVECTOR lightPos = -2.0f * mSceneBounds.Radius * lightDir;
	XMVECTOR targetPos = XMLoadFloat3(&mSceneBounds.Center);
	XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);

	XMStoreFloat3(&mLightPosW, lightPos);

	// Transform bounding sphere to light space.
	XMFLOAT3 sphereCenterLS;
	XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

	// Ortho frustum in light space encloses scene.
	float l = sphereCenterLS.x - mSceneBounds.Radius;
	float b = sphereCenterLS.y - mSceneBounds.Radius;
	float n = sphereCenterLS.z - mSceneBounds.Radius;
	float r = sphereCenterLS.x + mSceneBounds.Radius;
	float t = sphereCenterLS.y + mSceneBounds.Radius;
	float f = sphereCenterLS.z + mSceneBounds.Radius;

	mLightNearZ = n;
	mLightFarZ = f;
	XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	XMMATRIX S = lightView * lightProj * T;
	XMStoreFloat4x4(&mLightView, lightView);
	XMStoreFloat4x4(&mLightProj, lightProj);
	XMStoreFloat4x4(&mShadowTransform, S);
}

void GameFramework::UpdateMainPassCB(float dt)
{
	auto& camera = GetMainCamera();
	XMMATRIX view = camera.GetView();
	XMMATRIX proj = camera.GetProj();

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	auto viewVec = XMMatrixDeterminant(view);
	XMMATRIX invView = XMMatrixInverse(&viewVec, view);
	auto projVec = XMMatrixDeterminant(proj);
	XMMATRIX invProj = XMMatrixInverse(&projVec, proj);
	auto viewProjVec = XMMatrixDeterminant(viewProj);
	XMMATRIX invViewProj = XMMatrixInverse(&viewProjVec, viewProj);

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	XMMATRIX viewProjTex = XMMatrixMultiply(viewProj, T);
	XMMATRIX shadowTransform = XMLoadFloat4x4(&mShadowTransform);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProjTex, XMMatrixTranspose(viewProjTex));
	XMStoreFloat4x4(&mMainPassCB.ShadowTransform, XMMatrixTranspose(shadowTransform));
	mMainPassCB.EyePosW = camera.GetPosition3f();
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.DeltaTime = dt;
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.Lights[0].Direction = mRotatedLightDirections[0];
	mMainPassCB.Lights[0].Strength = { 0.9f, 0.9f, 0.7f };
	mMainPassCB.Lights[1].Direction = mRotatedLightDirections[1];
	mMainPassCB.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
	mMainPassCB.Lights[2].Direction = mRotatedLightDirections[2];
	mMainPassCB.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void GameFramework::UpdateShadowPassCB(float dt)
{
	XMMATRIX view = XMLoadFloat4x4(&mLightView);
	XMMATRIX proj = XMLoadFloat4x4(&mLightProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	auto viewVec = XMMatrixDeterminant(view);
	XMMATRIX invView = XMMatrixInverse(&viewVec, view);
	auto projVec = XMMatrixDeterminant(proj);
	XMMATRIX invProj = XMMatrixInverse(&projVec, proj);
	auto viewProjVec = XMMatrixDeterminant(viewProj);
	XMMATRIX invViewProj = XMMatrixInverse(&viewProjVec, viewProj);

	UINT w = mShadowMap->Width();
	UINT h = mShadowMap->Height();

	XMStoreFloat4x4(&mShadowPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mShadowPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mShadowPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mShadowPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mShadowPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mShadowPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mShadowPassCB.EyePosW = mLightPosW;
	mShadowPassCB.RenderTargetSize = XMFLOAT2((float)w, (float)h);
	mShadowPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / w, 1.0f / h);
	mShadowPassCB.NearZ = mLightNearZ;
	mShadowPassCB.FarZ = mLightFarZ;

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(1, mShadowPassCB);
}

void GameFramework::UpdateSsaoCB(float dt)
{
	SsaoConstants ssaoCB;

	auto& camera = GetMainCamera();
	XMMATRIX P = camera.GetProj();

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	ssaoCB.Proj = mMainPassCB.Proj;
	ssaoCB.InvProj = mMainPassCB.InvProj;
	XMStoreFloat4x4(&ssaoCB.ProjTex, XMMatrixTranspose(P * T));

	mSsao->GetOffsetVectors(ssaoCB.OffsetVectors);

	auto blurWeights = mSsao->CalcGaussWeights(2.5f);
	ssaoCB.BlurWeights[0] = XMFLOAT4(&blurWeights[0]);
	ssaoCB.BlurWeights[1] = XMFLOAT4(&blurWeights[4]);
	ssaoCB.BlurWeights[2] = XMFLOAT4(&blurWeights[8]);

	ssaoCB.InvRenderTargetSize = XMFLOAT2(1.0f / mSsao->SsaoMapWidth(), 1.0f / mSsao->SsaoMapHeight());

	// Coordinates given in view space.
	ssaoCB.OcclusionRadius = 0.5f;
	ssaoCB.OcclusionFadeStart = 0.2f;
	ssaoCB.OcclusionFadeEnd = 2.0f;
	ssaoCB.SurfaceEpsilon = 0.05f;

	auto currSsaoCB = mCurrFrameResource->SsaoCB.get();
	currSsaoCB->CopyData(0, ssaoCB);
}

void GameFramework::UpdateImGui()
{
	// Start the Dear ImGui frame
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	if (showStartMenu)
		return;

	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImGui::Begin("Parry Simulation Setting"); // Create a window called "Hello, world!" and append into it.

	ImGui::SliderFloat("Enemy Attack Animation Speed", &enemyAttackAnimationSpeed, 0.1f, 3.0f);
	ImGui::SliderFloat("Parry Judgement Frame Offset", &parryJudgementFrameOffset, -0.1f, 0.2f);
	ImGui::SliderFloat("Roll Judgement Frame Offset", &rollJudgementFrameOffset, -0.2f, 1.4f);
	ImGui::Checkbox("Player No Hit", &playerNoHit);
	ImGui::Checkbox("Show Enemy Valid Attack Frame", &showEnemyValidAttackFrame);
	ImGui::Checkbox("Show Player Valid Parry Frame", &showPlayerValidParryFrame);
	ImGui::Checkbox("Show Player Valid Roll Hit", &showPlayerValidRollHit);
	ImGui::Checkbox("Enemy Active", &bEnemyActive);
	ImGui::Checkbox("Show Pattern Editor", &showPatternEditor);

	ImGui::SeparatorText("Audio");
	auto& audio = AudioManager::GetInstance();
	float sfxVolume = audio.GetSfxVolume();
	float bgmVolume = audio.GetBgmVolume();
	if (ImGui::SliderFloat("SFX Volume", &sfxVolume, 0.0f, 1.0f))
		audio.SetSfxVolume(sfxVolume);
	if (ImGui::SliderFloat("Music Volume", &bgmVolume, 0.0f, 1.0f))
		audio.SetBgmVolume(bgmVolume);
	if (!audio.GetLastError().empty())
		ImGui::TextWrapped("Audio: %s", audio.GetLastError().c_str());

	ImGui::SeparatorText("Attack Type(Preparing)");
	static int e = 0;
	ImGui::RadioButton("One Hand Sword", &e, 0); ImGui::SameLine();
	ImGui::RadioButton("Two Hand Sword", &e, 1);

	if (e == 0)
		bOneHandMode = true;
	else if (e == 1 )
		bOneHandMode = false;

	ImGui::SeparatorText("Game World Simulation");
	if (ImGui::Button("Restart"))
		mRestartRequested = true;

	ImGui::End();
	DrawPatternEditor();
}

void GameFramework::DrawPatternEditor()
{
	if (!showPatternEditor)
		return;
	ImGui::SetNextWindowSize(ImVec2(760, 660), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(660, 540), ImVec2(FLT_MAX, FLT_MAX));
	if (!ImGui::Begin("Pattern Editor", &showPatternEditor))
	{
		ImGui::End();
		return;
	}

	auto& draft = patternSequenceDraft;
	auto& applied = enemyPatternSettings;
	ImGui::TextWrapped("Drag patterns into the sequence. Drag rows to reorder; drop in Remove to delete.");
	ImGui::TextDisabled("Caps Lock: release/capture the mouse. Changes take effect after Apply.");
	ImGui::Checkbox("Lock editing", &patternEditorLocked);
	ImGui::SameLine();
	ImGui::BeginDisabled(patternEditorLocked);
	ImGui::Checkbox("Fixed order", &draft.useFixedOrder);
	ImGui::SameLine();
	ImGui::BeginDisabled(!draft.useFixedOrder);
	ImGui::Checkbox("Repeat", &draft.repeat);
	ImGui::EndDisabled();
	ImGui::EndDisabled();

	// Queue edits while drawing: mutating a vector inside the row loop invalidates rows.
	EnemyPattern pendingAdd = EnemyPattern::None;
	int pendingMove = -1;
	int pendingRemove = -1;
	std::size_t destination = draft.entries.size();
	auto acceptDrop = [&](std::size_t gap)
	{
		if (patternEditorLocked || !ImGui::BeginDragDropTarget())
			return;
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENEMY_PATTERN_ADD"))
		{
			if (payload->DataSize == sizeof(EnemyPattern))
			{
				pendingAdd = *static_cast<const EnemyPattern*>(payload->Data);
				destination = gap;
			}
		}
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENEMY_PATTERN_MOVE"))
		{
			if (payload->DataSize == sizeof(int))
			{
				pendingMove = *static_cast<const int*>(payload->Data);
				destination = gap;
			}
		}
		ImGui::EndDragDropTarget();
	};

	const float listHeight = (std::max)(180.0f, ImGui::GetContentRegionAvail().y - 230.0f);
	if (ImGui::BeginTable("PatternColumns", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableSetupColumn("Available patterns", ImGuiTableColumnFlags_WidthStretch, 0.4f);
		ImGui::TableSetupColumn("Sequence", ImGuiTableColumnFlags_WidthStretch, 0.6f);
		ImGui::TableHeadersRow();
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::BeginChild("PatternLibrary", ImVec2(0, listHeight), ImGuiChildFlags_Borders);
		ImGui::BeginDisabled(patternEditorLocked);
		for (unsigned int value = static_cast<unsigned int>(EnemyPattern::EnemyAttack1);
			value <= static_cast<unsigned int>(EnemyPattern::Wait); ++value)
		{
			const auto pattern = static_cast<EnemyPattern>(value);
			ImGui::PushID(static_cast<int>(value));
			if (ImGui::Selectable(GetEnemyPatternLabel(pattern), false, ImGuiSelectableFlags_None, ImVec2(0, 26)))
				pendingAdd = pattern;
			if (!patternEditorLocked && ImGui::BeginDragDropSource())
			{
				ImGui::SetDragDropPayload("ENEMY_PATTERN_ADD", &pattern, sizeof(pattern));
				ImGui::Text("Add %s", GetEnemyPatternLabel(pattern));
				ImGui::EndDragDropSource();
			}
			ImGui::PopID();
		}
		ImGui::EndDisabled();
		ImGui::TextDisabled("Click also adds to the end.");
		ImGui::EndChild();

		ImGui::TableSetColumnIndex(1);
		ImGui::BeginChild("PatternSequence", ImVec2(0, listHeight), ImGuiChildFlags_Borders);
		ImGui::BeginDisabled(patternEditorLocked);
		for (std::size_t i = 0; i < draft.entries.size(); ++i)
		{
			const auto& entry = draft.entries[i];
			ImGui::PushID(entry.id);
			char label[100];
			snprintf(label, sizeof(label), "%zu. %s###PatternRow", i + 1, GetEnemyPatternLabel(entry.pattern));
			const float width = (std::max)(60.0f, ImGui::GetContentRegionAvail().x - 36.0f);
			ImGui::Button(label, ImVec2(width, 30));
			const auto rowMin = ImGui::GetItemRectMin();
			const auto rowMax = ImGui::GetItemRectMax();
			const std::size_t gap = ImGui::GetMousePos().y < (rowMin.y + rowMax.y) * 0.5f ? i : i + 1;
			if (!patternEditorLocked && ImGui::BeginDragDropSource())
			{
				ImGui::SetDragDropPayload("ENEMY_PATTERN_MOVE", &entry.id, sizeof(entry.id));
				ImGui::Text("Move %s", GetEnemyPatternLabel(entry.pattern));
				ImGui::EndDragDropSource();
			}
			acceptDrop(gap);
			if (!patternEditorLocked && ImGui::GetDragDropPayload() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
			{
				const float y = gap == i ? rowMin.y : rowMax.y;
				ImGui::GetWindowDrawList()->AddLine(ImVec2(rowMin.x, y), ImVec2(rowMax.x, y),
					ImGui::GetColorU32(ImGuiCol_DragDropTarget), 3.0f);
			}
			ImGui::SameLine();
			if (ImGui::Button("X", ImVec2(28, 30)))
				pendingRemove = entry.id;
			ImGui::PopID();
		}
		ImGui::Button(draft.entries.empty() ? "Drop first pattern here" : "Drop here to append", ImVec2(-1, 44));
		acceptDrop(draft.entries.size());
		ImGui::EndDisabled();
		ImGui::EndChild();
		ImGui::EndTable();
	}

	ImGui::BeginDisabled(patternEditorLocked);
	ImGui::Button("Remove: drop a sequence row here", ImVec2(-1, 34));
	if (!patternEditorLocked && ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENEMY_PATTERN_MOVE"))
		{
			if (payload->DataSize == sizeof(int))
				pendingRemove = *static_cast<const int*>(payload->Data);
		}
		ImGui::EndDragDropTarget();
	}
	if (pendingAdd != EnemyPattern::None)
		draft.entries.insert(draft.entries.begin() + destination, { nextPatternEntryId++, pendingAdd });
	else if (pendingMove != -1)
		MoveEnemyPatternEntry(draft.entries, pendingMove, destination);
	else if (pendingRemove != -1)
		std::erase_if(draft.entries, [pendingRemove](const EnemyPatternEntry& entry) { return entry.id == pendingRemove; });

	bool oneHandEnemy = false;
	auto& stateMachines = WORLD.GetComponents<StateMachineComponent>();
	if (stateMachines.size() > 1)
	{
		const auto equipment = WORLD.GetComponent<EquipmentOwnerComponent>(stateMachines[1].GetOwner());
		oneHandEnemy = equipment
			&& equipment->equipments[(UINT)EquipmentType::OneHandSword].has_value()
			&& !equipment->equipments[(UINT)EquipmentType::TwoHandSword].has_value();
	}
	const bool incompatible = !oneHandEnemy
		&& std::any_of(draft.entries.begin(), draft.entries.end(),
			[](const EnemyPatternEntry& entry) { return IsOneHandOnlyPattern(entry.pattern); });
	ImGui::BeginDisabled(incompatible);
	if (ImGui::Button("Apply"))
	{
		const auto revision = applied.revision + 1;
		applied = draft;
		applied.revision = revision;
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Revert draft"))
		draft = applied;
	ImGui::SameLine();
	if (ImGui::Button("Clear draft"))
		draft.entries.clear();
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!applied.useFixedOrder || applied.entries.empty());
	if (ImGui::Button("Restart applied"))
		++applied.revision;
	ImGui::EndDisabled();

	const bool dirty = draft.entries != applied.entries || draft.useFixedOrder != applied.useFixedOrder || draft.repeat != applied.repeat;
	ImGui::Text("Active: %s | %zu steps | %s%s", applied.useFixedOrder ? "Fixed order" : "Random",
		applied.entries.size(), !applied.useFixedOrder ? "Continuous" : (applied.repeat ? "Repeat" : "Play once"),
		dirty ? " | Unapplied changes" : "");
	if (incompatible)
		ImGui::TextWrapped("Cannot apply: Attack 1-3 and Offbeat require a one-hand enemy weapon. Use Combo or movement patterns.");
	else if (draft.entries.empty())
		ImGui::TextDisabled("An empty applied list keeps the enemy waiting in both modes.");
	else
		ImGui::TextDisabled("Apply takes effect after the current action. Fixed order starts at step 1.");
	ImGui::TextDisabled("Random: original distance/behavior AI; the applied list filters candidates.");
	ImGui::TextDisabled("Duplicate rows multiply that pattern's original AI weight. Repeat is fixed-order only.");
	ImGui::TextDisabled("Enable Enemy Active to run the applied list.");
	ImGui::TextDisabled("Sequence settings last for this session. Lock editing prevents accidental changes.");
	ImGui::End();
}

void GameFramework::LoadTextures()
{
	std::vector<std::string> texNames =
	{
		"startPageDiffuseMap",
		"bricksNormalMap",
		"tileDiffuseMap",
		"tileNormalMap",
		"defaultDiffuseMap",
		"defaultNormalMap",
		"skyCubeMap"
	};

	std::vector<std::wstring> texFilenames =
	{
		L"Textures/StartScreen.dds",
		L"Textures/bricks2_nmap.dds",
		L"Textures/tile.dds",
		L"Textures/tile_nmap.dds",
		L"Textures/white1x1.dds",
		L"Textures/default_nmap.dds",
		L"Textures/snowcube1024.dds"
	};

	// Add skinned model textures to list so we can reference by name later.
	for (UINT i = 0; i < mSkinnedMats.size(); ++i)
	{
		std::string diffuseName = mSkinnedMats[i].DiffuseMapName;
		std::string normalName = mSkinnedMats[i].NormalMapName;

		std::wstring diffuseFilename = L"Textures/" + AnsiToWString(diffuseName);
		std::wstring normalFilename = L"Textures/" + AnsiToWString(normalName);

		// strip off extension
		diffuseName = diffuseName.substr(0, diffuseName.find_last_of("."));
		normalName = normalName.substr(0, normalName.find_last_of("."));

		mSkinnedTextureNames.push_back(diffuseName);
		texNames.push_back(diffuseName);
		texFilenames.push_back(diffuseFilename);

		mSkinnedTextureNames.push_back(normalName);
		texNames.push_back(normalName);
		texFilenames.push_back(normalFilename);
	}

	for (int i = 0; i < (int)texNames.size(); ++i)
	{
		// Don't create duplicates.
		if (mTextures.find(texNames[i]) == std::end(mTextures))
		{
			auto texMap = std::make_unique<Texture>();
			texMap->Name = texNames[i];
			texMap->Filename = texFilenames[i];
			ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
				mCommandList.Get(), texMap->Filename.c_str(),
				texMap->Resource, texMap->UploadHeap));

			mTextures[texMap->Name] = std::move(texMap);
		}
	}
}

void GameFramework::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable0;
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 0);

	CD3DX12_DESCRIPTOR_RANGE texTable1;
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 48, 3, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[7];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsConstantBufferView(1);
	slotRootParameter[2].InitAsConstantBufferView(2);
	slotRootParameter[3].InitAsShaderResourceView(0, 1);
	slotRootParameter[4].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[5].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[6].InitAsConstantBufferView(3);

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(7, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void GameFramework::BuildSsaoRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable0;
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0);

	CD3DX12_DESCRIPTOR_RANGE texTable1;
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsConstants(1, 1);
	slotRootParameter[2].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[3].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC depthMapSam(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
		0.0f,
		0,
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	std::array<CD3DX12_STATIC_SAMPLER_DESC, 4> staticSamplers =
	{
		pointClamp, linearClamp, depthMapSam, linearWrap
	};

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mSsaoRootSignature.GetAddressOf())));
}

void GameFramework::BuildDescriptorHeaps()
{
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 84;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	std::vector<ComPtr<ID3D12Resource>> tex2DList =
	{
		mTextures["startPageDiffuseMap"]->Resource,
		mTextures["bricksNormalMap"]->Resource,
		mTextures["tileDiffuseMap"]->Resource,
		mTextures["tileNormalMap"]->Resource,
		mTextures["defaultDiffuseMap"]->Resource,
		mTextures["defaultNormalMap"]->Resource
	};

	mSkinnedSrvHeapStart = (UINT)tex2DList.size();

	for (UINT i = 0; i < (UINT)mSkinnedTextureNames.size(); ++i)
	{
		auto texResource = mTextures[mSkinnedTextureNames[i]]->Resource;
		assert(texResource != nullptr);
		tex2DList.push_back(texResource);
	}


	auto skyCubeMap = mTextures["skyCubeMap"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	for (UINT i = 0; i < (UINT)tex2DList.size(); ++i)
	{
		srvDesc.Format = tex2DList[i]->GetDesc().Format;
		srvDesc.Texture2D.MipLevels = tex2DList[i]->GetDesc().MipLevels;
		md3dDevice->CreateShaderResourceView(tex2DList[i].Get(), &srvDesc, hDescriptor);

		// next descriptor
		hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	}

	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = skyCubeMap->GetDesc().MipLevels;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	srvDesc.Format = skyCubeMap->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(skyCubeMap.Get(), &srvDesc, hDescriptor);

	auto hSkyObj = WORLD.AddObject();
	SkyComponent comSky = SkyComponent(hSkyObj);
	comSky.mSkyTexHeapIndex = (UINT)tex2DList.size();
	WORLD.AttachComponent(hSkyObj, move(comSky));

	mShadowMapHeapIndex = (UINT)tex2DList.size() + 1;
	mSsaoHeapIndexStart = mShadowMapHeapIndex + 1;
	mSsaoAmbientMapIndex = mSsaoHeapIndexStart + 3;
	mNullCubeSrvIndex = mSsaoHeapIndexStart + 5;
	mNullTexSrvIndex1 = mNullCubeSrvIndex + 1;
	mNullTexSrvIndex2 = mNullTexSrvIndex1 + 1;
	mParticleHeapIndexStart = mNullTexSrvIndex2 + 1;

	auto nullSrv = GetCpuSrv(mNullCubeSrvIndex);
	mNullSrv = GetGpuSrv(mNullCubeSrvIndex);

	md3dDevice->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);
	nullSrv.Offset(1, mCbvSrvUavDescriptorSize);

	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	md3dDevice->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);

	nullSrv.Offset(1, mCbvSrvUavDescriptorSize);
	md3dDevice->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);

	mShadowMap->BuildDescriptors(
		GetCpuSrv(mShadowMapHeapIndex),
		GetGpuSrv(mShadowMapHeapIndex),
		GetDsv(1));

	mSsao->BuildDescriptors(
		mDepthStencilBuffer.Get(),
		GetCpuSrv(mSsaoHeapIndexStart),
		GetGpuSrv(mSsaoHeapIndexStart),
		GetRtv(SwapChainBufferCount),
		mCbvSrvUavDescriptorSize,
		mRtvDescriptorSize);

	mParticleManager.BuildDescriptors(
		GetCpuSrv(mParticleHeapIndexStart),
		GetGpuSrv(mParticleHeapIndexStart),
		mCbvSrvUavDescriptorSize
	);
}

void GameFramework::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO skinnedDefines[] =
	{
		"SKINNED", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO equippedDefines[] =
	{
		"EQUIPPED", "1",
		NULL, NULL
	};

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Core\\Shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skinnedVS"] = d3dUtil::CompileShader(L"Core\\Shaders\\Default.hlsl", skinnedDefines, "VS", "vs_5_1");
	mShaders["equippedVS"] = d3dUtil::CompileShader(L"Core\\Shaders\\Default.hlsl", equippedDefines, "VS", "vs_5_1");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Core\\Shaders\\Default.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["skinnedPS"] = d3dUtil::CompileShader(L"Core\\Shaders\\Default.hlsl", skinnedDefines, "PS_Simple", "ps_5_1");
	mShaders["equippedPS"] = d3dUtil::CompileShader(L"Core\\Shaders\\Default.hlsl", equippedDefines, "PS_Simple", "ps_5_1");

	mShaders["shadowVS"] = d3dUtil::CompileShader(L"Core\\Shaders\\Shadows.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skinnedShadowVS"] = d3dUtil::CompileShader(L"Core\\Shaders\\Shadows.hlsl", skinnedDefines, "VS", "vs_5_1");
	mShaders["equippedShadowVS"] = d3dUtil::CompileShader(L"Core\\Shaders\\Shadows.hlsl", equippedDefines, "VS", "vs_5_1");
	mShaders["shadowOpaquePS"] = d3dUtil::CompileShader(L"Core\\Shaders\\Shadows.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["shadowAlphaTestedPS"] = d3dUtil::CompileShader(L"Core\\Shaders\\Shadows.hlsl", alphaTestDefines, "PS", "ps_5_1");

	mShaders["debugVS"] = d3dUtil::CompileShader(L"Core\\Shaders\\ShadowDebug.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["debugPS"] = d3dUtil::CompileShader(L"Core\\Shaders\\ShadowDebug.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["drawNormalsVS"] = d3dUtil::CompileShader(L"Core\\Shaders\\DrawNormals.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skinnedDrawNormalsVS"] = d3dUtil::CompileShader(L"Core\\Shaders\\DrawNormals.hlsl", skinnedDefines, "VS", "vs_5_1");
	mShaders["equippedDrawNormalsVS"] = d3dUtil::CompileShader(L"Core\\Shaders\\DrawNormals.hlsl", equippedDefines, "VS", "vs_5_1");
	mShaders["drawNormalsPS"] = d3dUtil::CompileShader(L"Core\\Shaders\\DrawNormals.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["ssaoVS"] = d3dUtil::CompileShader(L"Core\\Shaders\\Ssao.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["ssaoPS"] = d3dUtil::CompileShader(L"Core\\Shaders\\Ssao.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["ssaoBlurVS"] = d3dUtil::CompileShader(L"Core\\Shaders\\SsaoBlur.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["ssaoBlurPS"] = d3dUtil::CompileShader(L"Core\\Shaders\\SsaoBlur.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["skyVS"] = d3dUtil::CompileShader(L"Core\\Shaders\\Sky.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skyPS"] = d3dUtil::CompileShader(L"Core\\Shaders\\Sky.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["startPageVS"] = d3dUtil::CompileShader(L"Core\\Shaders\\DrawStartPage.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["startPagePS"] = d3dUtil::CompileShader(L"Core\\Shaders\\DrawStartPage.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	mSkinnedInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "WEIGHTS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONEINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 56, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void GameFramework::BuildShapeGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(100.0f, 100.0f, 120, 120);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);
	GeometryGenerator::MeshData quad = geoGen.CreateQuad(0.0f, 0.0f, 1.0f, 1.0f, 0.0f);

	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();
	UINT quadVertexOffset = cylinderVertexOffset + (UINT)cylinder.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();
	UINT quadIndexOffset = cylinderIndexOffset + (UINT)cylinder.Indices32.size();

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

	SubmeshGeometry quadSubmesh;
	quadSubmesh.IndexCount = (UINT)quad.Indices32.size();
	quadSubmesh.StartIndexLocation = quadIndexOffset;
	quadSubmesh.BaseVertexLocation = quadVertexOffset;

	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//

	auto totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		sphere.Vertices.size() +
		cylinder.Vertices.size() +
		quad.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;
		vertices[k].TangentU = box.Vertices[i].TangentU;
	}

	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].TexC = grid.Vertices[i].TexC;
		vertices[k].TangentU = grid.Vertices[i].TangentU;
	}

	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;
		vertices[k].TangentU = sphere.Vertices[i].TangentU;
	}

	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;
		vertices[k].TangentU = cylinder.Vertices[i].TangentU;
	}

	for (int i = 0; i < quad.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = quad.Vertices[i].Position;
		vertices[k].Normal = quad.Vertices[i].Normal;
		vertices[k].TexC = quad.Vertices[i].TexC;
		vertices[k].TangentU = quad.Vertices[i].TangentU;
	}

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));
	indices.insert(indices.end(), std::begin(quad.GetIndices16()), std::end(quad.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;
	geo->DrawArgs["quad"] = quadSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void GameFramework::LoadSkinnedModel()
{
	// Load Humanoid Asset
	std::vector<SkinnedVertex> vertices;
	std::vector<UINT> indices32bit;
	std::vector<FbxBinLoader::SubMeshInfo> skinnedSubsets;

	mSkinnedAnimationSets.emplace(
		std::piecewise_construct,
		std::forward_as_tuple(HumanoidAssets::AssetName),
		std::forward_as_tuple(std::make_unique<AnimationSets>()));

	fbxBinLoader.LoadFbxBin(HumanoidAssets::BaseModelFileName, vertices, indices32bit,
		skinnedSubsets, mSkinnedAnimationSets[HumanoidAssets::AssetName].get());

	for (const string& animationFileName : HumanoidAssets::AnimationFileNames) {
		std::vector<SkinnedVertex> tmpVertices;
		std::vector<UINT> tmpIndices32bit;
		std::vector<FbxBinLoader::SubMeshInfo> tmpSkinnedSubsets;
		AnimationSets animationSets;
		fbxBinLoader.LoadFbxBin(animationFileName, tmpVertices, tmpIndices32bit,
			tmpSkinnedSubsets, &animationSets);

		mSkinnedAnimationSets[HumanoidAssets::AssetName].get()->rawAnimationSets.merge(move(animationSets.rawAnimationSets));
	}

	std::vector<AnimationType> changeEndTime = {
		AnimationType::OneHandBaseAttackTypeC_1,
		AnimationType::OneHandBaseAttackTypeC_2,
		AnimationType::OneHandBaseAttackTypeC_3,
		AnimationType::OneHandBaseAttackTypeC_4,
		AnimationType::OneHandBaseAttackTypeC_5,
		AnimationType::OneHandBaseAttackTypeC_6,
		AnimationType::OneHandBaseAttackTypeC_7,
	};

	auto animationSet = mSkinnedAnimationSets[HumanoidAssets::AssetName].get();
	animationSet->generateAnimationSet(HumanoidAssets::MoveTypeToName);
	animationSet->SetAnimationEffectiveTimeInfos(&HumanoidAssets::AnimationEffectiveTimeInfos);

	//for (auto& it : changeEndTime) {
	//	animationSet->animationSets[it]->startTime = 0.200000f;
	//	animationSet->animationSets[it]->endTime = animationSet->animationSets[it]->endTime / 2;
	//}
	animationSet->animationSets[AnimationType::TwoHandRollForward]->endTime = 1.133333f;
	animationSet->animationSets[AnimationType::TwoHandRollForwardRight45]->endTime = 1.133333f;
	animationSet->animationSets[AnimationType::TwoHandRollForwardLeft45]->endTime = 1.133333f;
	animationSet->animationSets[AnimationType::TwoHandRollBackwardRight45]->endTime = 1.133333f;
	animationSet->animationSets[AnimationType::TwoHandRollBackwardLeft45]->endTime = 1.133333f;
	animationSet->animationSets[AnimationType::TwoHandRollRight]->endTime = 1.133333f;
	animationSet->animationSets[AnimationType::TwoHandRollLeft]->endTime = 1.133333f;
	animationSet->animationSets[AnimationType::TwoHandRollBackward]->endTime = 1.133333f;
	
	animationSet->animationSets[AnimationType::OneHandBaseAttackTypeC_1]->endTime = 0.766667f;

	animationSet->animationSets[AnimationType::OneHandBaseAttackTypeC_2]->startTime = 0.166667f;
	animationSet->animationSets[AnimationType::OneHandBaseAttackTypeC_2]->endTime = 0.833333f;

	animationSet->animationSets[AnimationType::OneHandBaseAttackTypeC_3]->startTime = 0.133333f;
	animationSet->animationSets[AnimationType::OneHandBaseAttackTypeC_3]->endTime = 0.666667f;

	animationSet->animationSets[AnimationType::OneHandBaseAttackTypeC_4]->startTime = 0.133333f;
	animationSet->animationSets[AnimationType::OneHandBaseAttackTypeC_4]->endTime = 0.666667f;

	animationSet->animationSets[AnimationType::OneHandBaseAttackTypeC_5]->startTime = 0.133333f;
	animationSet->animationSets[AnimationType::OneHandBaseAttackTypeC_5]->endTime = 0.633333f;

	animationSet->animationSets[AnimationType::OneHandBaseAttackTypeC_6]->startTime = 0.300000f;
	animationSet->animationSets[AnimationType::OneHandBaseAttackTypeC_6]->endTime = 1.000000f;

	animationSet->animationSets[AnimationType::OneHandBaseAttackTypeC_7]->startTime = 0.133333f;

	animationSet->animationSets[AnimationType::TwoHandBaseAttackTypeB_1]->endTime = 1.166667f;
	animationSet->animationSets[AnimationType::TwoHandBaseAttackTypeB_2]->startTime = 0.300000f;
	animationSet->animationSets[AnimationType::TwoHandBaseAttackTypeB_2]->endTime = 0.933333f;
	animationSet->animationSets[AnimationType::TwoHandBaseAttackTypeB_3]->startTime = 0.300000f;


	animationSet->animationSets[AnimationType::Parried]->startTime = 0.7f;
	animationSet->animationSets[AnimationType::Parried]->endTime = 1.2f;

	XMVECTOR quat = MathHelper::QuaternionFromPithYawRoll(-2.0f, -0.1f, -0.5f);
	animationSet->equipmentLocations[EquipmentType::OneHandSword] = EquipmentLocation(
		37, XMMatrixTranslation(-2.0f, -0.5f, -7.0f) * XMMatrixRotationQuaternion(quat)
	);

	XMVECTOR tquat = MathHelper::QuaternionFromPithYawRoll(-1.7f, .0f, .0f);
	animationSet->equipmentLocations[EquipmentType::TwoHandSword] = EquipmentLocation(
		37, XMMatrixTranslation(-2.5f, -4.0f, -8.0f) * XMMatrixRotationQuaternion(tquat)
	);
	
	animationSet->equipmentLocations[EquipmentType::Shield] = EquipmentLocation(
		12, XMMatrixScaling(0.7f, 0.7f, 0.7f) * XMMatrixRotationZ(-45.0f) * XMMatrixRotationY(-90.0f)
		* XMMatrixTranslation(-3.0f, 10.0f, 0.0f)
	);

	std::vector<std::uint16_t> indices;
	indices.reserve(indices32bit.size());
	for (UINT index : indices32bit) {
		indices.push_back(index);
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(SkinnedVertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = HumanoidAssets::AssetName;

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(SkinnedVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	for (UINT i = 0; i < (UINT)skinnedSubsets.size(); ++i)
	{
		SubmeshGeometry submesh;
		std::string name(move(skinnedSubsets[i].name));

		submesh.IndexCount = (UINT)skinnedSubsets[i].indexCount;
		submesh.StartIndexLocation = skinnedSubsets[i].startIndexLocation;
		submesh.BaseVertexLocation = skinnedSubsets[i].baseVertexLocation;

		geo->DrawArgs[name] = submesh;
	}

	mGeometries[geo->Name] = std::move(geo);

	mSkinnedMats.resize(skinnedSubsets.size());
	mSkinnedMats[0].Name = std::string("soldier_head");
	mSkinnedMats[0].DiffuseAlbedo = { 0.7f, 0.7f, 0.7f, 1.0f };
	mSkinnedMats[0].FresnelR0 = { 0.05f, 0.05f, 0.05f };
	mSkinnedMats[0].Roughness = 0.5;
	mSkinnedMats[0].AlphaClip = false;
	mSkinnedMats[0].MaterialTypeName = std::string("Skinned");
	mSkinnedMats[0].DiffuseMapName = std::string("checkboard.dds");
	mSkinnedMats[0].NormalMapName = std::string("upBody_norm.dds");

	mSkinnedMats[1].Name = std::string("soldier_upBody");
	mSkinnedMats[1].DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	mSkinnedMats[1].FresnelR0 = { 0.05f, 0.05f, 0.05f };
	mSkinnedMats[1].Roughness = 0.4;
	mSkinnedMats[1].AlphaClip = false;
	mSkinnedMats[1].MaterialTypeName = std::string("Skinned");
	mSkinnedMats[1].DiffuseMapName = std::string("checkboard.dds");
	mSkinnedMats[1].NormalMapName = std::string("upBody_norm.dds");

	mSkinnedMats[2].Name = std::string("soldier_upBody2");
	mSkinnedMats[2].DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	mSkinnedMats[2].FresnelR0 = { 0.05f, 0.05f, 0.05f };
	mSkinnedMats[2].Roughness = 0.3;
	mSkinnedMats[2].AlphaClip = false;
	mSkinnedMats[2].MaterialTypeName = std::string("Skinned");
	mSkinnedMats[2].DiffuseMapName = std::string("checkboard.dds");
	mSkinnedMats[2].NormalMapName = std::string("upBody_norm.dds");
}

void GameFramework::BuildFbxModelGeometry()
{
	auto equipmentGeo = std::make_unique<MeshGeometry>();
	equipmentGeo->Name = "equipmentGeo";

	std::vector<Vertex> oneHandSwordVertices;
	std::vector<UINT> oneHandSwordIndices32bit;
	std::vector<FbxBinLoader::SubMeshInfo> oneHandSwordSubsets;
	std::string oneHandSwordFileName = "Models\\OneHandSword.bin";
	fbxBinLoader.LoadFbxBin(oneHandSwordFileName,
		oneHandSwordVertices, oneHandSwordIndices32bit, oneHandSwordSubsets);

	std::vector<Vertex> shieldVertices;
	std::vector<UINT> shieldIndices32bit;
	std::vector<FbxBinLoader::SubMeshInfo> shieldSubsets;
	std::string shieldFileName = "Models\\Shield.bin";
	fbxBinLoader.LoadFbxBin(shieldFileName,
		shieldVertices, shieldIndices32bit, shieldSubsets);

	std::vector<Vertex> equipmentVertices;
	equipmentVertices.insert(equipmentVertices.end(), std::begin(oneHandSwordVertices), std::end(oneHandSwordVertices));
	equipmentVertices.insert(equipmentVertices.end(), std::begin(shieldVertices), std::end(shieldVertices));

	std::vector<std::uint16_t> equipmentIndices;
	equipmentIndices.insert(equipmentIndices.end(), std::begin(oneHandSwordIndices32bit), std::end(oneHandSwordIndices32bit));
	equipmentIndices.insert(equipmentIndices.end(), std::begin(shieldIndices32bit), std::end(shieldIndices32bit));

	const UINT vbByteSize = (UINT)equipmentVertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)equipmentIndices.size() * sizeof(std::uint16_t);

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &equipmentGeo->VertexBufferCPU));
	CopyMemory(equipmentGeo->VertexBufferCPU->GetBufferPointer(), equipmentVertices.data(), vbByteSize);
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &equipmentGeo->IndexBufferCPU));
	CopyMemory(equipmentGeo->IndexBufferCPU->GetBufferPointer(), equipmentIndices.data(), ibByteSize);

	equipmentGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), equipmentVertices.data(), vbByteSize, equipmentGeo->VertexBufferUploader);

	equipmentGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), equipmentIndices.data(), ibByteSize, equipmentGeo->IndexBufferUploader);

	equipmentGeo->VertexByteStride = sizeof(Vertex);
	equipmentGeo->VertexBufferByteSize = vbByteSize;
	equipmentGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	equipmentGeo->IndexBufferByteSize = ibByteSize;

	for (auto& subset : oneHandSwordSubsets)
	{
		SubmeshGeometry submesh;
		std::string name("OneHandSword" + subset.name);

		submesh.IndexCount = (UINT)subset.indexCount;
		submesh.StartIndexLocation = subset.startIndexLocation;
		submesh.BaseVertexLocation = subset.baseVertexLocation;

		equipmentGeo->DrawArgs[name] = submesh;
	}

	for (auto& subset : shieldSubsets)
	{
		SubmeshGeometry submesh;
		std::string name("Shield" + subset.name);

		submesh.IndexCount = (UINT)subset.indexCount;
		submesh.StartIndexLocation = subset.startIndexLocation + oneHandSwordIndices32bit.size();
		submesh.BaseVertexLocation = subset.baseVertexLocation + oneHandSwordVertices.size();

		equipmentGeo->DrawArgs[name] = submesh;
	}

	mGeometries[equipmentGeo->Name] = std::move(equipmentGeo);
}

void GameFramework::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC equippedOpaquePsoDesc = opaquePsoDesc;
	equippedOpaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["equippedVS"]->GetBufferPointer()),
		mShaders["equippedVS"]->GetBufferSize()
	};
	equippedOpaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["equippedPS"]->GetBufferPointer()),
		mShaders["equippedPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&equippedOpaquePsoDesc, IID_PPV_ARGS(&mPSOs["equippedOpaque"])));

	//
	// PSO for skinned pass.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedOpaquePsoDesc = opaquePsoDesc;
	skinnedOpaquePsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
	skinnedOpaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["skinnedVS"]->GetBufferPointer()),
		mShaders["skinnedVS"]->GetBufferSize()
	};
	skinnedOpaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["skinnedPS"]->GetBufferPointer()),
		mShaders["skinnedPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedOpaquePsoDesc, IID_PPV_ARGS(&mPSOs["skinnedOpaque"])));

	//
	// PSO for shadow map pass.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC smapPsoDesc = opaquePsoDesc;
	smapPsoDesc.RasterizerState.DepthBias = 100000;
	smapPsoDesc.RasterizerState.DepthBiasClamp = 0.0f;
	smapPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
	smapPsoDesc.pRootSignature = mRootSignature.Get();
	smapPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["shadowVS"]->GetBufferPointer()),
		mShaders["shadowVS"]->GetBufferSize()
	};
	smapPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["shadowOpaquePS"]->GetBufferPointer()),
		mShaders["shadowOpaquePS"]->GetBufferSize()
	};

	// Shadow map pass does not have a render target.
	smapPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	smapPsoDesc.NumRenderTargets = 0;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&smapPsoDesc, IID_PPV_ARGS(&mPSOs["shadow_opaque"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedSmapPsoDesc = smapPsoDesc;
	skinnedSmapPsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
	skinnedSmapPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["skinnedShadowVS"]->GetBufferPointer()),
		mShaders["skinnedShadowVS"]->GetBufferSize()
	};
	skinnedSmapPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["shadowOpaquePS"]->GetBufferPointer()),
		mShaders["shadowOpaquePS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedSmapPsoDesc, IID_PPV_ARGS(&mPSOs["skinnedShadow_opaque"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC equippedSmapPsoDesc = smapPsoDesc;
	equippedSmapPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["equippedShadowVS"]->GetBufferPointer()),
		mShaders["equippedShadowVS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&equippedSmapPsoDesc, IID_PPV_ARGS(&mPSOs["equippedShadow_opaque"])));

	//
	// PSO for debug layer.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPsoDesc = opaquePsoDesc;
	debugPsoDesc.pRootSignature = mRootSignature.Get();
	debugPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["debugVS"]->GetBufferPointer()),
		mShaders["debugVS"]->GetBufferSize()
	};
	debugPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["debugPS"]->GetBufferPointer()),
		mShaders["debugPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&debugPsoDesc, IID_PPV_ARGS(&mPSOs["debug"])));

	//
	// PSO for drawing normals.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC drawNormalsPsoDesc = opaquePsoDesc;
	drawNormalsPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["drawNormalsVS"]->GetBufferPointer()),
		mShaders["drawNormalsVS"]->GetBufferSize()
	};
	drawNormalsPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["drawNormalsPS"]->GetBufferPointer()),
		mShaders["drawNormalsPS"]->GetBufferSize()
	};
	drawNormalsPsoDesc.RTVFormats[0] = Ssao::NormalMapFormat;
	drawNormalsPsoDesc.SampleDesc.Count = 1;
	drawNormalsPsoDesc.SampleDesc.Quality = 0;
	drawNormalsPsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&drawNormalsPsoDesc, IID_PPV_ARGS(&mPSOs["drawNormals"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedDrawNormalsPsoDesc = drawNormalsPsoDesc;
	skinnedDrawNormalsPsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
	skinnedDrawNormalsPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["skinnedDrawNormalsVS"]->GetBufferPointer()),
		mShaders["skinnedDrawNormalsVS"]->GetBufferSize()
	};
	skinnedDrawNormalsPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["drawNormalsPS"]->GetBufferPointer()),
		mShaders["drawNormalsPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedDrawNormalsPsoDesc, IID_PPV_ARGS(&mPSOs["skinnedDrawNormals"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC equippedDrawNormalsPsoDesc = drawNormalsPsoDesc;
	equippedDrawNormalsPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["equippedDrawNormalsVS"]->GetBufferPointer()),
		mShaders["equippedDrawNormalsVS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&equippedDrawNormalsPsoDesc, IID_PPV_ARGS(&mPSOs["equippedDrawNormals"])));


	//
	// PSO for SSAO.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC ssaoPsoDesc = opaquePsoDesc;
	ssaoPsoDesc.InputLayout = { nullptr, 0 };
	ssaoPsoDesc.pRootSignature = mSsaoRootSignature.Get();
	ssaoPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["ssaoVS"]->GetBufferPointer()),
		mShaders["ssaoVS"]->GetBufferSize()
	};
	ssaoPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["ssaoPS"]->GetBufferPointer()),
		mShaders["ssaoPS"]->GetBufferSize()
	};

	// SSAO effect does not need the depth buffer.
	ssaoPsoDesc.DepthStencilState.DepthEnable = false;
	ssaoPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	ssaoPsoDesc.RTVFormats[0] = Ssao::AmbientMapFormat;
	ssaoPsoDesc.SampleDesc.Count = 1;
	ssaoPsoDesc.SampleDesc.Quality = 0;
	ssaoPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&ssaoPsoDesc, IID_PPV_ARGS(&mPSOs["ssao"])));

	//
	// PSO for SSAO blur.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC ssaoBlurPsoDesc = ssaoPsoDesc;
	ssaoBlurPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["ssaoBlurVS"]->GetBufferPointer()),
		mShaders["ssaoBlurVS"]->GetBufferSize()
	};
	ssaoBlurPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["ssaoBlurPS"]->GetBufferPointer()),
		mShaders["ssaoBlurPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&ssaoBlurPsoDesc, IID_PPV_ARGS(&mPSOs["ssaoBlur"])));

	//
	// PSO for sky.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = opaquePsoDesc;

	// The camera is inside the sky sphere, so just turn off culling.
	skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	// Make sure the depth function is LESS_EQUAL and not just LESS.  
	// Otherwise, the normalized depth values at z = 1 (NDC) will 
	// fail the depth test if the depth buffer was cleared to 1.
	skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	skyPsoDesc.pRootSignature = mRootSignature.Get();
	skyPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["skyVS"]->GetBufferPointer()),
		mShaders["skyVS"]->GetBufferSize()
	};
	skyPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["skyPS"]->GetBufferPointer()),
		mShaders["skyPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSOs["sky"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC startPagePsoDesc = opaquePsoDesc;
	startPagePsoDesc.pRootSignature = mRootSignature.Get();
	startPagePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["startPageVS"]->GetBufferPointer()),
		mShaders["startPageVS"]->GetBufferSize()
	};
	startPagePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["startPagePS"]->GetBufferPointer()),
		mShaders["startPagePS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&startPagePsoDesc, IID_PPV_ARGS(&mPSOs["startPage"])));
}

void GameFramework::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			2, (UINT)WORLD.GetComponents<MeshComponent>().size(),
			(UINT)WORLD.GetComponents<AnimationComponent>().size(),
			(UINT)mMaterials.size(),
			(UINT)(WORLD.GetComponents<EquipmentOwnerComponent>().size() * (UINT)EquipmentType::Count), ParticleManager::MAX_BILLBOARD_PARTICLES
		));
	}
}

void GameFramework::BuildMaterials()
{
	auto bricks0 = std::make_unique<Material>();
	bricks0->Name = "bricks0";
	bricks0->MatCBIndex = 0;
	bricks0->DiffuseSrvHeapIndex = 0;
	bricks0->NormalSrvHeapIndex = 1;
	bricks0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks0->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	bricks0->Roughness = 0.3f;

	auto tile0 = std::make_unique<Material>();
	tile0->Name = "tile0";
	tile0->MatCBIndex = 1;
	tile0->DiffuseSrvHeapIndex = 2;
	tile0->NormalSrvHeapIndex = 3;
	tile0->DiffuseAlbedo = XMFLOAT4(0.9f, 0.9f, 0.9f, 1.0f);
	tile0->FresnelR0 = XMFLOAT3(0.2f, 0.2f, 0.2f);
	tile0->Roughness = 0.1f;

	auto mirror0 = std::make_unique<Material>();
	mirror0->Name = "mirror0";
	mirror0->MatCBIndex = 2;
	mirror0->DiffuseSrvHeapIndex = 4;
	mirror0->NormalSrvHeapIndex = 5;
	mirror0->DiffuseAlbedo = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mirror0->FresnelR0 = XMFLOAT3(0.98f, 0.97f, 0.95f);
	mirror0->Roughness = 0.1f;

	auto sky = std::make_unique<Material>();
	sky->Name = "sky";
	sky->MatCBIndex = 3;
	sky->DiffuseSrvHeapIndex = 6;
	sky->NormalSrvHeapIndex = 7;
	sky->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	sky->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	sky->Roughness = 1.0f;

	mMaterials["bricks0"] = std::move(bricks0);
	mMaterials["tile0"] = std::move(tile0);
	mMaterials["mirror0"] = std::move(mirror0);
	mMaterials["sky"] = std::move(sky);

	UINT matCBIndex = 4;
	UINT srvHeapIndex = mSkinnedSrvHeapStart;
	for (UINT i = 0; i < mSkinnedMats.size(); ++i)
	{
		auto mat = std::make_unique<Material>();
		mat->Name = mSkinnedMats[i].Name;
		mat->MatCBIndex = matCBIndex++;
		mat->DiffuseSrvHeapIndex = srvHeapIndex++;
		mat->NormalSrvHeapIndex = srvHeapIndex++;
		mat->DiffuseAlbedo = mSkinnedMats[i].DiffuseAlbedo;
		mat->FresnelR0 = mSkinnedMats[i].FresnelR0;
		mat->Roughness = mSkinnedMats[i].Roughness;

		mMaterials[mat->Name] = std::move(mat);
	}

	auto equipment = std::make_unique<Material>();
	equipment->Name = "equipment";
	equipment->MatCBIndex = matCBIndex++;
	equipment->DiffuseSrvHeapIndex = 4;
	equipment->NormalSrvHeapIndex = -1;
	equipment->DiffuseAlbedo = XMFLOAT4(0.5f, 0.7f, 1.0f, 1.0f);
	equipment->FresnelR0 = XMFLOAT3(0.98f, 0.97f, 0.95f);
	equipment->Roughness = 0.5f;
	mMaterials["equipment"] = std::move(equipment);

	auto equipment2 = std::make_unique<Material>();
	equipment2->Name = "equipment2";
	equipment2->MatCBIndex = matCBIndex++;
	equipment2->DiffuseSrvHeapIndex = 4;
	equipment2->NormalSrvHeapIndex = -1;
	equipment2->DiffuseAlbedo = XMFLOAT4(0.5f, 0.f, 0.f, 1.0f);
	equipment2->FresnelR0 = XMFLOAT3(0.98f, 0.97f, 0.95f);
	equipment2->Roughness = 0.5f;
	mMaterials["equipment2"] = std::move(equipment2);

	auto equipment3 = std::make_unique<Material>();
	equipment3->Name = "equipment3";
	equipment3->MatCBIndex = matCBIndex++;
	equipment3->DiffuseSrvHeapIndex = 4;
	equipment3->NormalSrvHeapIndex = -1;
	equipment3->DiffuseAlbedo = XMFLOAT4(4.0f, 0.02f, 0.02f, 1.0f);
	equipment3->FresnelR0 = XMFLOAT3(1.0f, 0.0f, 0.0f);
	equipment3->Roughness = 0.15f;
	mMaterials["equipment3"] = std::move(equipment3);


	auto equipment4 = std::make_unique<Material>();
	equipment4->Name = "equipment4";
	equipment4->MatCBIndex = matCBIndex++;
	equipment4->DiffuseSrvHeapIndex = 4;
	equipment4->NormalSrvHeapIndex = -1;
	equipment4->DiffuseAlbedo = XMFLOAT4(0.02f, 3.0f, 4.0f, 1.0f);
	equipment4->FresnelR0 = XMFLOAT3(0.0f, 0.7f, 1.0f);
	equipment4->Roughness = 0.15f;
	mMaterials["equipment4"] = std::move(equipment4);

	auto hit = std::make_unique<Material>();
	hit->Name = "hit";
	hit->MatCBIndex = matCBIndex++;
	hit->DiffuseSrvHeapIndex = 4;
	hit->NormalSrvHeapIndex = -1;
	hit->DiffuseAlbedo = XMFLOAT4(1.0f, 0.60f, 0.6f, 1.0f);
	hit->FresnelR0 = XMFLOAT3(0.98f, 0.97f, 0.95f);
	hit->Roughness = 0.5f;

	mMaterials["hit"] = std::move(hit);
}

void GameFramework::BuildRenderItems()
{
	using namespace physics;
	using namespace JPH;

	auto& bodyInterface = PHYSICS_SYSTEM.GetBodyInterfaceNoLock();
	UINT objCBIndexCounter = 0;

	auto& skyComponents = WORLD.GetComponents<SkyComponent>();
	for (auto& skyCom : skyComponents) {
		MeshComponent comSkyMesh{ skyCom.GetOwner(), objCBIndexCounter++,
			XMFLOAT3(5000.0f, 5000.0f, 5000.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f),
			XMMatrixIdentity(),
			{mGeometries["shapeGeo"].get()}, {"sphere"}, { mMaterials["sky"].get() }
		};
		WORLD.AttachComponent(skyCom.GetOwner(), std::move(comSkyMesh));
	}

	auto hQuadObj = WORLD.AddObject();
	MeshComponent comQuadMesh{ hQuadObj, objCBIndexCounter++,
		XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f),
		XMMatrixIdentity(),
		{mGeometries["shapeGeo"].get()}, {"quad"}, { mMaterials["bricks0"].get() }
	};
	WORLD.AttachComponent(hQuadObj, std::move(comQuadMesh));
	WORLD.AttachComponent(hQuadObj, NonAnimatedComponent(hQuadObj));
	StaticCompoundShapeSettings compoundShapeSettings;
	ShapeRefC pPlaneShape{ new BoxShape{ Vec3{50.f, 0.1f, 50.f} } };
	compoundShapeSettings.AddShape(Vec3(0.f, -0.1f, 0.f), Quat::sIdentity(), pPlaneShape);
	compoundShapeSettings.AddShape(Vec3(50.f, 0.f, 0.f), Quat::sRotation(Vec3::sAxisZ(), MathHelper::Pi / 2.f), pPlaneShape);
	compoundShapeSettings.AddShape(Vec3(-50.f, 0.f, 0.f), Quat::sRotation(-Vec3::sAxisZ(), MathHelper::Pi / 2.f), pPlaneShape);
	compoundShapeSettings.AddShape(Vec3(0.f, 0.f, 50.f), Quat::sRotation(-Vec3::sAxisX(), MathHelper::Pi / 2.f), pPlaneShape);
	compoundShapeSettings.AddShape(Vec3(0.f, 0.f, -50.f), Quat::sRotation(Vec3::sAxisX(), MathHelper::Pi / 2.f), pPlaneShape);
	StaticBody comQuadBody{ hQuadObj, bodyInterface, compoundShapeSettings.Create().Get(), Vec3::sZero(), Quat::sIdentity() };
	WORLD.AttachComponent(hQuadObj, std::move(comQuadBody));

	//auto hBoxObj = WORLD.AddObject();
	//MeshComponent comBoxMesh(hBoxObj, objCBIndexCounter++,
	//	XMFLOAT3(2.0f, 1.0f, 2.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.5f, 0.0f),
	//	XMMatrixScaling(1.0f, 1.0f, 1.0f),
	//	{ mGeometries["shapeGeo"].get() }, { "box" }, { mMaterials["bricks0"].get() }
	//);
	//WORLD.AttachComponent(hBoxObj, std::move(comBoxMesh));
	//WORLD.AttachComponent(hBoxObj, NonAnimatedComponent(hBoxObj));

	auto hGridObj = WORLD.AddObject();
	MeshComponent comGridMesh(hGridObj, objCBIndexCounter++,
		XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f),
		XMMatrixScaling(8.0f, 8.0f, 1.0f),
		{ mGeometries["shapeGeo"].get() }, { "grid" }, { mMaterials["tile0"].get() }
	);
	WORLD.AttachComponent(hGridObj, std::move(comGridMesh));
	WORLD.AttachComponent(hGridObj, NonAnimatedComponent(hGridObj));

	//XMMATRIX brickTexTransform = XMMatrixScaling(1.5f, 2.0f, 1.0f);
	//for (int i = 0; i < 5; ++i)
	//{
	//	auto hLeftCylObj = WORLD.AddObject();
	//	auto hRightCylObj = WORLD.AddObject();
	//	auto hLeftSphereObj = WORLD.AddObject();
	//	auto hRightSphereObj = WORLD.AddObject();

	//	XMFLOAT3 leftCylWorldTranslation = XMFLOAT3(-5.0f, 1.5f, -10.0f + i * 5.0f);
	//	XMFLOAT3 rightCylWorldTranslation = XMFLOAT3(+5.0f, 1.5f, -10.0f + i * 5.0f);

	//	XMFLOAT3 leftSphereWorldTranslation = XMFLOAT3(-5.0f, 3.5f, -10.0f + i * 5.0f);
	//	XMFLOAT3 rightSphereWorldTranslation = XMFLOAT3(+5.0f, 3.5f, -10.0f + i * 5.0f);

	//	MeshComponent comLeftCyleMesh(hLeftCylObj, objCBIndexCounter++,
	//		XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), leftCylWorldTranslation,
	//		brickTexTransform,
	//		{ mGeometries["shapeGeo"].get() }, { "cylinder" }, { mMaterials["bricks0"].get() }
	//	);
	//	WORLD.AttachComponent(hLeftCylObj, std::move(comLeftCyleMesh));
	//	WORLD.AttachComponent(hLeftCylObj, NonAnimatedComponent(hLeftCylObj));

	//	MeshComponent comRightCyleMesh(hRightCylObj, objCBIndexCounter++,
	//		XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), rightCylWorldTranslation,
	//		brickTexTransform,
	//		{ mGeometries["shapeGeo"].get() }, { "cylinder" }, { mMaterials["bricks0"].get() }
	//	);
	//	WORLD.AttachComponent(hRightCylObj, std::move(comRightCyleMesh));
	//	WORLD.AttachComponent(hRightCylObj, NonAnimatedComponent(hRightCylObj));

	//	MeshComponent comLeftSphereMesh(hLeftCylObj, objCBIndexCounter++,
	//		XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), leftSphereWorldTranslation,
	//		XMMatrixIdentity(),
	//		{ mGeometries["shapeGeo"].get() }, { "sphere" }, { mMaterials["mirror0"].get() }
	//	);
	//	WORLD.AttachComponent(hLeftSphereObj, std::move(comLeftSphereMesh));
	//	WORLD.AttachComponent(hLeftSphereObj, NonAnimatedComponent(hLeftSphereObj));

	//	MeshComponent comRightSphereMesh(hLeftCylObj, objCBIndexCounter++,
	//		XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), rightSphereWorldTranslation,
	//		XMMatrixIdentity(),
	//		{ mGeometries["shapeGeo"].get() }, { "sphere" }, { mMaterials["mirror0"].get() }
	//	);
	//	WORLD.AttachComponent(hRightSphereObj, std::move(comRightSphereMesh));
	//	WORLD.AttachComponent(hRightSphereObj, NonAnimatedComponent(hRightSphereObj));
	//}

	UINT skinnedCBIndexCounter = 0;
	UINT equippedCBIndexCounter = 0;

	// Build Player
	auto hPlayerObj = WORLD.AddObject();
	auto pPlayerObj = WORLD.GetGameObject(hPlayerObj);
	pPlayerObj->AddChild(WORLD, GetMainCamera().GetOwner());
	WORLD.AttachComponent(hPlayerObj, PlayerComponent(hPlayerObj));

	XMFLOAT3 playerOffset = PlayerSpawnPosition;
	XMFLOAT3 playerRot = CharacterSpawnRotation;
	BuildHumanoidItem<CharacterBody>(
		hPlayerObj,
		skinnedCBIndexCounter,
		equippedCBIndexCounter,
		mMaterials[mSkinnedMats[0].Name].get(),
		playerOffset,
		playerRot,
		objCBIndexCounter,
		mMaterials["equipment"].get(),
		true,
		true,
		false
	);

	InitializeCharacterStateMachine(hPlayerObj, true);

	// Build Enemy
	auto hEnemyObj = WORLD.AddObject();
	XMFLOAT3 ememyOffset = EnemySpawnPosition;
	XMFLOAT3 ememyRot = CharacterSpawnRotation;
	BuildHumanoidItem(
		hEnemyObj,
		skinnedCBIndexCounter,
		equippedCBIndexCounter,
		mMaterials["mirror0"].get(),
		ememyOffset,
		ememyRot,
		objCBIndexCounter,
		mMaterials["equipment2"].get(),
		true,
		false,
		false
	);
	InitializeCharacterStateMachine(hEnemyObj, false);


}

void GameFramework::InitializeCharacterStateMachine(Handle<GameObject> owner, bool isPlayer)
{
	std::unordered_map<StateType, std::unique_ptr<CharacterState>> states;
	if (isPlayer)
	{
		states[StateType::Idle] = std::make_unique<PlayerIdleState>();
		states[StateType::Walk] = std::make_unique<PlayerWalkState>();
		states[StateType::Roll] = std::make_unique<PlayerRollState>();
		states[StateType::Attack] = std::make_unique<PlayerAttackState>();
		states[StateType::Parry] = std::make_unique<PlayerParryState>();
		states[StateType::Hit] = std::make_unique<PlayerHitState>();
		states[StateType::Parried] = std::make_unique<PlayerParriedState>();
	}
	else
	{
		states[StateType::Idle] = std::make_unique<EnemyIdleState>();
		states[StateType::Attack] = std::make_unique<EnemyAttackState>(enemyAttackAnimationSpeed);
		states[StateType::Hit] = std::make_unique<EnemyHitState>();
		states[StateType::Parried] = std::make_unique<EnemyParriedState>();
		states[StateType::Walk] = std::make_unique<EnemyWalkState>();
		states[StateType::Pattern] = std::make_unique<EnemyPatternState>(enemyPatternSettings);
	}
	if (auto stateMachine = WORLD.GetComponent<StateMachineComponent>(owner))
		stateMachine->Reset(move(states), StateType::Idle);
	else
		WORLD.AttachComponent(owner, StateMachineComponent{ owner, move(states), StateType::Idle });
}

void GameFramework::RestartGame()
{
	using namespace physics;
	for (auto& stateMachine : WORLD.GetComponents<StateMachineComponent>())
	{
		const auto owner = stateMachine.GetOwner();
		const bool isPlayer = WORLD.GetComponent<PlayerComponent>(owner) != nullptr;
		const auto& spawn = isPlayer ? PlayerSpawnPosition : EnemySpawnPosition;
		const RVec3 position{ spawn.x, spawn.y, spawn.z };
		if (auto body = WORLD.GetComponent<CharacterBody>(owner))
		{
			body->targetVelocity = Vec3::sZero();
			body->desiredVelocity = Vec3::sZero();
			body->allowSliding = false;
			body->inJump = false;
			body->GetController().SetPosition(position);
			body->GetController().SetRotation(Quat::sIdentity());
			body->GetController().SetLinearVelocity(Vec3::sZero());
		}
		if (auto body = WORLD.GetComponent<SimpleCharacterBody>(owner))
		{
			body->targetVelocity = Vec3::sZero();
			body->GetController().SetPositionAndRotation(position, Quat::sIdentity());
			body->GetController().SetLinearAndAngularVelocity(Vec3::sZero(), Vec3::sZero());
			body->GetController().PostSimulation(0.05f);
		}
		auto mesh = WORLD.GetComponent<MeshComponent>(owner);
		mesh->Translation = spawn;
		mesh->Rotation = CharacterSpawnRotation;
		mesh->NumFramesDirty = gNumFrameResources;
		if (isPlayer)
			for (auto& drawItem : mesh->drawItems)
				drawItem.Mat = mMaterials[mSkinnedMats[0].Name].get();

		// Discard old blends/root motion and recreate states to reset combo and pattern cursors.
		auto animation = WORLD.GetComponent<AnimationComponent>(owner);
		animation->mSkinnedModelAnimationController =
			std::make_unique<AnimationController>(mSkinnedAnimationSets[HumanoidAssets::AssetName].get());
		InitializeCharacterStateMachine(owner, isPlayer);
	}

	mPlayerHitTime = 0.0f;
	directionlock = false;
	directionlockTime = 0.0f;
	mTargetLocked = false;
	mParticleManager.Clear();
	AudioManager::GetInstance().StopSfx();
	InputManager::GetInstance().Reset();
	WORLD.GetComponent<AttachedCamera>(GetMainCamera().GetOwner())->offset = Vec3{ 0.f, 2.f, -13.f };
	physics::UpdateAttachedCamera();
	GetMainCamera().UpdateViewMatrix();
}

void GameFramework::BuildEquipmentItem(
	Handle<GameObject> ownerHandle,
	UINT& objCBIndexCounter,
	EquipmentOwnerComponent& comEquipmentOwner,
	EquipmentType equipmentType,
	Material* pMaterial,
	bool equip
) {
	const char* subMeshNames[(UINT)EquipmentType::Count] = {
		"OneHandSword0",
		"Shield0",
		"OneHandSword0"
	};
	auto hEquipmentObj = WORLD.AddObject();
	MeshComponent comMesh(hEquipmentObj, objCBIndexCounter++,
		XMFLOAT3(0.03f, 0.03f, 0.03f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f),
		XMMatrixIdentity(),
		{ mGeometries["equipmentGeo"].get() }, { subMeshNames[(UINT)equipmentType] }, { pMaterial }
	);
	auto comEquipment = EquipmentComponent(hEquipmentObj);
	comEquipment.equipmentType = equipmentType;
	WORLD.AttachComponent(hEquipmentObj, std::move(comMesh));
	WORLD.AttachComponent(hEquipmentObj, std::move(comEquipment));

	if (equip) {
		auto comEquipped = EquippedComponent(hEquipmentObj); // for rendering
		comEquipped.EquippedCBIndex = comEquipmentOwner.equippedCBIndices[(UINT)equipmentType];
		comEquipped.owner = ownerHandle;
		WORLD.AttachComponent(hEquipmentObj, std::move(comEquipped));
		comEquipmentOwner.equipments[(UINT)equipmentType] = hEquipmentObj;
	}

	comEquipmentOwner.equipmentStorage[(UINT)equipmentType] = hEquipmentObj;
}

void GameFramework::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<AnimationComponent>& ritems)
{
	auto skinnedCB = mCurrFrameResource->SkinnedCB->Resource();

	// For each render item...
	for (const auto& ritem : ritems)
	{
		auto hOwner = ritem.GetOwner();
		auto pMesh = WORLD.GetComponent<MeshComponent>(hOwner);

		if (pMesh) {
			D3D12_GPU_VIRTUAL_ADDRESS skinnedCBAddress = skinnedCB->GetGPUVirtualAddress() + ritem.SkinnedCBIndex * skinnedCBByteSize;
			cmdList->SetGraphicsRootConstantBufferView(1, skinnedCBAddress);
			DrawRenderItem(cmdList, pMesh);
		}
	}
}

void GameFramework::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<EquippedComponent>& ritems)
{
	auto equippedObjectCB = mCurrFrameResource->EquippedObjectCB->Resource();

	// For each render item...
	for (const auto& ritem : ritems)
	{
		auto hOwner = ritem.GetOwner();
		auto pMesh = WORLD.GetComponent<MeshComponent>(hOwner);

		if (pMesh) {
			D3D12_GPU_VIRTUAL_ADDRESS equippedCBAddress = equippedObjectCB->GetGPUVirtualAddress() + ritem.EquippedCBIndex * equippedObjCBByteSize;
			cmdList->SetGraphicsRootConstantBufferView(6, equippedCBAddress);
			DrawRenderItem(cmdList, pMesh);
		}
	}
}

void GameFramework::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<NonAnimatedComponent>& ritems)
{
	// For each render item...
	for (const auto& ritem : ritems)
	{
		auto hOwner = ritem.GetOwner();
		auto pMesh = WORLD.GetComponent<MeshComponent>(hOwner);
		if (pMesh) {
			cmdList->SetGraphicsRootConstantBufferView(1, 0);
			DrawRenderItem(cmdList, pMesh);
		}
	}
}

void GameFramework::DrawRenderItem(ID3D12GraphicsCommandList* cmdList, const MeshComponent const* ritem)
{
	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ritem->ObjCBIndex * objCBByteSize;

	cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);

	for (const auto& di : ritem->drawItems) {
		auto vbv = di.Geo->VertexBufferView();
		auto ibv = di.Geo->IndexBufferView();
		cmdList->IASetVertexBuffers(0, 1, &vbv);
		cmdList->IASetIndexBuffer(&ibv);
		cmdList->IASetPrimitiveTopology(di.PrimitiveType);

		cmdList->DrawIndexedInstanced(di.IndexCount, 1, di.StartIndexLocation, di.BaseVertexLocation, 0);
	}

}

void GameFramework::DrawSceneToShadowMap()
{
	auto vp = mShadowMap->Viewport();
	mCommandList->RSSetViewports(1, &vp);
	auto sr = mShadowMap->ScissorRect();
	mCommandList->RSSetScissorRects(1, &sr);

	// Change to DEPTH_WRITE.
	auto grToDw = CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->Resource(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	mCommandList->ResourceBarrier(1, &grToDw);

	// Clear the back buffer and depth buffer.
	mCommandList->ClearDepthStencilView(mShadowMap->Dsv(),
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	auto dsv = mShadowMap->Dsv();
	mCommandList->OMSetRenderTargets(0, nullptr, false, &dsv);

	// Bind the pass constant buffer for the shadow map pass.
	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));
	auto passCB = mCurrFrameResource->PassCB->Resource();
	D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = passCB->GetGPUVirtualAddress() + 1 * passCBByteSize;
	mCommandList->SetGraphicsRootConstantBufferView(2, passCBAddress);

	mCommandList->SetPipelineState(mPSOs["shadow_opaque"].Get());
	DrawRenderItems(mCommandList.Get(), WORLD.GetComponents<NonAnimatedComponent>());

	mCommandList->SetPipelineState(mPSOs["equippedShadow_opaque"].Get());
	DrawRenderItems(mCommandList.Get(), WORLD.GetComponents<EquippedComponent>());

	mCommandList->SetPipelineState(mPSOs["skinnedShadow_opaque"].Get());
	DrawRenderItems(mCommandList.Get(), WORLD.GetComponents<AnimationComponent>());

	// Change back to GENERIC_READ so we can read the texture in a shader.
	auto dwToRg = CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->Resource(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
	mCommandList->ResourceBarrier(1, &dwToRg);
}

void GameFramework::DrawNormalsAndDepth()
{
	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	auto normalMap = mSsao->NormalMap();
	auto normalMapRtv = mSsao->NormalMapRtv();

	// Change to RENDER_TARGET.
	auto grToRt = CD3DX12_RESOURCE_BARRIER::Transition(normalMap,
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mCommandList->ResourceBarrier(1, &grToRt);

	// Clear the screen normal map and depth buffer.
	float clearValue[] = { 0.0f, 0.0f, 1.0f, 0.0f };
	mCommandList->ClearRenderTargetView(normalMapRtv, clearValue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	auto dsv = DepthStencilView();
	mCommandList->OMSetRenderTargets(1, &normalMapRtv, true, &dsv);

	// Bind the constant buffer for this pass.
	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

	mCommandList->SetPipelineState(mPSOs["drawNormals"].Get());
	DrawRenderItems(mCommandList.Get(), WORLD.GetComponents<NonAnimatedComponent>());

	mCommandList->SetPipelineState(mPSOs["equippedDrawNormals"].Get());
	DrawRenderItems(mCommandList.Get(), WORLD.GetComponents<EquippedComponent>());

	mCommandList->SetPipelineState(mPSOs["skinnedDrawNormals"].Get());
	DrawRenderItems(mCommandList.Get(), WORLD.GetComponents<AnimationComponent>());

	// Change back to GENERIC_READ so we can read the texture in a shader.
	auto rtToGr = CD3DX12_RESOURCE_BARRIER::Transition(normalMap,
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
	mCommandList->ResourceBarrier(1, &rtToGr);
}
CD3DX12_CPU_DESCRIPTOR_HANDLE GameFramework::GetCpuSrv(int index)const
{
	auto srv = CD3DX12_CPU_DESCRIPTOR_HANDLE(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	srv.Offset(index, mCbvSrvUavDescriptorSize);
	return srv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE GameFramework::GetGpuSrv(int index)const
{
	auto srv = CD3DX12_GPU_DESCRIPTOR_HANDLE(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	srv.Offset(index, mCbvSrvUavDescriptorSize);
	return srv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE GameFramework::GetDsv(int index)const
{
	auto dsv = CD3DX12_CPU_DESCRIPTOR_HANDLE(mDsvHeap->GetCPUDescriptorHandleForHeapStart());
	dsv.Offset(index, mDsvDescriptorSize);
	return dsv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE GameFramework::GetRtv(int index)const
{
	auto rtv = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	rtv.Offset(index, mRtvDescriptorSize);
	return rtv;
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GameFramework::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC shadow(
		6, // shaderRegister
		D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
		0.0f,                               // mipLODBias
		16,                                 // maxAnisotropy
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp,
		shadow
	};
}
