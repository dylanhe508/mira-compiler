#include <windows.h>
#include <GL/gl.h>
#include <math.h>
#include <stdio.h>

/* Force NVIDIA Optimus to use the high-performance GPU */
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;

static int g_running = 1;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_DESTROY || msg == WM_CLOSE) {
		g_running = 0;
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static HWND g_hwnd = NULL;

__declspec(dllexport) void mira_gl_init_3d(int w, int h) {
	WNDCLASSA wc = {0};
	wc.lpfnWndProc   = WndProc;
	wc.hInstance     = GetModuleHandleA(NULL);
	wc.lpszClassName = "MiraGL3DWindow";
	RegisterClassA(&wc);

	HWND hwnd = CreateWindowExA(0, "MiraGL3DWindow", "Mira 3D Cube - FPS: Calculating...",
					WS_OVERLAPPEDWINDOW | WS_VISIBLE,
					CW_USEDEFAULT, CW_USEDEFAULT, w, h,
					NULL, NULL, wc.hInstance, NULL);
	g_hwnd = hwnd;

	HDC hdc = GetDC(hwnd);

	PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR), 1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
		PFD_TYPE_RGBA, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 24, 8, 0, PFD_MAIN_PLANE, 0, 0, 0, 0
	};

	int pf = ChoosePixelFormat(hdc, &pfd);
	SetPixelFormat(hdc, pf, &pfd);

	HGLRC hrc = wglCreateContext(hdc);
	wglMakeCurrent(hdc, hrc);

	/* Try to disable VSync if the extension is supported */
	typedef BOOL (WINAPI *PFNWGLSWAPINTERVALEXTPROC)(int);
	PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
	if (wglSwapIntervalEXT) {
		wglSwapIntervalEXT(0);
	}

	glViewport(0, 0, w, h);
	glEnable(GL_DEPTH_TEST);

	/* Set up perspective projection */
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	double fov = 45.0;
	double aspect = (double)w / (double)h;
	double zNear = 0.1;
	double zFar = 100.0;
	
	double fH = tan(fov / 360 * 3.14159) * zNear;
	double fW = fH * aspect;
	glFrustum(-fW, fW, -fH, fH, zNear, zFar);
	
	/* Back to modelview matrix */
	glMatrixMode(GL_MODELVIEW);

	/* Store for swap */
	SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)hdc);
	
	char info[512];
	sprintf(info, "GL_VENDOR: %s\nGL_RENDERER: %s\nGL_VERSION: %s", 
	        glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION));
	MessageBoxA(hwnd, info, "OpenGL Info", MB_OK | MB_ICONINFORMATION);
}

__declspec(dllexport) void mira_gl_clear_3d(int r, int g, int b) {
	glClearColor(r/255.0f, g/255.0f, b/255.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

__declspec(dllexport) void mira_gl_cube_draw(int xRot, int yRot, int zRot) {
	glLoadIdentity();
	/* Translate back so we can see it */
	glTranslatef(0.0f, 0.0f, -5.0f);
	
	glRotatef((float)xRot, 1.0f, 0.0f, 0.0f);
	glRotatef((float)yRot, 0.0f, 1.0f, 0.0f);
	glRotatef((float)zRot, 0.0f, 0.0f, 1.0f);

	glBegin(GL_QUADS);
	/* Front Face (Red) */
	glColor3f(1.0f, 0.0f, 0.0f);
	glVertex3f(-1.0f, -1.0f,  1.0f);
	glVertex3f( 1.0f, -1.0f,  1.0f);
	glVertex3f( 1.0f,  1.0f,  1.0f);
	glVertex3f(-1.0f,  1.0f,  1.0f);
	/* Back Face (Green) */
	glColor3f(0.0f, 1.0f, 0.0f);
	glVertex3f(-1.0f, -1.0f, -1.0f);
	glVertex3f(-1.0f,  1.0f, -1.0f);
	glVertex3f( 1.0f,  1.0f, -1.0f);
	glVertex3f( 1.0f, -1.0f, -1.0f);
	/* Top Face (Blue) */
	glColor3f(0.0f, 0.0f, 1.0f);
	glVertex3f(-1.0f,  1.0f, -1.0f);
	glVertex3f(-1.0f,  1.0f,  1.0f);
	glVertex3f( 1.0f,  1.0f,  1.0f);
	glVertex3f( 1.0f,  1.0f, -1.0f);
	/* Bottom Face (Yellow) */
	glColor3f(1.0f, 1.0f, 0.0f);
	glVertex3f(-1.0f, -1.0f, -1.0f);
	glVertex3f( 1.0f, -1.0f, -1.0f);
	glVertex3f( 1.0f, -1.0f,  1.0f);
	glVertex3f(-1.0f, -1.0f,  1.0f);
	/* Right face (Magenta) */
	glColor3f(1.0f, 0.0f, 1.0f);
	glVertex3f( 1.0f, -1.0f, -1.0f);
	glVertex3f( 1.0f,  1.0f, -1.0f);
	glVertex3f( 1.0f,  1.0f,  1.0f);
	glVertex3f( 1.0f, -1.0f,  1.0f);
	/* Left Face (Cyan) */
	glColor3f(0.0f, 1.0f, 1.0f);
	glVertex3f(-1.0f, -1.0f, -1.0f);
	glVertex3f(-1.0f, -1.0f,  1.0f);
	glVertex3f(-1.0f,  1.0f,  1.0f);
	glVertex3f(-1.0f,  1.0f, -1.0f);
	glEnd();
}

__declspec(dllexport) void mira_gl_swap_3d() {
	HDC hdc = wglGetCurrentDC();
	SwapBuffers(hdc);
	
	static DWORD lIRTime = 0;
	static int frames = 0;
	DWORD currentTime = GetTickCount();
	frames++;
	if (currentTime - lIRTime >= 1000) {
		if (g_hwnd) {
			char title[128];
			sprintf(title, "Mira 3D Cube - %d FPS", frames);
			SetWindowTextA(g_hwnd, title);
		}
		frames = 0;
		lIRTime = currentTime;
	}
}

__declspec(dllexport) int mira_gl_is_running() {
	return g_running;
}

static int internal_rot_x = 0;
static int internal_rot_y = 0;
static int internal_rot_z = 0;
__declspec(dllexport) void mira_gl_draw_auto() {
	internal_rot_x += 2;
	internal_rot_y += 3;
	internal_rot_z += 1;
	mira_gl_cube_draw(internal_rot_x, internal_rot_y, internal_rot_z);
}

__declspec(dllexport) void mira_gl_sleep_events(int ms) {
	DWORD start = GetTickCount();
	MSG msg;
	while (GetTickCount() - start < (DWORD)ms) {
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (msg.message == WM_QUIT) {
				g_running = 0;
				return;
			}
		}
		/* Sleep(1); -- 砍掉这一行，体验原生态“狂暴模式�?*/
	}
}
