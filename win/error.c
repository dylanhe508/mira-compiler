/* Mira error reporting �?统一的编译错误输�?
 * 带行号、源码上下文、箭头指示、Windows 控制台颜�?*/
#include "mira.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* Windows 控制台颜�?*/
#ifdef _WIN32
static HANDLE hStderr = INVALID_HANDLE_VALUE;
static WORD  original_attr = 7;

static void init_console(void) {
	if (hStderr == INVALID_HANDLE_VALUE) {
		hStderr = GetStdHandle(STD_ERROR_HANDLE);
		CONSOLE_SCREEN_BUFFER_INFO info;
		if (GetConsoleScreenBufferInfo(hStderr, &info))
			original_attr = info.wAttributes;
	}
}

static void set_color_red(void)   { init_console(); SetConsoleTextAttribute(hStderr, FOREGROUND_RED | FOREGROUND_INTENSITY); }
static void set_color_green(void) { init_console(); SetConsoleTextAttribute(hStderr, FOREGROUND_GREEN | FOREGROUND_INTENSITY); }
static void set_color_white(void) { init_console(); SetConsoleTextAttribute(hStderr, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY); }
static void set_color_cyan(void)  { init_console(); SetConsoleTextAttribute(hStderr, FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY); }
static void set_color_gray(void)  { init_console(); SetConsoleTextAttribute(hStderr, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); }
static void reset_color(void)     { init_console(); SetConsoleTextAttribute(hStderr, original_attr); }
#else
static void set_color_red(void)   { fprintf(stderr, "\033[1;31m"); }
static void set_color_green(void) { fprintf(stderr, "\033[1;32m"); }
static void set_color_white(void) { fprintf(stderr, "\033[1;37m"); }
static void set_color_cyan(void)  { fprintf(stderr, "\033[1;36m"); }
static void set_color_gray(void)  { fprintf(stderr, "\033[0;37m"); }
static void reset_color(void)     { fprintf(stderr, "\033[0m"); }
#endif

/* 从源码字符串中提取第 target_line 行（1-based�?
 * 返回行首指针�?out_len 为行长度（不含换行） */
static const char *find_line(const char *src, int target_line, int *out_len) {
	if (!src || target_line < 1) { *out_len = 0; return NULL; }
	const char *p = src;
	int cur = 1;
	while (*p && cur < target_line) {
		if (*p == '\n') cur++;
		p++;
	}
	if (cur != target_line) { *out_len = 0; return NULL; }
	const char *start = p;
	while (*p && *p != '\n' && *p != '\r') p++;
	*out_len = (int)(p - start);
	return start;
}

/* 主要错误报告函数
 * src: 源码全文（用于提取行内容�?
 * filename: 文件�?
 * line: 出错行号�?-based），0 表示无行�?
 * col: 出错列号�?-based），0 表示无列�?
 * exit_code: 程序的退出码
 * fmt, ...: 错误信息（printf 格式�?*/
void mira_error(const char *src, const char *filename, int line, int col, int exit_code, const char *fmt, ...) {
	const char *fname = filename ? filename : "<input>";

	/* 第一行：文件路径 */
	fprintf(stderr, "\n");
	set_color_cyan();
	fprintf(stderr, "%s", fname);
	reset_color();
	fprintf(stderr, "\n");

	/* 第二行：编译器公�?*/
	set_color_white();
	fprintf(stderr, "Mira discovered an error during the compilation/linking process:");
	reset_color();
	fprintf(stderr, "\n");

	/* 第三行：行号和列�?*/
	if (line > 0) {
		set_color_gray();
		fprintf(stderr, "Line %d", line);
		if (col > 0) fprintf(stderr, ", Column %d", col);
		fprintf(stderr, ":");
		reset_color();
		fprintf(stderr, "\n");
	}

	/* 源码上下�?*/
	if (src && line > 0) {
		int len = 0;
		const char *line_str = find_line(src, line, &len);
		if (line_str && len > 0) {
			/* 行号 + 代码 */
			set_color_gray();
			fprintf(stderr, " %4d | ", line);
			reset_color();
			fwrite(line_str, 1, (size_t)len, stderr);
			fprintf(stderr, "\n");

			/* 波浪�?箭头指示 */
			set_color_green();
			fprintf(stderr, "      | ");
			if (col > 0 && col <= len) {
				/* 指向精确�?*/
				for (int i = 0; i < col - 1; i++)
					fputc(line_str[i] == '\t' ? '\t' : ' ', stderr);
				fputc('^', stderr);
			} else {
				/* 回退：指向整行非空内�?*/
				int indent = 0;
				while (indent < len && (line_str[indent] == ' ' || line_str[indent] == '\t'))
					indent++;
				for (int i = 0; i < indent; i++)
					fputc(line_str[i] == '\t' ? '\t' : ' ', stderr);
				int content_len = len - indent;
				if (content_len <= 0) content_len = 1;
				for (int i = 0; i < content_len; i++)
					fputc('^', stderr);
			}
			reset_color();
			fprintf(stderr, "\n");
		}
	}

	/* 错误描述 */
	set_color_red();
	fprintf(stderr, "error");
	reset_color();
	fprintf(stderr, ": ");
	set_color_white();
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	reset_color();
	fprintf(stderr, "\n");

	/* 退出码 */
	set_color_gray();
	fprintf(stderr, "Exit code: (%d)", exit_code);
	reset_color();
	fprintf(stderr, "\n\n");

	exit(exit_code);
}

/* 简化版：不带源码上下文的通用错误（用�?main.c 等） */
void mira_error_simple(int exit_code, const char *fmt, ...) {
	set_color_red();
	fprintf(stderr, "error");
	reset_color();
	fprintf(stderr, ": ");

	set_color_white();
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	reset_color();
	fprintf(stderr, "\n\n");
	exit(exit_code);
}
