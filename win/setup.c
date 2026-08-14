/* setup.c �?Mira 鑷В鍘嬪畨瑁呯▼�?
 *
 * 宸ヤ綔鍘熺悊:
 *   1. 缂栬瘧姝ゆ枃浠朵�?setup_stub.exe
 *   2. �?zip 鍖呰拷鍔犲埌 stub 鍚庨�? copy /b setup_stub.exe + payload.zip setup.exe
 *   3. �?setup.exe 鏈熬杩藉姞 stub 澶у皬�?瀛楄�?little-endian�?
 *   4. 杩愯�?setup.exe 鏃讹紝瀹冭鍙栬嚜韬紝鎻愬彇 zip 閮ㄥ垎锛岃В鍘嬪埌鐢ㄦ埛鎸囧畾鐩�?
 *
 * 缂栬�? gcc -O2 -o setup_stub.exe setup.c -ladvapi32 -lshell32 -lole32
 */
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include <direct.h>

/* ===== 娉ㄥ唽琛ㄥ伐�?===== */
static void reg_set(HKEY root, const char *path, const char *name, const char *val) {
    HKEY hk;
    if (RegCreateKeyExA(root, path, 0, NULL, 0, KEY_WRITE, NULL, &hk, NULL) == ERROR_SUCCESS) {
        RegSetValueExA(hk, name, 0, REG_SZ, (const BYTE *)val, (DWORD)(strlen(val) + 1));
        RegCloseKey(hk);
    }
}

static void reg_set_expand(HKEY root, const char *path, const char *name, const char *val) {
    HKEY hk;
    if (RegCreateKeyExA(root, path, 0, NULL, 0, KEY_WRITE, NULL, &hk, NULL) == ERROR_SUCCESS) {
        RegSetValueExA(hk, name, 0, REG_EXPAND_SZ, (const BYTE *)val, (DWORD)(strlen(val) + 1));
        RegCloseKey(hk);
    }
}

static int path_contains(const char *path_env, const char *dir) {
    char lp[8192], ld[512];
    int i;
    for (i = 0; path_env[i] && i < 8191; i++)
        lp[i] = (path_env[i] >= 'A' && path_env[i] <= 'Z') ? path_env[i] + 32 : path_env[i];
    lp[i] = '\0';
    for (i = 0; dir[i] && i < 511; i++)
        ld[i] = (dir[i] >= 'A' && dir[i] <= 'Z') ? dir[i] + 32 : dir[i];
    ld[i] = '\0';
    return strstr(lp, ld) != NULL;
}

/* ===== 鎻愬彇宓屽叆�?zip ===== */
static int extract_payload(const char *self_path, const char *tmp_zip) {
    /* 璇诲�?self exe 鏈�?4 瀛楄�?= stub 澶у皬 */
    FILE *f = fopen(self_path, "rb");
    if (!f) return 0;

    fseek(f, -4, SEEK_END);
    long total_size = ftell(f) + 4;
    unsigned char marker[4];
    fread(marker, 1, 4, f);
    long stub_size = marker[0] | (marker[1] << 8) | (marker[2] << 16) | (marker[3] << 24);

    long zip_size = total_size - stub_size - 4; /* -4 for the marker itself */
    if (zip_size <= 0 || stub_size <= 0 || stub_size >= total_size) {
        fclose(f);
        return 0;
    }

    /* 鎻愬�?zip 鍒颁复鏃舵枃�?*/
    fseek(f, stub_size, SEEK_SET);
    FILE *out = fopen(tmp_zip, "wb");
    if (!out) { fclose(f); return 0; }

    char buf[8192];
    long remaining = zip_size;
    while (remaining > 0) {
        size_t chunk = remaining > 8192 ? 8192 : remaining;
        size_t r = fread(buf, 1, chunk, f);
        if (r == 0) break;
        fwrite(buf, 1, r, out);
        remaining -= (long)r;
    }
    fclose(out);
    fclose(f);
    return 1;
}

/* ===== �?PowerShell 瑙ｅ�?===== */
static int unzip(const char *zip_path, const char *dest) {
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "powershell -NoProfile -Command \"Expand-Archive -Path '%s' -DestinationPath '%s' -Force\"",
        zip_path, dest);
    return system(cmd) == 0;
}

/* ===== 鍒涘缓鐩綍锛堥€掑綊锛?===== */
static void mkdirs(const char *path) {
    char tmp[MAX_PATH];
    strncpy(tmp, path, MAX_PATH - 1);
    tmp[MAX_PATH - 1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '\\' || *p == '/') {
            *p = '\0';
            CreateDirectoryA(tmp, NULL);
            *p = '\\';
        }
    }
    CreateDirectoryA(tmp, NULL);
}

/* ===== 鐢熸垚鍗歌浇鑴氭�?===== */
static void write_uninstaller(const char *install_dir) {
    char path[MAX_PATH];
    snprintf(path, MAX_PATH, "%s\\uninstall.bat", install_dir);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "@echo off\n");
    fprintf(f, "chcp 65001 >nul 2>&1\n");
    fprintf(f, "echo.\n");
    fprintf(f, "echo   Uninstalling Mira...\n");
    fprintf(f, "echo.\n");
    fprintf(f, "\n");
    fprintf(f, "reg delete \"HKCU\\Software\\Classes\\.mira\" /f >nul 2>&1\n");
    fprintf(f, "reg delete \"HKCU\\Software\\Classes\\MiraSourceFile\" /f >nul 2>&1\n");
    fprintf(f, "reg delete \"HKCU\\Software\\Mira\" /f >nul 2>&1\n");
    fprintf(f, "reg delete \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Mira\" /f >nul 2>&1\n");
    fprintf(f, "\n");
    /* Remove from PATH */
    fprintf(f, "powershell -NoProfile -Command \"$p = [Environment]::GetEnvironmentVariable('Path','User'); ");
    fprintf(f, "$dirs = $p -split ';' | Where-Object { $_ -ne '%s' }; ", install_dir);
    fprintf(f, "[Environment]::SetEnvironmentVariable('Path', ($dirs -join ';'), 'User')\"\n");
    fprintf(f, "\n");
    fprintf(f, "echo   [OK] Registry cleaned\n");
    fprintf(f, "echo   [OK] Removed from PATH\n");
    fprintf(f, "echo.\n");
    fprintf(f, "echo   To finish, delete this folder: %s\n", install_dir);
    fprintf(f, "echo.\n");
    fprintf(f, "pause\n");
    fclose(f);
}

/* ===== 璁剧疆鏂囦欢鍏宠仈鍜?PATH ===== */
static void configure_system(const char *install_dir) {
    char exe_path[MAX_PATH], ico_path[MAX_PATH], cmd[MAX_PATH * 2];
    char uninstall_path[MAX_PATH];
    snprintf(exe_path, MAX_PATH, "%s\\mira.exe", install_dir);
    snprintf(ico_path, MAX_PATH, "%s\\mira.ico", install_dir);
    snprintf(uninstall_path, MAX_PATH, "%s\\uninstall.bat", install_dir);

    printf("\n  [2/4] Registering .mira file association...\n");

    reg_set(HKEY_CURRENT_USER, "Software\\Classes\\.mira", NULL, "MiraSourceFile");
    reg_set(HKEY_CURRENT_USER, "Software\\Classes\\MiraSourceFile", NULL, "Mira Source File");

    if (GetFileAttributesA(ico_path) != INVALID_FILE_ATTRIBUTES) {
        reg_set(HKEY_CURRENT_USER, "Software\\Classes\\MiraSourceFile\\DefaultIcon", NULL, ico_path);
        printf("        [OK] File icon\n");
    }

    reg_set(HKEY_CURRENT_USER, "Software\\Classes\\MiraSourceFile\\shell", NULL, "compile");
    reg_set(HKEY_CURRENT_USER, "Software\\Classes\\MiraSourceFile\\shell\\compile", NULL, "Compile with Mira");
    reg_set(HKEY_CURRENT_USER, "Software\\Classes\\MiraSourceFile\\shell\\compile", "Icon", exe_path);
    snprintf(cmd, sizeof(cmd), "\"%s\" \"%%1\"", exe_path);
    reg_set(HKEY_CURRENT_USER, "Software\\Classes\\MiraSourceFile\\shell\\compile\\command", NULL, cmd);
    printf("        [OK] Right-click compile\n");

    reg_set(HKEY_CURRENT_USER, "Software\\Classes\\MiraSourceFile\\shell\\open", NULL, "Compile & Run");
    snprintf(cmd, sizeof(cmd), "cmd /c \"\"%s\" \"%%1\" && echo. && pause\"", exe_path);
    reg_set(HKEY_CURRENT_USER, "Software\\Classes\\MiraSourceFile\\shell\\open\\command", NULL, cmd);
    printf("        [OK] Double-click compile\n");

    /* PATH */
    printf("\n  [3/4] Configuring PATH...\n");
    HKEY hk_env;
    char user_path[8192] = "";
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Environment", 0, KEY_READ | KEY_WRITE, &hk_env) == ERROR_SUCCESS) {
        DWORD type, sz = sizeof(user_path) - 1;
        RegQueryValueExA(hk_env, "Path", NULL, &type, (BYTE *)user_path, &sz);
        RegCloseKey(hk_env);
    }

    if (path_contains(user_path, install_dir)) {
        printf("        [-]  Already in PATH\n");
    } else {
        char new_path[8192];
        if (user_path[0])
            snprintf(new_path, sizeof(new_path), "%s;%s", user_path, install_dir);
        else
            snprintf(new_path, sizeof(new_path), "%s", install_dir);
        reg_set_expand(HKEY_CURRENT_USER, "Environment", "Path", new_path);
        SendMessageTimeoutA(HWND_BROADCIR, WM_SETTINGCHANGE, 0,
                            (LPARAM)"Environment", SMTO_ABORTIFHUNG, 5000, NULL);
        printf("        [OK] Added to PATH\n");
    }

    /* Add/Remove Programs 鍗歌浇淇℃伅 */
    printf("\n  [4/4] Registering uninstaller...\n");
    write_uninstaller(install_dir);

    const char *unreg = "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Mira";
    reg_set(HKEY_CURRENT_USER, unreg, "DisplayName", "Mira Programming Language");
    reg_set(HKEY_CURRENT_USER, unreg, "DisplayVersion", "5.2.6");
    reg_set(HKEY_CURRENT_USER, unreg, "Publisher", "Mira");
    reg_set(HKEY_CURRENT_USER, unreg, "InstallLocation", install_dir);
    reg_set(HKEY_CURRENT_USER, unreg, "DisplayIcon", ico_path);
    snprintf(cmd, sizeof(cmd), "\"%s\"", uninstall_path);
    reg_set(HKEY_CURRENT_USER, unreg, "UninstallString", cmd);
    reg_set(HKEY_CURRENT_USER, unreg, "NoModify", "1");
    reg_set(HKEY_CURRENT_USER, unreg, "NoRepair", "1");
    /* Estimated size in KB */
    {
        HKEY hk;
        DWORD est_kb = 300; /* ~300 KB */
        if (RegOpenKeyExA(HKEY_CURRENT_USER, unreg, 0, KEY_WRITE, &hk) == ERROR_SUCCESS) {
            RegSetValueExA(hk, "EstimatedSize", 0, REG_DWORD, (const BYTE *)&est_kb, sizeof(est_kb));
            RegCloseKey(hk);
        }
    }
    printf("        [OK] Added to Programs and Features\n");

    reg_set(HKEY_CURRENT_USER, "Software\\Mira", "InstallPath", install_dir);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
}

int main(void) {
    SetConsoleOutputCP(65001);

    printf("\n");
    printf("  +==========================================+\n");
    printf("  |     Mira v5.2.6  Installer               |\n");
    printf("  |  Stack Language, Native x86-64 Compiler   |\n");
    printf("  +==========================================+\n");
    printf("\n");

    /* 鑾峰彇鑷韩璺�?*/
    char self_path[MAX_PATH];
    GetModuleFileNameA(NULL, self_path, MAX_PATH);

    /* 榛樿瀹夎鐩綍 */
    char default_dir[MAX_PATH];
    char *userprofile = getenv("USERPROFILE");
    if (userprofile)
        snprintf(default_dir, MAX_PATH, "%s\\mira", userprofile);
    else
        snprintf(default_dir, MAX_PATH, "C:\\mira");

    /* 璇㈤棶瀹夎鐩綍 */
    char install_dir[MAX_PATH];
    printf("  Install directory [%s]: ", default_dir);
    fflush(stdout);
    if (fgets(install_dir, MAX_PATH, stdin)) {
        /* 鍘绘帀鎹㈣�?*/
        char *nl = strchr(install_dir, '\n');
        if (nl) *nl = '\0';
        nl = strchr(install_dir, '\r');
        if (nl) *nl = '\0';
    }
    if (install_dir[0] == '\0')
        strcpy(install_dir, default_dir);

    /* 鍘绘帀鏈熬鍙嶆枩�?*/
    int len = (int)strlen(install_dir);
    while (len > 0 && (install_dir[len-1] == '\\' || install_dir[len-1] == '/'))
        install_dir[--len] = '\0';

    printf("\n  Installing to: %s\n\n", install_dir);

    /* Step 1: 瑙ｅ�?*/
    printf("  [1/4] Extracting files...\n");

    char tmp_zip[MAX_PATH];
    char *tmp = getenv("TEMP");
    snprintf(tmp_zip, MAX_PATH, "%s\\mira_setup_payload.zip", tmp ? tmp : ".");

    if (!extract_payload(self_path, tmp_zip)) {
        printf("\n  [!] No embedded payload found.\n");
        printf("      This setup.exe may not have been built correctly.\n");
        printf("      (Did you run: copy /b setup_stub.exe + payload.zip + marker setup.exe ?)\n\n");
        printf("  Press any key to exit...\n");
        getchar();
        return 1;
    }

    mkdirs(install_dir);

    if (!unzip(tmp_zip, install_dir)) {
        printf("  [!] Failed to extract files.\n\n");
        printf("  Press any key to exit...\n");
        DeleteFileA(tmp_zip);
        getchar();
        return 1;
    }
    DeleteFileA(tmp_zip);
    printf("        [OK] Files extracted\n");

    /* Step 2-3: 娉ㄥ�?*/
    configure_system(install_dir);

    printf("\n  ==========================================\n");
    printf("  Installation complete!\n\n");
    printf("  Open a NEW terminal and try:\n");
    printf("    mira hello.mira     Compile Mira source\n");
    printf("    mira                Enter REPL mode\n\n");
    printf("  To uninstall: Control Panel > Programs or\n");
    printf("    run uninstall.bat in the install folder.\n\n");
    printf("  ==========================================\n\n");
    printf("  Press any key to exit...\n");
    getchar();
    return 0;
}
