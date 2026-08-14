/* libs-dll/lwmgl.c
   Mira OpenGL Extension (LightWeight Mira Graphics Library)
   Compile: gcc -shared -O3 -o lwmgl.dll lwmgl.c -lgdi32 -lopengl32

   娉ㄦ剰锛歁ira 涓嶆敮鎸佹诞鐐规暟瀛楅潰閲忥紝鎵€浠ユ墍鏈?API 鍏ㄩ儴浣跨敤鏁存暟锛?
   棰滆壊鍊肩敤 0~255锛屽潗鏍囩敤鍍忕礌鍊硷紝鍐呴儴杞崲�?OpenGL 娴偣銆?
*/

#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>
#include <math.h>

/* Global state */
static HWND g_hwnd = NULL;
static HDC g_hdc = NULL;
static HGLRC g_hrc = NULL;
static int g_width = 800;
static int g_height = 600;

/* Prefer discrete GPU if available */
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;
	case WM_DESTROY:
		return 0;
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

/*
 * lwmgl_init(width, height)
 * 鍒涘�?OpenGL 绐楀�?
 */
__declspec(dllexport) void lwmgl_init(int width, int height) {
	g_width = width;
	g_height = height;

	HINSTANCE hInstance = GetModuleHandle(NULL);
	WNDCLASSA wc = {0};
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = "MiraGLWindow";
	wc.style = CS_OWNDC;
	RegisterClassA(&wc);

	g_hwnd = CreateWindowExA(
		0, "MiraGLWindow", "LWMGL (LightWeight Mira Graphics Library)",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, width, height,
		NULL, NULL, hInstance, NULL
	);

	if (!g_hwnd) return;

	g_hdc = GetDC(g_hwnd);

	PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR), 1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
		PFD_TYPE_RGBA, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		24, 8, 0, PFD_MAIN_PLANE, 0, 0, 0, 0
	};
	int pixelFormat = ChoosePixelFormat(g_hdc, &pfd);
	SetPixelFormat(g_hdc, pixelFormat, &pfd);

	g_hrc = wglCreateContext(g_hdc);
	if (!g_hrc) return;
	wglMakeCurrent(g_hdc, g_hrc);

	ShowWindow(g_hwnd, SW_SHOWDEFAULT);
	SetForegroundWindow(g_hwnd);
	UpdateWindow(g_hwnd);

	glViewport(0, 0, width, height);
}

/*
 * lwmgl_clear(r, g, b)
 * 娓呭睆锛岄鑹插€艰寖鍥?0~255
 */
__declspec(dllexport) void lwmgl_clear(int r, int g, int b) {
	glClearColor(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

/*
 * lwmgl_color(r, g, b)
 * 璁剧疆褰撳墠缁樺埗棰滆壊锛岃寖鍥?0~255
 */
__declspec(dllexport) void lwmgl_color(int r, int g, int b) {
	glColor3f(r / 255.0f, g / 255.0f, b / 255.0f);
}

/*
 * lwmgl_rect(x, y, w, h)
 * 画矩形，像素坐标（左上角为原点）
 */
__declspec(dllexport) void lwmgl_rect(int x, int y, int w, int h) {
	/* 灏嗗儚绱犲潗鏍囪浆涓?OpenGL NDC (-1 ~ +1) */
	float x1 = (2.0f * x / g_width) - 1.0f;
	float y1 = 1.0f - (2.0f * y / g_height);          /* Y 杞寸炕杞?*/
	float x2 = (2.0f * (x + w) / g_width) - 1.0f;
	float y2 = 1.0f - (2.0f * (y + h) / g_height);
	glRectf(x1, y2, x2, y1);
}

/*
 * lwmgl_swap()
 * 浜ゆ崲缂撳啿骞跺鐞嗙獥鍙ｆ秷鎭?
 */
__declspec(dllexport) void lwmgl_swap(void) {
	SwapBuffers(g_hdc);

	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) exit(0);
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

/*
 * lwmgl_close()
 * 閲婃�?OpenGL 涓婁笅鏂囧苟閿€姣佺獥鍙?
 */
__declspec(dllexport) void lwmgl_close(void) {
	if (g_hrc) { wglMakeCurrent(NULL, NULL); wglDeleteContext(g_hrc); g_hrc = NULL; }
	if (g_hdc && g_hwnd) { ReleaseDC(g_hwnd, g_hdc); g_hdc = NULL; }
	if (g_hwnd) { DestroyWindow(g_hwnd); g_hwnd = NULL; }
	/* 澶勭悊娈嬬暀娑堟伅鍚庣洿鎺ラ€€鍑猴紝閬垮厤 opengl32.dll 鍗歌浇姝婚攣 */
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	ExitProcess(0);
}

/*
 * lwmgl_sleep(ms)
 * 绛夊緟鎸囧畾姣锛屽悓鏃朵繚鎸佺獥鍙ｅ搷搴旓紙涓嶄細杞湀鍦堬紒锛?
 */
__declspec(dllexport) void lwmgl_sleep(int ms) {
	DWORD start = GetTickCount();
	do {
		MSG msg;
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) { lwmgl_close(); return; }
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		Sleep(1);
	} while ((GetTickCount() - start) < (DWORD)ms);
}

/* lwmgl_enable_3d() */
__declspec(dllexport) void lwmgl_enable_3d(void) {
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float fovY = 45.0f * 3.14159f / 180.0f;
    float aspect = (float)g_width / (float)g_height;
    float f = 1.0f / (float)tan((double)fovY / 2.0);
    float zNear = 0.1f, zFar = 100.0f;
    float m[16] = {0};
    m[0] = f / aspect; m[5] = f; m[10] = (zFar + zNear) / (zNear - zFar); m[11] = -1.0f; m[14] = (2.0f * zFar * zNear) / (zNear - zFar);
    glLoadMatrixf(m);
    glMatrixMode(GL_MODELVIEW);
}

/* lwmgl_draw_cube() */
__declspec(dllexport) void lwmgl_draw_cube(int rx, int ry, int rz, int size) {
    float s = size / 100.0f;
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -5.0f);
    glRotatef((float)rx, 1.0f, 0.0f, 0.0f);
    glRotatef((float)ry, 0.0f, 1.0f, 0.0f);
    glRotatef((float)rz, 0.0f, 0.0f, 1.0f);
    glBegin(GL_QUADS);
    glColor3f(1.0f, 0.0f, 0.0f); glVertex3f(-s, -s,  s); glVertex3f( s, -s,  s); glVertex3f( s,  s,  s); glVertex3f(-s,  s,  s);
    glColor3f(0.0f, 1.0f, 0.0f); glVertex3f(-s, -s, -s); glVertex3f(-s,  s, -s); glVertex3f( s,  s, -s); glVertex3f( s, -s, -s);
    glColor3f(0.0f, 0.0f, 1.0f); glVertex3f(-s,  s, -s); glVertex3f(-s,  s,  s); glVertex3f( s,  s,  s); glVertex3f( s,  s, -s);
    glColor3f(1.0f, 1.0f, 0.0f); glVertex3f(-s, -s, -s); glVertex3f( s, -s, -s); glVertex3f( s, -s,  s); glVertex3f(-s, -s,  s);
    glColor3f(1.0f, 0.0f, 1.0f); glVertex3f( s, -s, -s); glVertex3f( s,  s, -s); glVertex3f( s,  s,  s); glVertex3f( s, -s,  s);
    glColor3f(0.0f, 1.0f, 1.0f); glVertex3f(-s, -s, -s); glVertex3f(-s, -s,  s); glVertex3f(-s,  s,  s); glVertex3f(-s,  s, -s);
    glEnd();
}

/* lwmgl_draw_triangle(rx, ry, rz, size) - equilateral triangle */
__declspec(dllexport) void lwmgl_draw_triangle(int rx, int ry, int rz, int size) {
    float s = size / 100.0f;
    glLoadIdentity();
    glTranslatef(-2.0f, 0.0f, -5.0f);
    glRotatef((float)rx, 1.0f, 0.0f, 0.0f);
    glRotatef((float)ry, 0.0f, 1.0f, 0.0f);
    glRotatef((float)rz, 0.0f, 0.0f, 1.0f);
    glBegin(GL_TRIANGLES);
    glColor3f(1.0f, 0.3f, 0.3f); glVertex3f(0.0f,  s, 0.0f);
    glColor3f(0.3f, 1.0f, 0.3f); glVertex3f(-s, -s, 0.0f);
    glColor3f(0.3f, 0.3f, 1.0f); glVertex3f( s, -s, 0.0f);
    glEnd();
}

/* lwmgl_draw_circle(x_off, y_off, z_off, radius) - filled circle */
__declspec(dllexport) void lwmgl_draw_circle(int x_off, int y_off, int z_off, int radius) {
    float r = radius / 100.0f;
    glLoadIdentity();
    float cx = x_off / 100.0f;
    float cy = y_off / 100.0f;
    glTranslatef(cx, cy, -5.0f);
    glRotatef((float)z_off, 0.0f, 0.0f, 1.0f);
    int segments = 64;
    glBegin(GL_TRIANGLE_FAN);
    glColor3f(1.0f, 0.85f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    for (int i = 0; i <= segments; i++) {
        float angle = 2.0f * 3.14159265f * (float)i / (float)segments;
        float dx = r * (float)cos((double)angle);
        float dy = r * (float)sin((double)angle);
        glColor3f(1.0f - (float)i / segments * 0.5f, 0.4f + (float)i / segments * 0.4f, 0.1f);
        glVertex3f(dx, dy, 0.0f);
    }
    glEnd();
}
