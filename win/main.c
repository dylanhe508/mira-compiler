/* Mira compiler driver 锟?import 鎲嶈悋馉綄锟借雹鑰拷璩拷锟斤拷鎲姮锟芥拸鍒革拷锟金─冿拷锟斤拷锟介潣鐑撅拷鎽ㄥ喗锟斤拷鈯ラ偊铦滐拷涓拷鍐介Μ鎲拷锟斤拷黏た锟斤拷锟芥挔鍓滀腑锟斤拷锟斤拷鏍斤拷锟藉í锟斤拷绁囩巩锟借鈪★拷绠濓拷锟戒鎻拷鎬掞拷锟芥锟藉珮锟斤拷鑸€绲筹拷椁咃拷锟斤拷锟芥啌鏍硷拷锟芥€狅拷鍢ワ拷锟藉锟芥啞锟斤拷锟藉柈锟斤拷钀勶拷锟金Б插噧鐠囩补锟斤拷绁嗗儹锟介牁锟斤拷鍡咃拷鐦ｈ姼锟芥喛稹潡鍋滐拷鍒革拷锟戒澕锟斤拷黏靖锟斤拷锟斤拷锟藉叐锟芥啞婀涳拷闇呴槨锟斤拷鐮旓拷锟芥箶锟斤拷锛革拷锟芥父锟借潖椁咃拷锟藉棯锟斤拷鏂囥锟斤拷铻傦拷鈭狅拷锟借锟斤拷皙櫅锟借潪稹伝楦橈拷妗€锟斤拷穰锟借潨鍋︺▋鐠嗭拷鎵囩拪鏂囷拷锟芥ⅱ锟斤拷椁咃拷锟姐殮锟斤拷銞囷拷鐬堟儵锟斤拷鍡伯铦戝锟界瀳鈭拷鎾忓埜锟斤拷锛癸拷铦伙拷锟借潝惘勭巩锟界稖鐙嶏拷槌达拷锟借硦锚锟斤拷锟芥啞鑴牸昏潩澧э拷锟戒箳锟介浛瑭癸拷锟借锟藉枃锟斤拷璩ｉ磦铦ュ暎锟芥啌妗€寤讹拷锟斤拷鐦涳拷锟斤拷錉凤拷锟藉吀锟斤拷婢撅拷锟斤拷楦橈拷鍠熷亯锟金Б诧拷锟解娍鎽氬殮锟斤拷锟借┗锟斤拷稹伝锟界殯锟解叀鎲★拷锟斤拷鎯╄縿鐨涳拷锟借潖椁呴枓鎾忔締寤讹拷稹彮锟斤拷锟斤拷锟斤拷锟界瀳瑙佹拻锟芥黏棃鐠婅尝杩勫殮銞囪嬁铦忓埢锟斤拷鐓久岋拷鑴╂迹锟借雹锟斤拷鎷氾拷锟芥▉缃革拷锟金婅澅锟斤拷黏铦庤垚穑埐锟藉爳鎲わ拷锟斤拷铦滐拷楦橈拷钀庯拷锟芥妴锟斤拷穑埊锟斤拷锟芥挓閸﹀繓锟界鐟ｏ拷鍓旂肪鎲挎⒍锟斤拷鍠查础?*/
#include "mira.h"
#include "codegen/codegen.h"
#include "codegen/abi.h"
#include "linker/linker.h"
#include "codegen/target.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

/* === Linux/ELF 符号扫描用的本地定义 ===
 * 不依赖 elfdefs.h(那是链接器内部头),这里自包含定义,
 * 仅在 main.c 的 ELF 符号扫描分支使用。名字带下划线后缀避免冲突。 */
#ifndef _WIN32
typedef struct { uint32_t sh_name, sh_type; uint64_t sh_flags, sh_addr, sh_offset, sh_size; uint32_t sh_link, sh_info; uint64_t sh_addralign, sh_entsize; } Elf64_Shdr_;
typedef struct { uint32_t st_name; uint8_t st_info, st_other; uint16_t st_shndx; uint64_t st_value, st_size; } Elf64_Sym_;
typedef struct { uint64_t r_offset, r_info; int64_t r_addend; } Elf64_Rela_;
static uint16_t elf_get_u16_(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t elf_get_u32_(const uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint64_t elf_get_u64_(const uint8_t *p) {
	return (uint64_t)elf_get_u32_(p) | ((uint64_t)elf_get_u32_(p + 4) << 32);
}
#endif

/* 目标文件后缀:Windows=.obj,Linux=.o。供 sym_map/mod_names 使用。 */
#ifdef _WIN32
#define RT_OBJ_EXT ".obj"
#else
#define RT_OBJ_EXT ".o"
#endif

static double mira_profile_ms(clock_t begin, clock_t end) {
	return (double)(end - begin) * 1000.0 / (double)CLOCKS_PER_SEC;
}

/* 浼樺寲绛夌骇: 0=鏃犱紭锟? 1=鍩虹, 2=鎺ㄨ崘(榛樿), 3=婵€锟?*/
int mira_opt_level = 2;
/* Modern x86-64 target by default; -mno-avx2 keeps a portable scalar path. */
int mira_target_avx2 = 1;
/* 目标 ABI:默认为宿主 ABI(Windows 上为 Win64,Linux 上为 SysV)。
 * 由 --target 选项覆盖。codegen/runtime 通过 abi.h 查询此值。 */
#ifdef _WIN32
MiraAbi mira_target_abi = MIRA_ABI_WIN64;
#else
MiraAbi mira_target_abi = MIRA_ABI_SYSV;
#endif

/* 锟斤拷锟斤拷鐟曡縿锟界锟介姶婕わ拷锟介锚虅鐠夛拷锟斤拷椁咃拷锟藉癁锟斤拷鍡碉拷锟借悇锟芥啰馉购锟芥拸鍦掞拷锟界锟斤拷锟金ぇ愭啌鍫掑卜鐠呮經锟斤拷闊拷锟斤拷榇＄瀳锟藉瀷锟芥妴锟斤拷锟斤拷鍤氬導鏅存啋鏀敗锟界畤锟斤拷鍡碉拷锟金滃欢锟芥江鎻氾拷鋾橈拷鎾燄冮础锟借蓟瀹屾拤皈籍锟芥啋璩婏拷锟芥泬锟斤拷锟金牕э拷锟斤拷锟介鎻ｇ槡鎯╂串锟界吘洳潝韪电挡铦忎鍑冿拷缃革拷锟界鎵囷拷韪碉拷锟界畤锟借彅锟斤拷绁嗭拷锟斤拷锟?import 锟斤拷锟斤拷璩婏拷鎾滃啎瀵ユ憵锟斤拷闃￠础锟界﹥锟斤拷鍧旓拷锟界锟界槪纭嬭繂鍎尝锟界槡婢嗭拷锟戒豢黏剟锟芥嫏锟斤拷鎶嗭拷锟斤拷锟芥暥閸︼拷锟金肩珯锟借悇寤讹拷鑴╋拷锟金ぉ猴拷鎾樿硦锟斤拷纭猴拷锟藉锟介埈楫嬸ò圭攬黏槳妞懓锟芥埈绁夊鐠嗗垹锟界槪鍓濓箿鐢堭拷锟芥隘锟斤拷姊拷锟藉椀锟斤拷婢楋拷鎾熻異锟斤拷锟借澐锟介笜锟斤拷缍夛拷锟斤拷铦滃仸绺濈攬璁愶拷锟界當铻傦拷婀э拷鍤氾拷姣猴拷锟界鎵戠瀳洵炬尓锟金夛拷锟斤拷锟斤拷鏇囩铦ゆ江鑰ㄧ拡锟斤拷锟金Б插噧鎾燄楃锟界鎽氾拷锟斤拷铦伙拷娲庯拷椐佺絹锟藉锟斤拷闉夛拷锟借。锟斤拷娼拷锟芥锟斤拷锟芥喛閸︷猴拷鐓鹃笜锟借雹锟斤拷鐟燂拷铦濆煗锟斤拷锟斤拷鎲嶈洈锟斤拷惘勶拷锟金い匡拷鎲掓€ラ枓鎾忥絹锟界槰鏈涜湏锟借悇锟界拤馥焽鍎掞拷锟介皧锟斤拷锟芥啌婀ч笜锟斤拷锟芥喛锛革拷鎾忕礁锟斤拷稹仠锟界瀳鈭狅拷锟界鎵橀浛瑁曞仸锟斤拷稹伓锟斤拷绠忎锟斤拷锟界殯锟斤拷闋╋拷锟借潳杓搁瘡锟姐暡皎锟斤拷澧欙拷闁栵拷锟芥媿黏妱鎲嶈悋锟斤拷鎶掗仚鐠堬拷鎾忕补锟斤拷鐫冿拷鐟熶腑锟芥花锟斤拷銞庯拷鐠囬锟界槥鐮傦拷锟斤拷锟斤拷黏徃馥亽锟借几皙儹锟金Ъ拷瀵ワ拷锟戒锟斤拷鍫掞拷铦氥樉黏剟锟芥嫏锟斤拷宄曢枓鎾樻恭锟斤拷鎵捐硳锟斤拷闃拷锟斤拷锟斤拷锟借。鍐拷琛€锟斤拷椁咃拷锟斤拷鐟曪拷锟?*/
#define IMPORT_MAX 64
static const char *imported[IMPORT_MAX];
static int imported_count;

static bool already_imported(const char *resolved) {
	for (int i = 0; i < imported_count; i++)
		if (imported[i] && strcmp(imported[i], resolved) == 0)
			return true;
	return false;
}

static void push_imported(const char *path) {
	if (imported_count < IMPORT_MAX) {
		imported[imported_count++] = strdup(path);
	}
}

/* libs-mira 锟斤拷锟斤拷璩婏拷鎾滃啎瀵ユ憵锟斤拷闃￠础锟界﹥锟斤拷鍧旓拷锟界锟界槪纭嬭繂鍎尝锟界槡婢嗭拷锟戒豢黏剟锟芥嫏锟斤拷鎶嗭拷锟斤拷锟芥暥閸︼拷锟金肩珯锟借悇寤讹拷鑴╋拷锟金ぉ猴拷鎾樿硦锟斤拷纭猴拷?*/
static char libs_dir[512] = {0};
static char libs_dll_dir[512] = {0};
static char exe_dir[512] = {0};  /* mira.exe 锟斤拷锟藉嚱黏實鏁娥拷闆块潩铔硅潖鍢ョ瑔闋濊嚞锟斤拷?*/

#ifdef _WIN32
#include <windows.h>
#endif

static void init_libs_dir(const char *argv0) {
	char full_path[512];
	const char *src = argv0;

#ifdef _WIN32
	DWORD n = GetModuleFileNameA(NULL, full_path, sizeof(full_path));
	if (n > 0 && n < sizeof(full_path)) src = full_path;
#else
	/* Linux:通过 /proc/self/exe 获取可执行文件绝对路径 */
	ssize_t n = readlink("/proc/self/exe", full_path, sizeof(full_path) - 1);
	if (n > 0) { full_path[n] = '\0'; src = full_path; }
#endif

	const char *lIR = strrchr(src, '\\');
	if (!lIR) lIR = strrchr(src, '/');
	if (lIR) {
		size_t dir_len = (size_t)(lIR - src + 1);
		if (dir_len < sizeof(exe_dir)) {
			memcpy(exe_dir, src, dir_len);
			exe_dir[dir_len] = '\0';
		}
#ifdef _WIN32
		if (dir_len + 10 < sizeof(libs_dir)) {
			memcpy(libs_dir, src, dir_len);
			memcpy(libs_dir + dir_len, "libs-mira\\", 10);
			libs_dir[dir_len + 10] = '\0';
		}
		if (dir_len + 9 < sizeof(libs_dll_dir)) {
			memcpy(libs_dll_dir, src, dir_len);
			memcpy(libs_dll_dir + dir_len, "libs-dll\\", 9);
			libs_dll_dir[dir_len + 9] = '\0';
		}
	} else {
		exe_dir[0] = '\0';
		memcpy(libs_dir, "libs-mira\\", 11);
		memcpy(libs_dll_dir, "libs-dll\\", 10);
	}
#else
		if (dir_len + 10 < sizeof(libs_dir)) {
			memcpy(libs_dir, src, dir_len);
			memcpy(libs_dir + dir_len, "libs-mira/", 10);
			libs_dir[dir_len + 10] = '\0';
		}
		if (dir_len + 9 < sizeof(libs_dll_dir)) {
			memcpy(libs_dll_dir, src, dir_len);
			memcpy(libs_dll_dir + dir_len, "libs-dll/", 9);
			libs_dll_dir[dir_len + 9] = '\0';
		}
	} else {
		exe_dir[0] = '\0';
		memcpy(libs_dir, "libs-mira/", 11);
		memcpy(libs_dll_dir, "libs-dll/", 10);
	}
#endif
}

/* 锟斤拷锟斤拷璩婏拷鎾滃啎瀵ユ憵锟斤拷闃￠笜锟斤絾锟借潖闉夛拷锟界礁穰劇锟芥緱鐒╋拷瑭ㄧ偖铦滃槬绁氾拷瑷櫤鎲灳锟斤拷馉墰锟介浛璩滆啯锟金滄锟斤拷锟斤拷稹拪锟斤拷鍠虫紩鎾樺柌锟斤拷鏉★拷?import 锟斤拷锟斤拷璩婏拷鎾滃啎瀵ユ憵锟斤拷闃￠笜锟斤絾锟借潵鏇夛拷锟斤拷绶碉拷璩ｉ够锟介瓊锟斤拷鎷氾拷锟界畻锟斤拷铔旓拷锟解娍锟借潝閬达拷锟藉焾锟芥啋鑿燂拷锟金讹拷锟借雹鍏烇拷鍦掞拷锟介鈪¤澔锟介偅铦氥樉楦樻暪馥獪纭瀳鈭犵挍鎾忕礁锟界槥姊憋拷锟斤拷锟斤拷鍡拷锟芥浮锟斤拷锟角滐拷閸﹁縿鍤氾拷閮庯拷銟撅拷锟藉埜锟界拝绠楋拷鐠嗭拷锟斤拷穑偧绔欒澀锟斤拷瑭伙拷锟借垁楹憋拷椁岋拷铦庰，氶仚锟藉爳锟斤拷绠革拷锟金　為锟芥敼棣达拷黏靖锟界拠绠囷拷锟藉枱锟介榇★拷锛歌啔锟芥畨锟界拠椐侊拷鐦炵爞锟斤拷锟斤拷闃★拷锟芥€幩欙拷璞氾拷瑙佽繂锟斤拷锟斤拷钀勶拷锟介灳锟斤拷娼拷锟藉潝稹牶铦凤拷鐬堭Ъ拷锟界吘绶ゆ拸锟斤拷鎲掕埅绉熸挊韪碉拷锟借悇濡ワ拷洵惧亯鎾忓槬鎹傦拷绮瑃 "path"锟斤拷锟斤拷璩婏拷锟斤拷锟斤拷黏徃馥亽锟借几皙儹锟斤拷皎疁锟金Б拆：胯潕鍓栵拷鐠呴钂撅拷榄傦拷锟芥嫐锟斤拷璞拷鐠嗭拷锟斤拷婕わ拷铦旈伌锟芥喛绌冦Д锟介畫绺濆劖璩拷锟藉锟芥挘稷爟锟芥啀锟解叀锟借锟借澓鑴ら笜鏁桂爥涳拷鎲嶏拷闈橈拷鐠囩畤锟介牆鍑芥墖锟借雹楝诧拷锟借芳锟界畯锟斤拷馉墰锟斤拷鑿燂拷锟芥枃锟界瀴瀛碉拷鎾忕补锟芥啳璁狅拷鐠婃枟锟芥拸鑿燄ò圭攬鍍愶拷锟金Ф忥拷鐦ｆ恭黏剟闆垮锟芥懓皎盎锟斤拷錉凤拷锟戒紞锟借澔锟斤拷锟借硦锟芥唽鎿э拷锟戒帤锟借潳椐佷伯锟芥姃鐩旓拷黏槳穑锟芥激锟介浛璞拷锟藉暰锟界槪缃革拷?import <path>锟斤拷锟斤拷璩婏拷锟斤拷锟斤拷黏徃馥亽锟借几皙儹锟斤拷皎疁锟金Б拆：胯潕鍓栵拷鐠呴钂撅拷榄傦拷锟芥嫐锟斤拷绠楋拷鐦ㄨ徊锟借潖皎锟斤拷锟介懍锟金┛炪瘎楫燂拷鎵瑰煄锟斤拷锟斤拷锟芥喛鍦濓拷锟芥ⅱ锟?mira锟?
   锟斤拷锟斤拷璩婏拷鎲岎婏拷鐨滃盃锟芥拤澧э拷锟借摜锟斤拷浼嶏拷锟斤拷皙儹锟藉爳锟斤拷棰拷锟芥⒍閬欑瀳锟斤拷锟藉硶锟斤拷绮癸拷锟藉導愫拷洵撅拷鎾橀锟斤拷绠囨尳锟借硟锟斤拷稹潡鍣拷寰夛拷锟金硷拷锟借ǐ杩勭殯锟斤拷鎽伴枡锟斤拷锟芥喛鍦濓拷鎲胯姼锟芥啋鍦掋▋鐠嗭拷锟借澔锟借澑鎾犱牱锟斤拷锟斤拷锟斤拷鈴氾拷榻胯€ㄧ拝绠忥拷锟借悇锟界拠绮癸拷鐦滐拷鐬堚埖锟借潝銟撅拷锟金ぉ傦拷鐦氾拷楹挊韪瑰欢锟斤拷涓勬喛鍓栵拷闈界緭鍐拷鎶捤夌拪鏂楋拷鐠夝い匡拷锟斤拷锟界礁锟斤拷宄曪拷锟藉療锟借潕璜桂⒍狅拷椁呮苟鎾夎矈锟?NULL锟?out_is_lib = 1 锟斤拷锟斤拷璩婏拷鎾滃啎瀵ユ憵锟斤拷闃￠笜锟斤絾锟借潵鏇夛拷锟斤拷绶碉拷璩ｉ够锟介瓊锟斤拷鎷氾拷锟借鏅拷悒嗭拷鐟斤拷锟介浛鑴ｏ拷鎲嶏己锟斤拷闁э拷闁拷锟斤拷鍢ユ豢鎲撴寤跺殫锟界秹锟?<> 锟斤拷锟斤拷璩婏拷鎾滃啎瀵ユ憵锟斤拷闃￠础锟界﹥锟斤拷鍧旓拷锟解姤鍣ゆ埈鑺ｆ倕锟金Ζ碉拷鎾ｅ瀺鐟ｆ啛璞㈤皧锟藉爦锟借澂椐侀础鎴姲瀹屾啰缇擄拷鎾栨締锟斤拷鍢ワ拷锟斤拷锟斤拷穑埊锟斤拷鐟烇拷?*/
/* 鎾犺偨锟斤拷?libs_dir 锟?libs_dll_dir 閵濆墰鍦撅拷穑偧锟介牆鑷拷锟?
 * 锟金硷拷锟藉棄锟斤拷澧ч暗锟斤拷锟斤拷锟斤拷鎾栧妤濇暫锟斤拷鎾熷棄鎸借澋?*out_is_binary=1 鎲掞拷锟斤拷?DLL
 * 鎲鎻栵拷鍡夛拷锟?NULL锟?
 */
static char *resolve_lib_path(const char *imp_path, int *out_is_binary) {
	*out_is_binary = 0;
	size_t plen = strlen(imp_path);
	char *try_path = malloc(512 + plen + 10);
	if (!try_path) return NULL;
	FILE *f = NULL;

	/* 1. libs-mira/xx.mira */
	sprintf(try_path, "%s%s.mira", libs_dir, imp_path);
	f = fopen(try_path, "rb");
	if (f) { fclose(f); return try_path; }

	/* 2. libs-mira/xx */
	sprintf(try_path, "%s%s", libs_dir, imp_path);
	f = fopen(try_path, "rb");
	if (f) { fclose(f); return try_path; }

	/* 3. libs-dll/xx.dll */
	sprintf(try_path, "%s%s.dll", libs_dll_dir, imp_path);
	f = fopen(try_path, "rb");
	if (f) { fclose(f); *out_is_binary = 1; return try_path; }

	/* 4. libs-dll/xx */
	sprintf(try_path, "%s%s", libs_dll_dir, imp_path);
	f = fopen(try_path, "rb");
	if (f) { fclose(f); *out_is_binary = 1; return try_path; }

	free(try_path);
	return NULL;
}

static char *parse_import_line(const char *line, int *out_is_lib) {
	*out_is_lib = 0;
	while (*line == ' ' || *line == '\t') line++;
	if (strncmp(line, "import", 6) != 0) return NULL;
	line += 6;
	if (*line != ' ' && *line != '\t') return NULL;
	while (*line == ' ' || *line == '\t') line++;
	if (*line == '"') {
		line++;
		const char *end = strchr(line, '"');
		if (!end) return NULL;
		size_t len = (size_t)(end - line);
		char *out = malloc(len + 1);
		if (!out) return NULL;
		memcpy(out, line, len);
		out[len] = '\0';
		*out_is_lib = 0;
		return out;
	}
	if (*line == '<') {
		line++;
		const char *end = strchr(line, '>');
		if (!end) return NULL;
		size_t len = (size_t)(end - line);
		char *out = malloc(len + 1);
		if (!out) return NULL;
		memcpy(out, line, len);
		out[len] = '\0';
		*out_is_lib = 1;
		return out;
	}
	/* import path 锟斤拷锟斤拷璩婏拷锟斤拷锟斤拷黏徃馥亽锟借几皙儹锟斤拷皎疁锟金Б拆：胯潕鍓栵拷鐠呴钂撅拷榄傦拷锟芥嫐锟斤拷璞拷鐠嗭拷锟斤拷婕わ拷铦旈伌锟芥喛绌冦Д锟介畫绺濆劖璩拷锟金牼嶏拷锟斤拷锟藉梿锟斤拷鐓撅拷锟金筹拷锟借┗锟斤拷鏋忚繂锟藉焾鈪㈢槡锟斤拷锟戒偪锟斤拷娼涳拷铦忔锟斤拷锟借稒锟芥锟斤拷锟斤拷锟斤拷锟斤拷锟藉仸绺濆劖璩滐拷鐬堭３囷拷鐠夌兙锟斤拷锟斤拷锟藉敵锟借潖椁冿拷鎾橀锟界槥锟斤拷锟藉槬婊块埈楫嬮瘡锟金囷拷铦忔湜锟借潩閸︽棳锟芥締锟斤拷鑸橈拷鐦ｃ殮锟芥啛瑭伙拷锟藉繓闆垮鐜拷璞拷鎲崇爫瀹岋拷鍦濓拷锟界潈馉凯锟芥嫐榇℃埈鐮嶏拷鎽拌ǐ锟芥拸娼拷鐠婇锟斤拷瑷拷锟斤拷榀忥拷馥焽锟界槡穰Φ锟斤拷鍏革拷锟藉爳锟界拪鎵庯拷锟斤拷璺硷拷黏槳锟斤拷馉邯锟借潱婊氾拷锟斤拷琚愶拷锟介槷锟界惪锟斤拷妗€鐜澔锟芥穿锟藉棯鈥橈拷椁呪叀锟斤拷锟斤拷锛镐腑锟斤拷锟芥儵锟斤拷閬达拷锟界畤锟斤拷绁嗭拷锟借悇瀛濓拷鍢ユ紩锟斤拷鎲筐告锟介槨穑嫚锟藉墱锟芥啞绌冿拷鎾熴棝榇★拷姊讹拷鎲胯姼锟芥啋鍦掋▋鐠嗭拷鐬忕吘锟借潝閬达拷锟藉皎獨鎾橀锟斤拷锟芥湒鐬堚埖锟界懝锟界党锟借垁鈪★拷鎬庯拷锟借垁鍎掞拷穰Φ锟斤拷锟斤拷鐦氭黏剟闆胯硟锟借潖鏈涴曪拷锟斤拷鐢堟毠鍣愶拷锟介槷锟斤拷锟界瀳锟界锟斤拷锟界懡锟介疅鏁硅ǐ濡ユ懓鏋忥拷鐠囩补锟界槰姊侊拷锟借┄婕遍埈绁夎但闇傜瑳绮癸拷锟借姡楫嶈澔锟借瀭锟斤拷锟斤拷娓革拷锟金オ滐拷锟介枛锟藉殮锟斤拷锟芥妴锟借潨锟斤拷?*/
	const char *p = line;
	while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
	size_t len = (size_t)(p - line);
	char *out = malloc(len + 1);
	if (!out) return NULL;
	memcpy(out, line, len);
	out[len] = '\0';
	return out;
}

/* 锟斤拷锟斤拷璩婏拷鎾滃啎瀵ユ憵锟斤拷闃￠笜锟斤絾锟借潖闉夛拷锟界礁穰劇锟芥緱鐒╋拷瑭ㄧ偖铦滃槬绁氾拷瑷櫤鎲灳锟斤拷馉墰锟介浛璩滆啯锟金滄锟斤拷锟斤拷稹拪锟斤拷鍠虫紩鎾樺柌锟斤拷鏉★拷?!import "file" as name 锟?!import <file> as name锟斤拷锟斤拷璩婏拷锟斤拷锟界拪瑙佹化锟借常鑰ㄧ拡锟藉埀锟藉瀺锟斤拷瑭ㄦ伃锟斤级婊风瀳鎯╁櫢锟借副锟斤拷锛凤拷锟芥箾锟斤拷稹伓锟斤拷锟斤拷锟芥⒍锟斤拷鍏稿欢锟界潈鎯ｇ瀳缃憋拷锟借洈锟斤拷姊讹拷锟界锟斤拷椁咃拷铦滃槬绁氭啀稹亸锟斤拷馥暒锟借潣鍦掞拷锟金滐拷锟戒澕锟斤拷瑾拷?1 锟斤拷锟斤拷璩婏拷锟斤拷锟斤拷黏徃馥亽锟斤拷榇★拷锟斤拷铦忓垹洫达拷缃革拷锟芥挘鍋﹂础鎴吀锟藉棯锟斤拷姣猴拷锟介煯锟斤拷馉墰锟介浛鑴ｏ拷鎲嶏己锟斤拷銡氾拷鐦几锟斤拷鍓骨滅懝锟金勬埈鎬犺埈锟藉硶锟斤拷鑸橈拷锟金撅拷铦伙拷铦惰潕鑸ǐワ拷鋫挎椏锟斤拷锟斤拷稹椐氾拷鍐斤拷锟戒紞锟界拠? 鎲嶈悋馉綄锟借雹鑰拷璩㈡啢锟芥綌铻傝潳椐侀笜锟界閴勶拷鍏革拷锟藉仸锟藉殮锟斤拷铦伙拷锟斤拷鍠诧拷鐦ㄨ锟斤拷黏﹤锟芥啰锝侊拷鐦ｉ缃侊拷瀛叼í氭挓锟藉繓锟芥锟斤拷璞拷鍎豢锟斤拷璩ｂ叀铦伙拷閭ｈ潥銟攫獌拷鐫冿拷鐬堚垹鐠涙拸缃革拷鐦炵爫锟芥啀鑲呭暎锟介灳锟斤拷鍌㈠锟斤拷锛桂勭瀳?*/
static int parse_import_as_line(const char *line, char **out_path, char **out_alias, int *out_is_lib) {
	*out_is_lib = 0;
	while (*line == ' ' || *line == '\t') line++;
	if (*line != '!') return 0;
	line++;
	if (strncmp(line, "import", 6) != 0) return 0;
	line += 6;
	if (*line != ' ' && *line != '\t') return 0;
	while (*line == ' ' || *line == '\t') line++;
	/* 锟斤拷锟斤拷璩婏拷锟斤拷锟界拪瑙佹紗锟斤拷杩勫殮銞囪潬锟藉暎锟芥挊鍦掋獥鐦炲墱锟斤拷瑭伙拷铦滐拷锟斤拷鑷挱鎲块煯銣鸿潕鍡ワ拷闆胯劊锟芥啀锛猴拷锟姐殮锟界槰锛革拷锟斤拷楹拷锟戒腑鍤氳異锟斤拷锟界笐锟斤拷锟斤拷鐟熱粊锟界礁榛侊拷锟斤拷锟借雹锟斤拷稷爟锟藉殭鎯╂嵍鎲筐オ滐拷锟斤拷锟斤拷锟借潿锟斤拷锟戒场黏叀锟金ぉ傦拷锟姐瘎锟斤拷鍏告铦伙拷锟斤拷鐟曪拷鐦ｈ姼鎻涳拷黏﹤锟斤拷椁咃拷锟斤拷妤婅櫞锟藉療锟斤拷婀э拷鎴牸锟斤拷锛癸拷锟?锟?锟斤拷锟斤拷璩婏拷锟斤拷锟芥懓锟斤拷鏁堕崷鏅喛鍦濓拷椁堝棯锟斤拷皈籍鎺冩啌婀ю勬拝鏇勶拷锟芥妴锟借潿锟斤拷锟借嚟鐖剧槡婢嗭拷锟借榇★拷绌冿拷闅★拷锟界暰韪癸拷铦忔牸锟介澖馉按锟芥挆穰牅瀛氭啀鑿婏拷鏁讹拷锟斤拷鍟撅拷?"" 锟?<> */
	char open_ch = *line;
	char close_ch;
	if (open_ch == '"') close_ch = '"';
	else if (open_ch == '<') { close_ch = '>'; *out_is_lib = 1; }
	else return 0;
	line++;
	const char *end = strchr(line, close_ch);
	if (!end) return 0;
	size_t path_len = (size_t)(end - line);
	const char *path_start = line;
	line = end + 1;
	/* 锟斤拷锟斤拷璩婏拷鎾滃啎瀵ユ憵锟斤拷闃￠础锟界﹥锟斤拷鍧旓拷锟芥锟斤拷洵撅拷铦伙拷锟斤拷鍘帮拷锟借嚞锟斤拷鎬ワ拷锟芥激锟借潝閬达拷鐦氬湑锟芥挊椁冿拷锟界摳鍒搁疅鏁桂牥达拷锟借劜穑锟姐洊锟斤拷绁夛拷锟介锟借潨鍢ョ闇傝亞锟斤拷璁愷牻岋拷锟斤拷锟介槷铦滈灳缃佺殯锟芥啢锟芥揪锟斤拷鐑愶拷锟藉穑嚜鎾夊ⅶ锟斤拷鐮嶏拷锟姐瘎洳拷娓★拷锟斤拷锟斤拷璩婄笣锟借悗锟界瀳澧э拷鍎嫏閲嗭拷锟斤拷闋╋拷楝查埈瀵э拷鎲瀴鐪忕槢锟斤拷锟戒牱锟斤拷鍏革拷锟斤拷馉摷鎾忚劋榇★拷鎯╋拷铦ユ泧锟借潨鍡拷鐦ㄥ嫍锟斤拷璩拷锟姐殮锟斤拷姣猴拷锟藉埗锟芥啰鍖у欢锟斤拷锟斤拷鍫掞拷铦涙湜锟斤拷鍡嗭拷鐬堟儵锟斤拷稹拪锟藉殭洵锯叀锟斤拷韪庣懡椁岋拷瀵烉氾拷缃稿槨锟界畯锟斤拷纾帮拷閳锟斤拷锟金牨冩啀锛达拷鎲嶏己锟界槪绗旓拷铦忎€癸拷锟界疂锟斤拷稷爟锟斤拷浜欏瓭锟借┄锟芥挓锟斤拷锟斤拷惘勯尙锟借惛锟金冮础?"as" */
	while (*line == ' ' || *line == '\t') line++;
	if (strncmp(line, "as", 2) != 0) return 0;
	line += 2;
	if (*line != ' ' && *line != '\t') return 0;
	while (*line == ' ' || *line == '\t') line++;
	/* 锟斤拷锟斤拷璩婏拷锟斤拷锟界拪瑙佹紗锟斤拷杩勫殮銞囪潬锟藉暎锟芥挊鍦掋獥鐦炲墱锟斤拷瑭伙拷铦滐拷锟斤拷鑷挱鎲块煯銣鸿潕鍡ワ拷闆胯劊锟芥啀锛猴拷锟姐殮锟界槰锛革拷锟斤拷楹拷锟戒腑鍤氳異锟斤拷锟界笐锟斤拷锟斤拷鐟熱粊锟界礁榛侊拷锟斤拷锟借雹锟斤拷鍠诧拷鐦挤锟斤拷钀勶拷锟芥锟介潣锟斤拷锟藉焾婀讹拷鐞匡拷锟芥花锟界拠瀵ョ返锟芥毠楣碉拷锟斤拷鐦氾拷鈥垫啳璁狅拷锟斤拷锟斤拷鏆癸拷锟藉潝鍗斤拷稹儚锟斤拷锟斤拷锟藉梾锟斤拷?*/
	const char *alias_start = line;
	while (*line && *line != ' ' && *line != '\t' && *line != '\n' && *line != '\r') line++;
	size_t alias_len = (size_t)(line - alias_start);
	if (alias_len == 0) return 0;
	*out_path = malloc(path_len + 1);
	memcpy(*out_path, path_start, path_len);
	(*out_path)[path_len] = '\0';
	*out_alias = malloc(alias_len + 1);
	memcpy(*out_alias, alias_start, alias_len);
	(*out_alias)[alias_len] = '\0';
	return 1;
}

/* 鎲嶈悋馉綄锟借雹鑰拷璩㈡啢锟芥綌铻傝潳椐侀笜锟界閴勶拷鍏革拷锟藉仸锟藉殮锟斤拷铦伙拷锟斤拷鍠诧拷锟芥江鈪拷钀勷牕х懡妗冿拷闆跨畯灞佹喛鎬狅拷鎲掕彑锟斤拷稹伓锟斤拷穑硣铔剧懝锟芥洺鍤氥棁鈪★拷澧ц偧锟借涓拷銞庣硴鎲挎€狅拷绻虫喛瑾╋拷锟斤絹锟斤拷皈籍鎺冩啌鍟侊拷锟借常鈪拷鎶嗭拷鐬忔姌缃佺殯锟斤拷锟解姤锟斤拷闁э拷瀚栵拷锟芥妴锟芥拸鐓撅拷锟界潈锟斤拷鎬狆氾拷鎷氾拷锟金囷拷锟戒紣锟斤拷鏋忥拷锟借锟斤拷锟界鎽嫐锟芥暥閸︽枒锟藉槬锟斤拷闁栦伯锟藉暎鍧愶拷皈⒉锟借潨鍋︼拷锟藉墫锟斤拷绠忥拷锟姐瘎锟斤拷韬帮拷铦忔畨锟斤拷鎭嶏拷锟芥枃锟斤拷鎿ю濓拷锟芥喛闆磋瀭锟界榇★拷娓哥～鎾夝Ъ拷鎲掕硦锟斤拷绁夎荆锟界锟斤拷鍝拷鎾熴棝榇＄拝瀵э拷鎲滄彧鎾忔亶锟斤拷鍘╀锟藉仸锟斤拷鍫掞拷锟藉療锟斤拷娼拷缇擄拷锟金拷鎲枡锟斤拷馉按锟借彑皙儹鐬堬拷鍋达拷鑴╋拷闈金冿拷鐠嗭拷閳泬寤跺殫锟斤拷锟金溼粊鎾忓杩嗭拷锟斤拷锟借悇锟斤拷闁э拷锟藉墱锟介牘锟斤拷铦よ几锟斤拷鎬犲瀷鎲垮墰妤婏拷锟斤拷锟金滆兌鎲掟牶讹拷鐠囧锟界攬皈禂锟界瀳洵鹃铦ゆ江锟斤拷鐫冿拷鎲掕埞锟界拠绮广锟金囸旓拷瑭ㄦ穿鍎姺锟斤拷鍙燄讹拷瀵烇拷鎾忕锟斤拷绠忥拷鎾栨姌锟界拡锟斤拷缃革拷锟界锟借澔锟斤拷锟芥姌绻拷鍠熷簵锟姐洊锟芥啰稹锟芥懓稹嬀锟斤拷鍏革拷鎾燂拷锟斤拷鍓旀尵锟藉锟借潨锟斤拷鎷囬厜铦滃棯閷拷馥暍锟斤拷榄傦拷锟界緭锟芥拪銜诧拷锟芥€ワ拷锟借ǐ寤讹拷娓告膊锟藉彑锟斤拷璩★拷锟芥锟斤拷瀚ｆ拡鐬忔壋鍎掞拷锟斤拷锟芥€ワ拷锟界锟斤拷瑾╄繂锟斤拷锟斤拷璞氾拷琛€鎵橈拷鍠囷拷锟界灳锟斤拷鑺佛獌拷鐫冨鎽伴枡鎻啋鐞匡拷锟界杈ｏ拷绠革拷锟藉摠锟芥啌鑸€涓拷琛ｏ拷锟藉癁峄侊拷錉凤拷鎲€广創锟斤拷闃拷锟界簰鎲嶐滄挒锟戒簤锟斤拷銟鹃础鎾呮泟皙偣锟借鎺涳拷锟斤拷锟芥嫏锟斤拷瀵烇拷鍎絹皎嵀鐠嗭拷锟芥啰鐬撅拷锟借彑锟介浛韪愷啰鐭嬶拷鍤氬ⅶ鍣ゆ啞閸︼拷锟斤拷璁愶拷鎽冿拷锟借劊锟斤拷鑸樺剶铦庨锟斤拷姣猴拷锟借雹锟芥喅鍓濇徎鏁跺煗榘婃喛姊讹拷鐣惧◢涓拷鈯ワ拷锟藉澧欙拷妗冿拷馥焽閰夛拷鍞崇笐锟借姼锟姐殮绾掞拷锟芥湵铦忦Б茬嫛鐬忦夛拷闆垮槚鐑愯喅锟芥牸锟芥啞鐑愶拷鎾屼豢黏剟锟姐洊锟芥啰穰牅鎻拷銡氾拷锟金忥拷鐬堢奖锟斤拷鏍硷拷锟借雹锟芥挘鈭锟斤拷鎾忔締寤舵埈鑺ｉ埆锟斤拷铻傦拷鎬犺繂锟芥嫏锟斤拷馉墰锟斤拷鍒镐伯铦忥挤锟斤拷鐟熱粊锟金﹀欢锟金斤拷锟藉斁绉燂拷鑿燂拷鐦ㄣ缓缃搞锟藉垹锟斤拷锟斤拷锟金ǐワ拷鋫挎椏锟界绉愶拷锟斤拷锟斤拷锟芥€庢墖鐬堝垹悌炴啰鍦堝磿锟金讹拷鎲°猾鈴氾拷锟介础鐦ㄦ潯稷垢锟金牥达拷锟介枡鍣ょ槰穰Φ锟芥啞婀旓拷铦庛瘎锟斤拷锛锋櫤鎲嶈悇皈椌鐟规棩锟斤拷鎷嶇繒锟斤拷锟界懡闉鹃懍锟芥浮锟斤拷钀勶拷铦烉崇懀锟借常锟斤拷鍧旔鸿澐锟介尙锟金︼拷锟借锟界拠绮桂ⅴ拷鑴板鐬夋妴锟借潩?
   鎲嶈悋馉綄锟借雹鑰拷璩㈡啢锟芥綌铻傝潳椐侀笜锟界閴勮潖洵捐笌锟界吘锟芥拸锟戒浀锟借┗锟斤拷鑸拷锟借傅锟斤拷稹锟斤拷銞囨が鐦氾拷闃拷鍓濔勬拝鏇勶拷锟借尝锟斤拷稹绺濓拷娓涳拷锟?alias="math", "add: { a b } a b +" 锟?"math.add: { a b } a b +" */
static char *prefix_definitions(const char *src, const char *alias) {
	size_t alias_len = strlen(alias);
	size_t src_len = strlen(src);
	/* 锟斤拷锟斤拷璩婏拷锟斤拷锟界拪瑙佹紗锟斤拷锟界槥锟斤拷鐠嗗垹锟斤拷穑硣锕滐拷锛革拷锟界潈锟借澋闁栵拷闆胯雹锟斤拷鋱戯拷鎾忓湀锟斤拷锝嗭拷鎲嶈悋馉摷铦ｆ激锟界攬黏槳锟斤拷鏆癸拷锟金婏拷鍤欎鈪¤澔锟介厜鎾犲敵锟斤拷鍘╋拷鎾夝Ъ拷铦庡潝锟斤拷鎵筹拷锟斤拷锟斤拷鑸拷鎾呮毠锟界瀳鎯氾拷黏懗閬濓拷澧э拷閵村弮锟斤拷锟界潈锟斤拷鎷欏卜闁拷锟芥懓闁ч槷锟斤拷鍍愶拷铦疆锟介浛瀵烇拷锟借埄锟芥拸鑴查惁锟戒害锟芥啀锛烘粨锟芥經鐟ｆ啛璞拷閵答拷锟界憺皓憟鎲絹锟芥喛锛革拷鎾忓嫍锟斤拷锛癸拷锟斤拷锟借澐锟界絸锟界锟界党锟藉墱锟界瀳皈籍锟借潕鏆革拷鎲Б诧拷锟借硞铚擄拷鏆革拷鐨涳拷鈪￠姶鍞筹拷閳湀榇＄攬瑾枡锟介鍑冩挊婀э拷鍤楋拷穰劇鐬堝ⅶ穑枙锟芥毟锟界瀳锟斤拷锟金，氾拷锟金﹁縿鍤氾拷锟界瀳妗€锟斤拷绁堬拷鐦ｈ锟斤拷鑹撅拷锟斤拷鐠堬拷锟斤拷娼涚锟藉槬锟芥暪馉墰锟借潵鍟ｏ拷铦よ几榇℃埈鎬犺埈鎽伴枡锟芥挓閽咃拷锟界宕曪拷黏た锟芥懓璩冿拷铦榇★拷浜欙拷杓革拷鐦ㄥ湑缈х拪鎵庯拷锟姐棝绻拷琛岋拷锟借疀锟斤拷鍕楋拷锟斤拷锟界瀳鏍煎兗鎾忓槬锟斤拷姣猴拷锟借悇锟斤拷闉夛拷锟斤拷锟斤拷鐮嶈繂鐠呮經鐥旓拷馉墰锛掞拷锟借縿鍤氥棁鑻跨瀴瀵烉＊х拠椁呯锟藉爦锟界瀳皈籍锟斤拷鎯╋拷锟借儻锟斤拷绠革拷鍤椾牱锟界槞锟姐锟借帋锟芥拸鐓锯彋锟借姲鎸斤拷瑭癸拷锟斤拷锟斤拷锟斤拷闋╋拷锟介埈鍦堭勬挆璁狅拷鎾夊ⅶ锟芥拸銞涳拷锟斤拷穑噳锟斤拷韫憋拷鐠婃父稷锟芥锟斤拷鑴╋拷锟藉柍锟斤拷瑾€橈拷姣猴拷锟芥浮锟斤拷锟借縿鐨涴樻集鎲緭锟斤拷锟介紙锟斤拷锟斤拷鑺帮拷锟芥妴锟斤拷锟斤拷锟藉墧锟斤拷锟斤拷锟金拷锟斤拷锟斤拷锟斤拷浜︼拷锟界畯洌愶拷鍋γ拷锟斤拷穰Φ璧拷浜欐櫤锟借锟藉柈锟斤拷瀛碉拷锟借姺锟斤拷鍖ф挵鐬堜楝硷拷銟鹃础鎾呮泟皙埑铦忥拷鑲硷拷绁堬拷鐦ｐ拷闇傝劑皓憟锟芥箾铚囷拷鐞块笜锟斤几褰嶏拷黏槳鍑冪拠绠囨滑锟芥鑶氾拷锟金ぇ氭喅鍟ｏ拷锟借┄锟芥喛黏槳锟借潽椐侊拷锟斤拷濡擄拷馥獪婕曪拷稷爟锟斤拷鈭狅拷?alias + "." */
	size_t cap = src_len * 2 + 4096;
	char *out = malloc(cap);
	if (!out) return NULL;
	size_t out_len = 0;

	const char *line = src;
	while (*line) {
		const char *eol = strchr(line, '\n');
		if (!eol) eol = line + strlen(line);
		size_t line_len = (size_t)(eol - line);

		/* 鐬堝垹悌炴啰鐞匡拷锟金滐拷锟界﹥锟斤拷鐮嶏拷锟金滃櫒锟借疇锟芥懓皎盎锟芥挊鍦掔笣锟斤拷鏁拷锟斤拷锟界锟斤拷鑸€鍎掞拷鏍兼が锟金Ъ拷锟姐樉榀忥拷锟藉嚱锟斤拷馥暒锟斤拷錆硷拷锟借。锟藉珫锟斤拷黏﹤锟斤拷璩婏拷锟藉導锟斤拷馉瑣锟斤拷鍋︼拷锟借矈锟斤拷钀囬槷锟芥瀼寤讹拷鍠彋鎾夝Ъ拷锟芥媿锟斤拷绉嬶拷锟斤拷黏锟芥棩鍢★拷銞庯拷锟藉壒锟斤拷璩￠笜鐠呴瓊锟斤拷锟斤拷锟芥哗锟斤拷黏槳锟芥拸鑸愬啫锟芥壋銊烇拷皎€ｏ拷鎾橀锟借潱鏈涳拷锟介婀惰潽鍦掞拷锟介锟界槡锟介函闅炴疆锟借潖鐫凁樻啋鎬狆ぇ氾拷妗咃拷闆胯┄榈拷婢楋拷鎾忛灳锟斤拷鍠拷锟借┕锟借潝銟撅拷锟借锟介牘锟斤拷鎾ｅ梾宀凤拷锟藉湌锟界槰锛锋锟金ò濓拷锟介穑锟金牥村欢鎴€庯拷鐬堬綖锟斤拷惘勯笜鐠嗭拷锟介灳鋵拷鏂囨暍鎲＄儛锟斤拷璩★拷闁拷锟斤拷灏狅拷铦伙拷锟斤拷鐞匡拷鎾婏拷锟斤拷璞曠槥鍑借但鐢堟瀼锟斤拷皎皾锟斤拷黏槳锟斤拷鈯匡拷鐦炴湜棣瀳鈯ワ拷锟姐瘎锟斤拷鈭狅拷鐬堝ⅶ锟介埈鏆搁础锟藉墫锟芥喛浜わ拷鐠囬锟界攬闉熷紭鐠婃锟斤拷鑿燂箿鐢堟瀼鎾喛姊讹拷锟戒豢锟斤拷鍓栵拷鎲垮吀楹啹闁欙拷鐦炲墧锟斤拷瀵★拷锟介枛锟斤拷鍞筹拷锟界潈锟斤拷锝侌勬拝鏇勷绘挅鏍煎兗鎲掕硦锟斤拷鎶嗭拷锟斤拷铻傦拷鈯匡拷鍎毠鎻拷黏﹤锟斤拷? 锟斤拷锟斤拷璩婏拷锟斤拷锟斤拷娼旇瀭铦ゅ洳拷娓★拷铦ユ泬锟斤拷缃革拷鎾栬┄锟斤拷瑭ㄦ穿锟藉幇锟斤拷琛€鏅拷悒嗭拷鐟斤拷锟斤拷鐮嶏拷鎲垮墧锟芥挊椐侊拷鎲緭锟斤拷瑭ㄦ帥锟芥毠锟界拡锟界拠绠囷拷鐠呴鍋斤拷韪碉拷锟借尝绁氭啀稹仠黏锟藉敩铻傦拷鐓句伯锟芥姷鈥橈拷鎶嗭拷铦ゅ墲锟斤拷琛屸參鐦氾拷鍔戯拷鏇夊繓锟藉柈锟斤拷鑷拷鎾忋樉锟斤拷鍡嗭拷锟斤拷锟界槪婕わ拷锟介浼诧拷鍢ラ簳锟界锟斤拷鍡拷鎽拷璩掔瀳锝烇拷鐠囬璺硷拷婀旓拷锟藉槬皙娊鎲枛锟介柅锟斤拷锟金ò会嚎锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷椁呯笐锟界爫锟斤拷黏﹤锟藉硰锟借傅锟斤拷姊堕仢铦忥挤榈愯潖椁呴枓鎾熴棝锟斤拷鑴诧拷锟斤拷锟斤拷婊氬卜鐠堬拷锟斤絹缃囷拷钀勶拷锟藉ⅶ锟斤拷鍡拷锟?identifier 锟斤拷锟斤拷璩婏拷锟斤拷锟界拪瑙佹紗锟斤拷锟界槥锟斤拷鐠嗗垹锟斤拷鑿燂拷锟介锟斤拷穑偧锟斤拷锟斤拷闆胯笎锟斤拷鍏告紗闅為槨愦擄拷锟介畫锟斤拷鍡碉拷锟借悆锟芥拻锟金牶渿鐟规儵杩勶拷鑸癸拷锟金Б诧拷锟界吘锟芥拸锟戒浀锟介鍏濓拷鑿燂拷鍤氬導鎲旓拷瑭ㄦ化锟姐瘎锟界槰鍡碉拷铦忔湜穑枙锟斤拷绺э拷婊氾拷锟斤拷铻傦拷鈯匡拷鍎拷鏁︽啋鏂囦创锟斤拷锟斤拷鋩归精锟芥⒍锟斤拷姣猴拷鑿燂拷铦濆柈涓暪姊讹拷锟金牥达拷锟介枡鍣わ拷鍢ワ拷锟界畻锟?*/
		int is_def = 0;
		if (line_len > 0 && line[0] != ' ' && line[0] != '\t' && line[0] != '#' && line[0] != '!') {
			/* 锟斤拷锟斤拷璩婏拷锟斤拷锟斤拷黏徃馥亽锟斤拷榇★拷锟斤拷铦忓垹洫达拷缃革拷锟芥挘鍋﹂础鎲嶏拷锟斤拷鏅烇拷锟斤拷瑭ㄦ紗锟介灍锟斤拷鐮嶏拷鎲℃綐锟芥挊椐侊拷锟金婏拷锟介锟斤拷皎叝鎵橈拷鑸€鈪℃啰瀛叼撅拷鐮旓拷鎾忓柌锟芥啳缇擄拷鐬堬綖锟斤拷瑾拷锟戒簷锟借潖妗冮函鎲撹劋楦樻暪姊讹拷锟?*/
			for (size_t j = 0; j < line_len; j++) {
				if (line[j] == ':') { is_def = 1; break; }
				if (line[j] == ' ' || line[j] == '\t') break;
			}
		}

		/* 铦伙拷锟斤拷璩婏拷锟斤拷锟斤拷黏徃馥亽锟斤拷榇★拷锟斤拷锟借垚锟芥挓纭嬭锟斤拷鎲掓疆楦橈拷榻擄拷鎾夊ⅶ稹祮锟芥喛鍦濓拷鐬堚垷锟借潩銟剧返锟芥壋锟斤拷绠忥拷铦滃槬锟斤拷瑷が鐦氾拷稹酣铦几锚锟斤拷锟芥啞鑴〉嗭拷锟姐獥锟戒偪锟斤拷钀勯槷锟戒牱锟斤拷鐫冿拷锟藉馉Ё锟藉爳锟界槪鑺ｂ參鐦氾拷鍔戯拷鏆逛腑锟借劜锟芥拤澧п粊鎾忓湌锟斤拷绁夛拷锟藉墱榘婏拷婀涳拷锟藉帺锟斤拷妗€锟斤拷锟藉鎾樻锚锟斤拷锟界槥绁囷拷鐬堚姤锟斤拷鈯匡拷鐠嗭拷锟?*/
		while (out_len + line_len + alias_len + 4 >= cap) {
			cap *= 2;
			out = realloc(out, cap);
		}

		if (is_def) {
			/* 锟斤拷锟斤拷璩婏拷锟斤拷锟界拪瑙佹紗锟斤拷杩勫殮銞囪潬锟藉暎锟芥挊鍦掋獥鐦炲墱锟斤拷瑭伙拷铦滐拷锟斤拷鑷挱锟姐▎锟斤拷锟斤拷锟界拤鍡佃但锟介鎾掞拷馥獪锟斤拷椁呰€ㄧ拞锟界拤錉烽崼锟介枡锟?alias. 锟斤拷锟斤拷璩婏拷锟斤拷锟界拪瑙佹紗锟斤拷锟界槥锟斤拷鐠嗗垹锟斤拷鑿燂拷锟介锟斤拷婢楋拷锟藉湌锟介浛璞拷铦忕畯锟芥拸闁栶氾拷浜欙拷锟藉锟借潕鑸拷锟斤几锟斤拷瀵烇拷锟界补榇★拷杓诲瓭锟?*/
			memcpy(out + out_len, alias, alias_len);
			out_len += alias_len;
			out[out_len++] = '.';
		}
		memcpy(out + out_len, line, line_len);
		out_len += line_len;
		out[out_len++] = '\n';

		line = eol + (eol[0] ? 1 : 0);
	}
	out[out_len] = '\0';
	return out;
}

/* 闁拷锟介牆鑷拷鍤楌躲鎾栧锟?base_dir 锟?root_dir 闁拷锟介牆鑷拷鍤楋拷鍍庨馥獪锟斤拷鍟ｏ拷锟芥花锟芥懏銢栨车閵濊劘锟?
 * 鎲掞拷锟?path 锟?'/' 鎾橈拷鎲〉锟斤拷鍡点鎾栧锟?root_dir闁拷锟?
 * 椁堭オ滐拷锟斤拷锟借潥铏憽锟斤拷闁拷锟斤拷?free */
static char *resolve_path(const char *base_dir, const char *root_dir, const char *path) {
	const char *base = base_dir;
	const char *p = path;
	if (*p == '/') {
		base = root_dir;
		p++;
	}
	size_t blen = strlen(base);
	size_t plen = strlen(p);
	char *res = malloc(blen + plen + 2);
	if (!res) return NULL;
	memcpy(res, base, blen);
	if (blen > 0 && base[blen - 1] != '/' && base[blen - 1] != '\\' && plen > 0)
		res[blen++] = '\\';
	memcpy(res + blen, p, plen + 1);
	return res;
}

/* 锟斤拷锟斤拷璩婏拷锟斤拷锟界拪瑙佹紗锟斤拷锟界槥锟斤拷锟藉癁锟斤拷鍠裁拷缇擄拷锟戒馉蓟鎲枛鎿€鐠嗭拷锟斤拷瀵燂拷鎲撶兙鈥樻啳穑偧锟斤拷銞囷拷闋ㄧ黏剟鎾呴锟藉棯皎嵀鐬堣锟芥喛纾帮拷鎲撹嚞寤讹拷鑴⒐歌潖妗€锟借潩澧э拷锟芥壇锟界瀳澧э拷锟藉穑埐鎲掟牶讹拷鐠囬锟斤拷鍤氶畫锟介浛鑴お栵拷鍞撅拷鐠夊焾锟界瀳皈⒉鍌︼拷锟介偅锟借劑锟借洈锟斤拷鎶橈拷鎲硷拷锟戒€癸拷锟芥激锟斤拷鑷挱鎲块煯銣虹槤鑺革拷锟斤拷鐤欒潖銞囧兗锟藉í锟斤拷妯佹姌榇★拷锟斤拷鎲挎€狆ǒ傝潩黏槳锟斤拷鎵筹拷锟介锟斤拷鍋︼拷鍤氾拷锟芥啰鐬撅拷閳拷锟界緭锟斤拷稹儞锟斤拷鐑撅拷鐦氿Б拆お栵拷鍞撅拷鐠夌兙锟斤拷铏拷鎲掍偪锟芥啋璩滐拷锟斤拷锟芥挅鏍煎兗鎲拷锟芥捑锟斤拷鐟烇拷铦滃仸缃囷拷黏懗锟斤拷瑭ㄦ串锟介閬欑槰锛革拷锟藉導稹唶锟介畫锟斤拷鍟ｄ繄铦绘粴锟芥挓锟斤拷锟斤拷锟斤拷娼稿櫌铦拷鏇筹拷璩ｂ叀铦ュ墫楹㈤埈瀵ワ拷鐦ㄧ锟芥懓瑷拷锟斤拷鎶嗭拷锟斤拷锟斤拷锟斤拷锟金擄拷锟戒牱悒冪槰鍡碉拷锟芥€狆氱槪稹潡锟斤拷锟斤拷锟斤拷穑徆鎾屼豢锟斤拷鐓猴拷锟斤拷浯达拷绮归瘡锟借鑸拷宄曪拷锟界吘悒冪拞馉购閬濊澔锟借攩闇堣劋锟界槰锛革拷鎲嶈悋黏棃鍎棩皎嵀鎽脊锟介姶璁愯芳锟金斤拷锟借亞锟斤拷锟斤拷黏靖馉凯鐬夋潯锟藉笅鎾堬拷妗呮憵锟姐棁锟斤拷瑭拷锟界瑪锟界殯锟解參锟藉硶锟斤拷闃￠础锟界憻缈╂拤澧э拷锟介枛锟斤拷绁夌懀锟藉墱锟斤拷鍫掞拷铦氿婚础鎴姼锟借几锟斤拷绗旓拷鎲＄儛锟界槪鍓旓拷闆跨畯锟借潖洵撅拷鎾橀锟界拝鐟燂拷锟芥€狆『ㄨ澂鏆癸拷鎲嶈悋锟斤拷锟斤拷锟借常锟斤拷锟斤拷皈籍鎺冩啌鑸€涓拷锟斤拷鎯╋拷锟借笎锟斤拷鎶嗭拷锟界憰锟斤拷绗旓拷鎲嶈悇锟斤拷鑷拷锟芥媷鈪★拷黏槳锟介埈鑴诧拷锟藉帺锟芥懓瑷拷鎾忛崷锟斤拷琛ｅ伒锟斤拷锟斤拷鎭嶏拷鎲嶏拷鎾叉啀馉按锟斤拷闁栵拷鍤氿ぞ革拷锟金ぉ鸿偧铦濆瀺锟界拝娼涳拷鐠婃锟借潨锟斤箿鐠呯畻锟芥啞锟界笡闅°殮锟斤拷稹仠锟界憺锟介潣锟芥挓銡氾拷寮囷拷鐮嶏拷鎲℃父娲拷锟斤拷锟斤拷鎹讹拷璁狅拷鎾熺兙锟斤拷锟斤拷锟借┄鎺涳拷鍒革拷鎾楋拷锟斤拷婀涳拷闅炲仸洳潣韪癸拷鎲挎瀼锟芥暫锟斤拷锟金Ζ靛槨鐬堚垹锟斤拷銡氾拷鐠嗭拷锟斤拷姘拷鐦欒劋楫熸啳锟斤拷锟界憻锟界緟锟藉槬稷倸锟斤拷鐠婃枟锟借潕榻匡拷鎲嶏拷锟斤拷馥暒黏叿锟斤絹锟介柅锟斤拷鎲¤劔閲嗭拷锛峰欢锟芥媷钄楋拷钀勶拷锟藉癁锟芥毥锟斤拷鎲＄儛锟斤拷鐑攫勯浛瑭ㄣ煵锟戒帤锟芥挊妗咃拷鐦ㄥ椀铇拷锟斤拷锟戒簷鎵樼拞锟斤拷锟芥箾铚囷拷鐞筐勬啳瑙併鐦氬垹闃拷锟斤拷鎽ㄢ埅锟界瀳皈籍姒嗭拷锟斤拷锟芥牸锟斤拷鐑愰偅锟界爫锟斤拷鏃ヰ╋拷绌冩摢锟借雹閭﹁潕鍧旓拷鐦ｈ姡锟芥啞姘拷锟借嚞锟界拡锟斤拷鈯匡拷锟斤拷鐑撅拷锟金滐拷鐟曡縿锟界锟斤拷婢撅拷铦掔兙黏剟锟芥嫏濡ワ拷鍐斤拷铦滃仸悒冪拝绠忥拷铦伙拷绉熺拤鍡拷锟芥江鈪拷钀囬槷鎲掕场锟斤拷锟斤拷铦忔牸鍑冩挊椐侊拷鎲Б茶嬁锟借雹锟界瀴銞涳拷锟斤拷?free */
static char *dir_of(const char *path) {
	const char *lIR = strrchr(path, '/');
	if (!lIR) lIR = strrchr(path, '\\');
	if (!lIR) {
		char *cwd = malloc(3);
		if (cwd) { cwd[0] = '.'; cwd[1] = '\0'; }
		return cwd;
	}
	size_t len = (size_t)(lIR - path + 1);
	char *d = malloc(len + 1);
	if (!d) return NULL;
	memcpy(d, path, len);
	d[len] = '\0';
	return d;
}

/* 鎾呰ǐ锟斤拷锟斤拷?import 闇傚墫铇傛挓鍡咃拷锟芥腐钁电殲闉燂拷锟斤拷鍍庨姖绠忥拷閵濊姺榈懏銢栨车锟?
 * 椁堭オ滐拷锟斤拷鍍庯拷稹拪锟芥懏銢栨车閵濊帋锟斤拷锟介柆锟斤拷锟?free */
static char *expand_imports(const char *path, const char *base_dir, const char *root_dir) {
	FILE *in = fopen(path, "rb");
	if (!in) {
		mira_error_simple(1, "cannot open '%s'", path);
	}
	size_t cap = 8192;
	char *raw = malloc(cap);
	size_t n = 0;
	for (int c; (c = fgetc(in)) != EOF;) {
		if (n + 1 >= cap) { cap *= 2; raw = realloc(raw, cap); }
		raw[n++] = (char)c;
	}
	raw[n] = '\0';
	fclose(in);

	char *out = malloc(1);
	if (!out) { free(raw); return NULL; }
	size_t out_cap = 1, out_len = 0;

	const char *line = raw;
	while (*line) {
		const char *eol = strchr(line, '\n');
		if (!eol) eol = line + strlen(line);
		size_t line_len = (size_t)(eol - line);
		if (line_len > 0 && line[line_len - 1] == '\r') line_len--;

		/* import-ext "file.json" (闈欙拷?JSON 鎻掍欢寮曞叆) */
		/* !import "file" as name 锟?锟斤拷锟斤拷璩婏拷锟斤拷锟界拪瑙佹紗锟斤拷锟界槥锟斤拷鐠嗗垹锟斤拷鑿燂拷鎾婂導锟芥喛姊娥牻岋拷鑸拷闆胯雹閴勮潖琛わ拷锟界潈锟界拝鐟燂拷锟借悇锟芥啰鐞库€欙拷閬达拷锟斤拷锟芥啌绁嗚€ㄧ拝瑭拷锟芥牸锟界钩鎲胯楹愶拷绮癸拷锟界緭锟斤拷稹儞锟斤拷瑾╁欢锟金ぉ吼お栫拪鏂楋拷锟藉柌锟斤拷鏍硷拷鎲筐楀亙鎲冲敵涓拷锟斤拷鎲匡几锟斤拷鎷欙拷锟芥牸鋻忥拷锟界鐦氱惪皎嵀锟芥浮锟界瀳鎯捐潚鐑攫勶拷鎷欏眮鐬堟锟斤拷鐓撅拷鐦炲墫锟斤拷椁呭厺锟借劑閬欑槰锛凤拷锟介灡婊擄拷鍡拷锟斤拷锟斤拷鎷嗭拷鎾橀锟借潱鏈涳拷铦伙拷锟斤拷锟金ǐ￠潣锟界拠绠囷拷锟藉柈锟借澔锟斤拷锟戒亥锟斤拷璞拷锟藉導鏇筹拷鐓猴拷鎲筐楅闊愨娍閷拷黏た黏€绘啰?*/
		char *as_path = NULL, *as_alias = NULL;
		/* 锟斤拷锟斤拷璩婏拷锟斤拷锟斤拷娼旇瀭铦ゅ洳拷娓★拷铦ユ泬黏铦濆瀺锟斤拷锟借潩鎷囷拷鎾屾锟斤拷皈⒉锟斤拷瀵ユ憵锟斤拷馉摋锟借傅锟斤拷鑷拷鐦ｈ姡锟斤拷鍖э拷锟斤拷锟介潣锟借姦锟藉埢婊擄拷鐞跨笣鍎常鐑碉拷椁呴偅铦ゅ墲锟斤拷锟解叀锟芥經锟斤拷銞涢础锟借蓟瀹岋拷鑷拷鎾熼拝锟斤拷锟藉伒锟斤拷锟界拤锟斤拷锟藉敵锟斤拷锛凤拷锟藉仸锟斤拷鑴ｂ叅鎲¤劔锟斤拷鐓锯彋锟芥嫏锟金煎欢鍤楋拷锟界瀳鈭狅拷鎲掕硦锟斤拷鎾燄Б诧拷铦ｅ帺锟斤拷皈⒉峄佹拸闉撅拷锟解埅锟斤拷穑埊锚锟斤拷锟斤拷黏徃馥亽锟藉棯榀忥拷锟界礁锚虒鍎拷鏅喛閸︼拷锟金拷锟借劊锟借潖椁凁猴拷浠挎憵锟斤拷璩￠笜锟界爫鎾版喛浜︿锟金い匡拷锟斤拷鎾犳€庯拷锟借鍏啰馉按锟芥拸閸︼拷锟借尝锟界拪瑙佹紗锟斤拷锟界槥锟斤拷鐠嗗垹锟斤拷穑硣锕滐拷锛革拷锟芥妴锟斤拷杓革拷鎲凤拷锟界畯锟斤拷稹锟斤拷浜ら磦锟姐棁锟斤拷浠夸伯锟芥締淇堬拷椁冩急铦炲仸绀讹拷鎶掞拷锟?null 铦伙拷锟斤拷璩婏拷锟斤拷锟斤拷娼旇瀭铦ゅ洳拷娓★拷铦ユ泬锟斤拷锟界笣鍤氾拷惘勯础鎲嶏拷缈╋拷鋷氾拷鎾樻恭锟界槡瑙侊拷锟借厛锟介姖婊氳縿锟金婐⒐革拷黏槳锟界拠椐侊拷鐦炴締锟斤拷璞獌潣瀵ワ拷鐦ㄥ盃锟藉殮穰挊锟芥喛皈禂锟斤拷闁栨憵锟斤拷鍓滐拷锟芥父鑵规拤皈籍娲╋拷鐓剧返锟斤拷銊烇拷锟斤拷锟藉湌锛冿拷榻撻穩锟借厛锟斤拷姊讹拷锟斤拷锟斤拷锟斤拷铦滃仸绺濈攬璁愶拷锟借雹锟芥啞稷爟锟斤拷姣猴拷锟藉埗锟芥啰闁栵拷铦忔潯锟斤拷锟斤拷锟借锟介柅锟斤拷锟藉枊锟斤拷稹潡鍣拷瀚帮拷锟金兼锟界瑪寤讹拷璩ｇ硟鎲匡几锟芥拸鍕楋拷鐢堣瓖锟斤拷锟斤拷鐦ｏ絹宕曠槪鑺ｏ拷鎲★拷穑枙锟介枡涓拷绗斿瑣锟金Б查枓锟解娍悒冪拝娼拷锟界畯锟借潿锟借潖洵撅拷铦滃棯锟界槥锟金牴昏澔锟斤拷锟藉棩璧拷皎€ｆ彚锟芥€掞拷锟戒豢锟?*/
		char *tmp_line = malloc(line_len + 1);
		memcpy(tmp_line, line, line_len);
		tmp_line[line_len] = '\0';
		int as_is_lib = 0;
		if (parse_import_as_line(tmp_line, &as_path, &as_alias, &as_is_lib)) {
			free(tmp_line);
			char *resolved;
			int is_binary = 0;
			if (as_is_lib) {
				resolved = resolve_lib_path(as_path, &is_binary);
			} else {
				resolved = resolve_path(base_dir, root_dir, as_path);
			}
			free(as_path);
			if (!resolved) { free(as_alias); free(raw); free(out); return NULL; }
			if (already_imported(resolved)) {
				free(resolved); free(as_alias);
				line = eol + (eol[0] ? 1 : 0);
				continue;
			}
			push_imported(resolved);
			if (is_binary) {
				free(resolved); free(as_alias);
				line = eol + (eol[0] ? 1 : 0);
				continue;
			}
			char *child_dir = dir_of(resolved);
			char *child_src = expand_imports(resolved, child_dir, root_dir);
			free(child_dir);
			free(resolved);
			if (!child_src) { free(as_alias); free(raw); free(out); return NULL; }
			/* 铦伙拷锟斤拷璩婏拷锟斤拷锟斤拷娼旇瀭铦ゅ洳拷娓★拷铦ユ泬锟斤拷锟界笣鍤氾拷惘勯础鎲嶏拷缈╋拷鋷氾拷鎾樻恭锟界槡瑙侊拷閵村敵锟借潳椐侌獌拷绠忓瓭锟界搻锟斤拷鈯匡拷锟借锟斤拷稹亸楝诧拷鐓撅拷锟藉導钂炬啀铔旓拷锟姐棝鍢★拷绁嗕腑锟斤拷锟借澔锟金堣潣瀛碉拷锟斤几锟芥懓鐡愶拷鐦ｅ墲锕滅槰瑕€锟斤拷鑰曪拷锟芥啀铔旓拷鍎嫐锟藉劗锟藉硥锟金﹂槷閳█ｄ腑锟借嚞锟借潵鍓栵拷锟界礁锟斤拷閬达拷锟藉療锟斤拷锟界锟借ǐ锟芥啀鑿滐拷鐦氾拷锟斤拷稹仠锟界憺锟介潣锟芥挓銡氾拷锟借常锟斤拷鑸橈拷鍤氾拷悌炴啀铔旓拷锟藉仸锟斤拷椁咃拷锟藉稹唶锟金Ζ碉拷鑿滆笌鎲冲湀锟斤拷绉嬶拷鐨滒滒氾拷鏂囧欢锟借彍褰嶏拷皈⒉锟斤拷鎲°猾锟借潳椐佷伯锟芥壒锟斤拷鍠抽孩锟界吘锟斤拷鎬ラ够鎲嶈洈锟斤拷椐侊箿鐢堟瀼鎾喛姊讹拷锟戒簷寤讹拷韪愶拷鎲°棁锟芥啹闁欙拷鐦炲墧锟斤拷瀵ワ拷锟界兙锟斤拷锟介皧鎲挎⒍锟斤拷锟戒腑锟解姤锟斤拷瀛叼鸿潥缃革拷鐦炵爫锟借澔锟斤拷闇堣劊姹欙拷鎾忕吘缃囪潖瑕€锟芥啞婀涳拷闇呴槨锟芥毥锟芥が锟芥亙锟芥啌鍟侊拷锟借硿皙姶鎾夝Ъ穿锟界吘锟斤拷锟斤拷锟斤拷锟界瀼鎶樷€欙拷琛€鎾炴啀馉按锟斤拷闁栦伯棰叉毟锟斤拷黏槳锟借潨鍋︾笣锟斤拷锟芥啰稹澂锟斤拷鏆革純闋濆喗锟斤拷闁с锟界憺锟介浛璩＄～锟借垁锟斤拷浠匡拷锟斤拷锟斤拷璞氾拷锟斤拷闅烉Б测參鐦氾拷钁わ拷稷爟黏剟锟芥锟芥懓闁欋粊鎲掑湌锟介浛璞㈡倓鐬堭Ъ珯鎲恭锟斤拷鍟ｆ啢锟戒紞锟斤拷缃葛勶拷稹伓锟借潖鎲撅拷锟芥啰鐬撅拷锟借贡鑰ㄦ啀锟界洈锟界憰悃ユ拸鍐斤拷锟藉吀鑲熺瀳鈭狅拷锟芥姃锟界槥鍡呮彃锟介灡瀣曪拷琛ｏ拷锟斤拷锟斤拷鍗濓拷锟藉仸锟斤拷鍠徎锟借┄锟界锟斤拷锟芥啋璩婏拷鎲嶏拷鑹旀啀钀囸堝劗鏃ョ巩锟戒锟斤拷鍡夝掞拷閸﹁縿鍤氾拷閮庯拷銟撅拷锟解娍悛楋拷锛凤紩锟借悇锟斤拷娓革拷锟芥媶锟借澔锟角滐拷?*/
			char *prefixed = prefix_definitions(child_src, as_alias);
			free(child_src);
			free(as_alias);
			if (!prefixed) { free(raw); free(out); return NULL; }
			size_t plen = strlen(prefixed);
			while (out_len + plen + 2 >= out_cap) {
				out_cap *= 2;
				out = realloc(out, out_cap);
				if (!out) { free(raw); free(prefixed); return NULL; }
			}
			memcpy(out + out_len, prefixed, plen + 1);
			out_len += plen;
			if (plen > 0 && prefixed[plen - 1] != '\n') {
				out[out_len++] = '\n';
				out[out_len] = '\0';
			}
			free(prefixed);
			line = eol + (eol[0] ? 1 : 0);
			continue;
		}
		free(tmp_line);

		int imp_is_lib = 0;
		char *imp_path = parse_import_line(line, &imp_is_lib);
		if (imp_path) {
			char *resolved;
			int is_binary = 0;
			if (imp_is_lib) {
				resolved = resolve_lib_path(imp_path, &is_binary);
			} else {
				resolved = resolve_path(base_dir, root_dir, imp_path);
			}
			free(imp_path);
			if (!resolved) { free(raw); free(out); return NULL; }
			if (already_imported(resolved)) {
				free(resolved);
				line = eol + (eol[0] ? 1 : 0);
				continue;
			}
			push_imported(resolved);
			if (is_binary) {
				free(resolved);
				line = eol + (eol[0] ? 1 : 0);
				continue;
			}
			char *child_dir = dir_of(resolved);
			char *child_src = expand_imports(resolved, child_dir, root_dir);
			free(child_dir);
			free(resolved);
			if (!child_src) { free(raw); free(out); return NULL; }
			size_t clen = strlen(child_src);
			while (out_len + clen + 2 >= out_cap) {
				out_cap *= 2;
				out = realloc(out, out_cap);
				if (!out) { free(raw); free(child_src); return NULL; }
			}
			memcpy(out + out_len, child_src, clen + 1);
			out_len += clen;
			if (clen > 0 && child_src[clen - 1] != '\n') {
				out[out_len++] = '\n';
				out[out_len] = '\0';
			}
			free(child_src);
		} else {
			while (out_len + line_len + 2 >= out_cap) {
				out_cap *= 2;
				out = realloc(out, out_cap);
				if (!out) { free(raw); return NULL; }
			}
			memcpy(out + out_len, line, line_len);
			out_len += line_len;
			out[out_len++] = '\n';
			out[out_len] = '\0';
		}
		line = eol + (eol[0] ? 1 : 0);
	}
	free(raw);
	return out;
}

/* 锟界憰锟芥暥鏋忥拷鐢囷拷閵佽澔黏锟斤拷锟斤拷闅炲梾锟藉殫锟斤拷 parser_do_import 锟金躲鎾栧妤濇暫锟藉溇锟界悳锟?*/
static Compiler *g_current_compiler = NULL;
static const char *comp_current_file(void) {
	return g_current_compiler ? g_current_compiler->filename : NULL;
}

/* 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟?
 * parser_do_import 锟?parser.c 锟?!import 锟斤拷妤濇暫锟藉溇锟金硷拷锟?
 * 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟?*/
void parser_do_import(const char *path, const char *alias, int is_lib) {
	char *resolved = NULL;
	int is_binary = 0;

	if (is_lib) {
		resolved = resolve_lib_path(path, &is_binary);
		if (!resolved) {
			resolved = malloc(strlen(libs_dir) + strlen(path) + 16);
			sprintf(resolved, "%s%s.mira", libs_dir, path);
			FILE *tf = fopen(resolved, "rb");
			if (!tf) { free(resolved); return; }
			fclose(tf);
		}
	} else {
		const char *cur_file = comp_current_file();
		char *cur_dir = dir_of(cur_file ? cur_file : ".");
		resolved = resolve_path(cur_dir, exe_dir, path);
		free(cur_dir);
	}

	if (!resolved) return;
	if (is_binary) { free(resolved); return; }

	if (already_imported(resolved)) { free(resolved); return; }
	push_imported(resolved);

	if (!lexer_push_file(resolved, alias)) {
		/* 锟斤拷杈ｏ拷鏇嗭拷锟藉爢锟斤拷錉凤拷闋濆斁锟?*/
	}
	free(resolved);
}

/* parser_do_import_ext 澶勭悊 `import-ext "file.json"` 璇箟 */
/* compile_file: 锟芥箶穰粬闇傞锟?.mira鍤椾敹锟斤拷婊╃播 expand_imports 锟斤拷皈嫤锟芥江穰粬锟?*/
void compile_file_obj(const char *path, const char *obj_path) {
	int profile_enabled = getenv("MIRA_COMPILE_PROFILE") != NULL;
	clock_t profile_begin = profile_enabled ? clock() : 0;
	imported_count = 0;
	for (int i = 0; i < IMPORT_MAX; i++) imported[i] = NULL;

	/* === 鍔拷?DLL 鎵╁睍锛氭壂锟?dll-map/ 鏂囦欢锟?=== */
	{
		/* 璁＄畻婧愭枃浠舵墍鍦ㄧ洰锟?*/
		char src_dir[512] = {0};
		const char *lIR_sep = strrchr(path, '\\');
		if (!lIR_sep) lIR_sep = strrchr(path, '/');
		if (lIR_sep) {
			int len = (int)(lIR_sep - path);
			if (len >= (int)sizeof(src_dir)) len = (int)sizeof(src_dir) - 1;
			memcpy(src_dir, path, len);
			src_dir[len] = '\0';
		} else {
			src_dir[0] = '.'; src_dir[1] = '\0';
		}
	}
	clock_t profile_scan = profile_enabled ? clock() : 0;

	/* Read source file into memory for parser */
	FILE *f = fopen(path, "rb");
	if (!f) mira_error_simple(1, "cannot open '%s'", path);
	fseek(f, 0, SEEK_END);
	long fsz = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *src = (char *)malloc((size_t)fsz + 1);
	fread(src, 1, (size_t)fsz, f);
	src[fsz] = '\0';
	fclose(f);
	clock_t profile_read = profile_enabled ? clock() : 0;

	push_imported(strdup(path));

	Compiler c = {0};
	c.src = src;
	c.out_path = obj_path;
	c.out = NULL;
	c.filename = path;
	g_current_compiler = &c;


	Program *prog = parser_parse(&c);
	clock_t profile_parse = profile_enabled ? clock() : 0;
	codegen(&c, prog);
	clock_t profile_codegen = profile_enabled ? clock() : 0;

	/* IR optimization passes */
	IrBuffer *ir = &cg->ir;
	if (mira_opt_level >= 2) {
		ir_opt_constant_fold(ir);
		ir_opt_strength_reduce(ir);
		ir_opt_const_fold_div(ir);
		ir_opt_redundant_load(ir);
		ir_opt_peephole(ir);
	}
	if (mira_opt_level >= 3) {
		ir_opt_ilp_schedule(ir);
	}
	clock_t profile_ir_opt = profile_enabled ? clock() : 0;

	/* Encode IR to machine code */
	EncodeResult enc = {0};
	int ret = ir_encode(ir, &enc);
	if (ret != 0) {
		mira_error_simple(1, "IR encoding failed for '%s'", path);
	}
	clock_t profile_encode = profile_enabled ? clock() : 0;

	/* Write object file:COFF(Win64)或 ELF(SysV)。
	 * 编译期宏分支:链接器后端始终匹配宿主平台,
	 * Linux 构建因此不引用 coff_write_obj。 */
#ifdef _WIN32
	ret = coff_write_obj(&enc, ir, obj_path);
#else
	ret = elf_write_obj(&enc, ir, obj_path);
#endif
	if (ret != 0) {
		mira_error_simple(1, "failed to write object file '%s'", obj_path);
	}
	clock_t profile_obj = profile_enabled ? clock() : 0;
	if (profile_enabled)
		fprintf(stderr,
		        "compile-profile scan=%.3f read=%.3f parse=%.3f "
		        "codegen=%.3f ir-opt=%.3f encode=%.3f obj=%.3f total=%.3f\n",
		        mira_profile_ms(profile_begin, profile_scan),
		        mira_profile_ms(profile_scan, profile_read),
		        mira_profile_ms(profile_read, profile_parse),
		        mira_profile_ms(profile_parse, profile_codegen),
		        mira_profile_ms(profile_codegen, profile_ir_opt),
		        mira_profile_ms(profile_ir_opt, profile_encode),
		        mira_profile_ms(profile_encode, profile_obj),
		        mira_profile_ms(profile_begin, profile_obj));

	encode_result_free(&enc);
	free(src);
	for (int i = 0; i < imported_count; i++) free((void *)imported[i]);
}

/* compile_file_ir_dump: compile .mira and dump IR text (for -S flag) */
void compile_file_ir_dump(const char *path, const char *out_path) {
	imported_count = 0;
	for (int i = 0; i < IMPORT_MAX; i++) imported[i] = NULL;

	FILE *f = fopen(path, "rb");
	if (!f) mira_error_simple(1, "cannot open '%s'", path);
	fseek(f, 0, SEEK_END);
	long fsz = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *src = malloc((size_t)fsz + 1);
	fread(src, 1, (size_t)fsz, f);
	src[fsz] = '\0';
	fclose(f);

	push_imported(strdup(path));

	Compiler c = {0};
	c.src = src;
	c.out_path = out_path;
	c.out = NULL;
	c.filename = path;
	g_current_compiler = &c;


	Program *prog = parser_parse(&c);
	codegen(&c, prog);

	IrBuffer *ir = &cg->ir;
	FILE *out = fopen(out_path, "w");
	if (!out) {
		mira_error_simple(1, "cannot write '%s'", out_path);
	}
	ir_dump(ir, out);
	fclose(out);
	
	for (int i = 0; i < imported_count; i++) free((void *)imported[i]);
}

/* 锟斤拷锟斤拷璩婏拷鎾滃啎瀵ユ憵锟姐棁锟斤拷皎叝锟界殱锟斤拷铦忕潈黏槝鎲掓€狆疯潕椐侊拷铦忔潯锟斤拷瀵★拷锟介槨鎼囩攬黏槳锟芥啰鐬撅拷鐟藉楫燂拷鎵瑰煄鎾夊ⅶ锟芥挓瑙佸欢锟借姡锟界瀳洵炬尓锟藉仸鈥垫啳闉夛拷锟芥黏棃锟界礁楫熸啞韪癸拷锟斤拷闅烉Б诧拷锟芥嫏锟斤拷锟界敘鎲＄儛鍋滆潳娼伯锟斤拷锟借潝璁愶拷闈橈拷锟斤拷楹拷锟斤拷锟界槥绁夛拷锟藉锟借澐锟界筏锟借锟斤拷鑱嗭拷锟斤拷鍓╋拷锟斤拷黏槳楝诧拷锟斤拷锟斤拷锟借潖鏇勶拷锟斤拷皈⒉鍑冿拷瀵ワ拷锟借矈锟斤拷锟借澊锟藉湌锟芥暪馉墰锟芥喛閸︷ぇ氳潪鑴ら疅鏁硅ǐ濡ｆ啋鑴ｏ拷锟藉吀鏃啳瑭ㄩ墑鐠婃铏燂拷鎯╋拷锟藉锟芥啋鑷笡閳姺锟斤拷鍙燂拷锟金ò濓拷锟戒帤锟斤拷鍒革拷锟斤拷锟介瘡? .mira 锟?.asm 锟?.obj 锟?.exe */

static void full_build(const char *mira_path) {
	int profile_enabled = getenv("MIRA_COMPILE_PROFILE") != NULL;
	clock_t profile_begin = profile_enabled ? clock() : 0;
	/* 锟?.mira 锟斤拷锟斤拷璩婏拷鎾滃啎瀵ユ憵锟斤拷闃￠础锟界﹥锟斤拷鍧旓拷锟界锟界槪纭嬭繂鍎尝锟界槡婢嗭拷锟戒豢黏剟锟芥嫏锟斤拷鎶嗭拷锟斤拷锟芥暥閸︼拷锟金肩珯锟借悇寤讹拷鑴╋拷锟金ぉ猴拷鎾樿硦锟斤拷纭猴拷锟藉锟介埈楫嬸ò圭攬黏槳妞懓锟芥埈绁夊鐠嗗垹锟界槪鍓濓箿鐢堭拷锟芥隘锟斤拷姊拷锟借喊锟芥喛鍓栵拷锟姐殮锟斤拷娼涳拷锟藉鍣㈣潪鐓撅拷锟芥江鍌︽啀钀勷虹懡璨婏拷锟斤脊瀹岋拷宄曪拷闅炶。锟界槡瑙侊拷鎲掓枃楹㈣澐鍘颁伯锟芥浮锟斤拷?basename */
	const char *base = strrchr(mira_path, '\\');
	if (!base) base = strrchr(mira_path, '/');
	base = base ? base + 1 : mira_path;
	char basename[256];
	strncpy(basename, base, sizeof(basename) - 1);
	basename[sizeof(basename) - 1] = '\0';
	char *dot = strrchr(basename, '.');
	if (dot) *dot = '\0';

	/* 锟斤拷锟斤拷璩婏拷鎾滃啎瀵ユ憵锟斤拷闃￠础锟界﹥锟斤拷鍧旓拷锟界锟界槪纭嬭繂鍎尝锟界槡婢嗭拷锟戒豢黏剟锟芥嫏锟斤拷鎶嗭拷锟斤拷锟芥暥閸︼拷锟金肩珯锟借悇寤讹拷鑴╋拷锟金ぉ猴拷鎾樿硦锟斤拷纭猴拷?*/
	char obj_path[512], exe_path[512];
#ifdef _WIN32
	snprintf(obj_path, sizeof(obj_path), "out\\%s.obj", basename);
	snprintf(exe_path, sizeof(exe_path), "%s.exe", basename);
#else
	snprintf(obj_path, sizeof(obj_path), "out/%s.o", basename);
	snprintf(exe_path, sizeof(exe_path), "%s", basename);
#endif

	/* 铦伙拷锟斤拷璩婏拷锟斤拷锟斤拷黏徃馥亽锟斤拷榇★拷锟斤拷锟借垚锟芥挓纭嬭锟斤拷鎲掓疆楦橈拷榻擄拷鎾夊ⅶ稹祮锟芥喛鍦濓拷鐬堚垷锟借潩銟剧返锟芥壋锟斤拷绠忥拷铦滃槬锟斤拷瑷が鐦氾拷稹酣铦几锚?out/ 锟斤拷锟斤拷璩婏拷鎾滃啎瀵ユ憵锟姐棁锟斤拷皎叝锟斤拷锟芥喛闆磋澏锟姐瘎黏剟锟芥锟斤拷鍩濓拷锟金　炵絿鎾忥拷锟芥啀鑲呭暎锟姐瘎锟藉劗锟斤拷婕わ拷鎽お栵拷椁呴偅锟金伙拷锟斤拷锟芥喛瑷孩鎾樻锟?*/
#ifdef _WIN32
	_mkdir("out");
#else
	mkdir("out", 0777);
#endif

	/* 1. 铦伙拷锟斤拷璩婏拷锟斤拷锟斤拷娼旇瀭铦ゅ洳拷娓★拷铦ユ泬锟斤拷缃革拷鎾栬硟馉铦忔毠锟界殱姊㈢伐锟斤拷锟斤拷鎬ワ拷锟借劋锟借潝閬达拷鎲跨﹥锟芥挊椐侊拷鎲緭锟界拪鏂楋拷锟芥壋绺戠槪鑺ｂ叀?.mira 锟?.asm */
	printf("[1/2] Compiling %s...\n", mira_path);
	compile_file_obj(mira_path, obj_path);
	clock_t profile_compile = profile_enabled ? clock() : 0;

		/* 鎵弿 section headers 鎵?.text */

	/* 2. Selective runtime linking: scan obj for undefined symbols, link only needed modules */
	printf("[2/2] Linking...\n");

	/* Build runtime directory path (alongside mira.exe) */
	char rt_dir[512];
	size_t ldir_len = strlen(libs_dir);
	if (ldir_len > 10) {
		memcpy(rt_dir, libs_dir, ldir_len - 10);
#ifdef _WIN32
		strcpy(rt_dir + ldir_len - 10, "runtime\\");
#else
		strcpy(rt_dir + ldir_len - 10, "runtime/");
#endif
	} else {
#ifdef _WIN32
		strcpy(rt_dir, "runtime\\");
#else
		strcpy(rt_dir, "runtime/");
#endif
	}

	/* Symbol-to-module mapping table */
	static const struct { const char *sym; const char *mod; } sym_map[] = {
		/* rt_print */
		{"mira_print",           "rt_print" RT_OBJ_EXT},
		/* rt_time */
		{"mira_time_ms",         "rt_time" RT_OBJ_EXT},
		{"mira_time_now",        "rt_time" RT_OBJ_EXT},
		/* rt_mem */
		{"mem_alloc",            "rt_mem" RT_OBJ_EXT},
		{"mem_free",             "rt_mem" RT_OBJ_EXT},
		{"mem_move",             "rt_mem" RT_OBJ_EXT},
		{"mem_erase",            "rt_mem" RT_OBJ_EXT},
		{"mira_mem_dump",        "rt_mem" RT_OBJ_EXT},
		/* rt_string */
		{"mira_str_len",         "rt_string" RT_OBJ_EXT},
		{"mira_str_concat",      "rt_string" RT_OBJ_EXT},
		{"mira_str_copy",        "rt_string" RT_OBJ_EXT},
		{"mira_str_eq",          "rt_string" RT_OBJ_EXT},
		{"mira_str_contains",    "rt_string" RT_OBJ_EXT},
		{"mira_str_trim",        "rt_string" RT_OBJ_EXT},
		{"mira_str_at",          "rt_string" RT_OBJ_EXT},
		{"mira_str_substr",      "rt_string" RT_OBJ_EXT},
		{"mira_int_to_str",      "rt_string" RT_OBJ_EXT},
		{"mira_to_str",          "rt_string" RT_OBJ_EXT},
		{"mira_str_to_int",      "rt_string" RT_OBJ_EXT},
		{"mira_int_to_float",    "rt_string" RT_OBJ_EXT},
		{"mira_float_to_int",    "rt_string" RT_OBJ_EXT},
		/* rt_collection */
		{"mira_list_new",        "rt_collection" RT_OBJ_EXT},
		{"mira_list_len",        "rt_collection" RT_OBJ_EXT},
		{"mira_list_get",        "rt_collection" RT_OBJ_EXT},
		{"mira_list_set",        "rt_collection" RT_OBJ_EXT},
		{"mira_list_free",       "rt_collection" RT_OBJ_EXT},
		{"mira_list_push",       "rt_collection" RT_OBJ_EXT},
		{"mira_list_pop",        "rt_collection" RT_OBJ_EXT},
		{"mira_dict_new",        "rt_collection" RT_OBJ_EXT},
		{"mira_dict_set",        "rt_collection" RT_OBJ_EXT},
		{"mira_dict_get",        "rt_collection" RT_OBJ_EXT},
		{"mira_dict_has",        "rt_collection" RT_OBJ_EXT},
		{"mira_dict_free",       "rt_collection" RT_OBJ_EXT},
		{"mira_dict_keys",       "rt_collection" RT_OBJ_EXT},
		{"mira_dict_count",      "rt_collection" RT_OBJ_EXT},
		/* rt_file */
		{"mira_file_read",       "rt_file" RT_OBJ_EXT},
		{"mira_file_write",      "rt_file" RT_OBJ_EXT},
		{"mira_file_append",     "rt_file" RT_OBJ_EXT},
		{"mira_file_exists",     "rt_file" RT_OBJ_EXT},
		{"mira_file_delete",     "rt_file" RT_OBJ_EXT},
		/* rt_math */
		{"mira_abs",             "rt_math" RT_OBJ_EXT},
		{"mira_min",             "rt_math" RT_OBJ_EXT},
		{"mira_max",             "rt_math" RT_OBJ_EXT},
		{"mira_f_sqrt",          "rt_math" RT_OBJ_EXT},
		{"mira_f_pow",           "rt_math" RT_OBJ_EXT},
		{"mira_f_floor",         "rt_math" RT_OBJ_EXT},
		{"mira_f_ceil",          "rt_math" RT_OBJ_EXT},
		{"mira_random",          "rt_math" RT_OBJ_EXT},
		{"mira_random_range",    "rt_math" RT_OBJ_EXT},
		{"mira_random_seed",     "rt_math" RT_OBJ_EXT},
		/* rt_error */
		{"mira_try_call",        "rt_error" RT_OBJ_EXT},
		{"mira_try_begin",       "rt_error" RT_OBJ_EXT},
		{"mira_try_end",         "rt_error" RT_OBJ_EXT},
		{"mira_throw",           "rt_error" RT_OBJ_EXT},
		{"mira_get_error",       "rt_error" RT_OBJ_EXT},
		/* rt_debug */
		{"mira_dump_data_stack", "rt_debug" RT_OBJ_EXT},
		{"mira_dump_var_slot",   "rt_debug" RT_OBJ_EXT},
		{"mira_dump_vars",       "rt_debug" RT_OBJ_EXT},
		{"mira_stats",           "rt_debug" RT_OBJ_EXT},
		{"mira_debug_break",     "rt_debug" RT_OBJ_EXT},
		{"mira_debug_step",      "rt_debug" RT_OBJ_EXT},
		{"mira_debug_next",      "rt_debug" RT_OBJ_EXT},
		{"mira_debug_continue",  "rt_debug" RT_OBJ_EXT},
		{"mira_dump_return_stack","rt_debug" RT_OBJ_EXT},
		{"mira_dump_type_stack", "rt_debug" RT_OBJ_EXT},
		{"mira_backtrace",       "rt_debug" RT_OBJ_EXT},
		{"mira_where",           "rt_debug" RT_OBJ_EXT},
		{"mira_watch_not_supported","rt_debug" RT_OBJ_EXT},
		/* rt_io */
		{"mira_read_int",        "rt_io" RT_OBJ_EXT},
		{"mira_input",           "rt_io" RT_OBJ_EXT},
		/* rt_struct */
		{"mira_struct_new",      "rt_struct" RT_OBJ_EXT},
		{"mira_struct_free",     "rt_struct" RT_OBJ_EXT},
		/* rt_win */
#ifdef _WIN32
		{"mira_win_msgbox",      "rt_win" RT_OBJ_EXT},
		{"mira_win_sleep",       "rt_win" RT_OBJ_EXT},
		{"mira_win_shell",       "rt_win" RT_OBJ_EXT},
		{"mira_win_env",         "rt_win" RT_OBJ_EXT},
		{"mira_win_env_set",     "rt_win" RT_OBJ_EXT},
		{"mira_win_clip_get",    "rt_win" RT_OBJ_EXT},
		{"mira_win_clip_set",    "rt_win" RT_OBJ_EXT},
		{"mira_win_beep",        "rt_win" RT_OBJ_EXT},
		{"mira_win_beep_freq",   "rt_win" RT_OBJ_EXT},
		{"mira_win_set_title",   "rt_win" RT_OBJ_EXT},
		{"mira_win_color_set",   "rt_win" RT_OBJ_EXT},
		{"mira_win_cursor_move", "rt_win" RT_OBJ_EXT},
		{"mira_win_screen_width","rt_win" RT_OBJ_EXT},
		{"mira_win_screen_height","rt_win" RT_OBJ_EXT},
		{"mira_win_pid",         "rt_win" RT_OBJ_EXT},
		{"mira_win_tick",        "rt_win" RT_OBJ_EXT},
		{"mira_win_tick_ns",     "rt_win" RT_OBJ_EXT},
		{"mira_async_start",     "rt_win" RT_OBJ_EXT},
		{"mira_async_yield",     "rt_win" RT_OBJ_EXT},
		{"mira_parallel_start",  "rt_win" RT_OBJ_EXT},
		{"mira_parallel_join",   "rt_win" RT_OBJ_EXT},
#else
		/* Linux:rt_win 未移植(里程碑 4)。clock-ns 所需的高分辨率时钟
		 * 符号由 rt_time(POSIX 分支)提供,保持符号名一致。 */
		{"mira_win_tick_ns",     "rt_time" RT_OBJ_EXT},
#endif
		/* rt_sched */
		{"mira_go_start0",       "rt_sched" RT_OBJ_EXT},
		{"mira_go_start_fast0",  "rt_sched" RT_OBJ_EXT},
		{"mira_go_join",         "rt_sched" RT_OBJ_EXT},
		{"mira_go_yield",        "rt_sched" RT_OBJ_EXT},
		{"mira_go_wait_all",     "rt_sched" RT_OBJ_EXT},
		/* rt_channel */
		{"mira_channel_new_value",   "rt_channel" RT_OBJ_EXT},
		{"mira_channel_send_value",  "rt_channel" RT_OBJ_EXT},
		{"mira_channel_recv_value",  "rt_channel" RT_OBJ_EXT},
		{"mira_channel_close_value", "rt_channel" RT_OBJ_EXT},
		{"mira_channel_free_value",  "rt_channel" RT_OBJ_EXT},
		{NULL, NULL}
	};

	/* Scan the compiled .obj COFF symbol table for undefined symbols */
	#define MAX_RT_OBJS 16
	const char *objs[MAX_RT_OBJS + 2]; /* program.obj + rt_core.obj + up to MAX_RT_OBJS modules */
	int obj_count = 0;
	int mod_used[MAX_RT_OBJS] = {0}; /* track which modules are already added */
	static const char *mod_names[] = {
		"rt_print" RT_OBJ_EXT, "rt_time" RT_OBJ_EXT, "rt_mem" RT_OBJ_EXT, "rt_string" RT_OBJ_EXT,
		"rt_collection" RT_OBJ_EXT, "rt_file" RT_OBJ_EXT, "rt_math" RT_OBJ_EXT, "rt_error" RT_OBJ_EXT,
		"rt_debug" RT_OBJ_EXT, "rt_io" RT_OBJ_EXT, "rt_struct" RT_OBJ_EXT, "rt_win" RT_OBJ_EXT,
		"rt_sched" RT_OBJ_EXT, "rt_channel" RT_OBJ_EXT,
#ifdef _WIN32
		"rt_chkstk_x86_64" RT_OBJ_EXT,
		NULL
#else
		/* POSIX 运行时附加模块:fiber 协程切换 + 同步原语,
		 * 由 rt_sched/rt_channel 的 POSIX 版实现依赖 */
		"rt_fiber" RT_OBJ_EXT, "rt_sync" RT_OBJ_EXT, NULL
#endif
	};

	/* Always include program obj first */
	objs[obj_count++] = obj_path;

	/* Always include rt_core (provides main + mira_cr + globals) */
	static char rt_core_path[512];
	snprintf(rt_core_path, sizeof(rt_core_path), "%srt_core" RT_OBJ_EXT, rt_dir);
	FILE *rf = fopen(rt_core_path, "rb");
	if (!rf) {
		/* Fallback to legacy monolithic runtime */
		char runtime_path[512];
		size_t ext_len = sizeof(RT_OBJ_EXT) - 1;
		if (ldir_len > ext_len) {
			memcpy(runtime_path, libs_dir, ldir_len - ext_len);
			strcpy(runtime_path + ldir_len - ext_len, "runtime" RT_OBJ_EXT);
		} else {
			strcpy(runtime_path, "runtime" RT_OBJ_EXT);
		}
		objs[obj_count++] = strdup(runtime_path);
		goto do_link;
	}
	fclose(rf);
	objs[obj_count++] = rt_core_path;

	/* Read the program .obj to find undefined symbols */
	{
		FILE *f = fopen(obj_path, "rb");
		if (f) {
#ifdef _WIN32
			/* === COFF 符号扫描(Windows)==={{{ */
			/* COFF header: 2 bytes machine, 2 bytes num_sections,
			   4 bytes timestamp, 4 bytes sym_table_offset, 4 bytes num_symbols */
			unsigned char hdr[20];
			if (fread(hdr, 1, 20, f) == 20) {
				uint16_t section_cnt = *(uint16_t *)&hdr[2];
				uint32_t sym_off = *(uint32_t *)&hdr[8];
				uint32_t sym_cnt = *(uint32_t *)&hdr[12];
				/* String table is right after symbol table */
				uint32_t strtab_off = sym_off + sym_cnt * 18;
				/* An undefined COFF symbol can be a declaration only.  Load a
				 * runtime module solely when at least one section relocation
				 * actually references that symbol.  This prevents low optimization
				 * levels from pulling every declared runtime module into the link. */
				unsigned char *referenced = (unsigned char *)calloc(sym_cnt ? sym_cnt : 1, 1);
				for (uint16_t si = 0; si < section_cnt; ++si) {
					unsigned char sh[40];
					fseek(f, 20L + (long)si * 40L, SEEK_SET);
					if (fread(sh, 1, 40, f) != 40) break;
					uint32_t reloc_off = *(uint32_t *)&sh[24];
					uint16_t reloc_cnt = *(uint16_t *)&sh[32];
					for (uint16_t ri = 0; ri < reloc_cnt; ++ri) {
						unsigned char rel[10];
						fseek(f, (long)reloc_off + (long)ri * 10L, SEEK_SET);
						if (fread(rel, 1, 10, f) != 10) break;
						uint32_t sym_index = *(uint32_t *)&rel[4];
						if (sym_index < sym_cnt) referenced[sym_index] = 1;
					}
				}

				for (uint32_t i = 0; i < sym_cnt; i++) {
					unsigned char sym[18];
					fseek(f, sym_off + i * 18, SEEK_SET);
					if (fread(sym, 1, 18, f) != 18) break;

					uint16_t sec_num = *(uint16_t *)&sym[12];
					uint8_t num_aux = sym[17];

					/* sec_num == 0 means undefined (extern reference) */
					if (sec_num == 0 && referenced[i]) {
						char name[256] = {0};
						if (*(uint32_t *)&sym[0] == 0) {
							/* Long name: offset into string table */
							uint32_t str_off = *(uint32_t *)&sym[4];
							long saved = ftell(f);
							fseek(f, strtab_off + str_off, SEEK_SET);
							int ch, ni = 0;
							while (ni < 255 && (ch = fgetc(f)) > 0) name[ni++] = (char)ch;
							name[ni] = '\0';
							fseek(f, saved, SEEK_SET);
						} else {
							/* Short name: inline 8 bytes */
							memcpy(name, sym, 8);
							name[8] = '\0';
						}

						/* Look up in sym_map */
						for (int s = 0; sym_map[s].sym; s++) {
							if (strcmp(name, sym_map[s].sym) == 0) {
								/* Find module index */
								for (int m = 0; mod_names[m]; m++) {
									if (strcmp(sym_map[s].mod, mod_names[m]) == 0 && !mod_used[m]) {
										mod_used[m] = 1;
										static char mod_paths[MAX_RT_OBJS][512];
										snprintf(mod_paths[m], sizeof(mod_paths[m]), "%s%s", rt_dir, mod_names[m]);
										objs[obj_count++] = mod_paths[m];
										break;
									}
								}
								break;
							}
						}
					}
					i += num_aux; /* skip aux entries */
				}
				free(referenced);
			}
#else
			/* === ELF 符号扫描(Linux)==={{{ */
			/* 读取整个文件,用 Elf64 结构解析符号表与重定位引用关系。
			 * 逻辑与 COFF 版完全对称:找到被 reloc 引用的未定义符号,
			 * 按 sym_map 映射到对应运行时模块。 */
			fseek(f, 0, SEEK_END);
			long elf_fsize = ftell(f);
			fseek(f, 0, SEEK_SET);
			uint8_t *elf_buf = (uint8_t *)malloc(elf_fsize);
			if (elf_buf && fread(elf_buf, 1, elf_fsize, f) == (size_t)elf_fsize &&
			    elf_fsize >= 64 && elf_buf[0] == 0x7f && elf_buf[1] == 'E') {
				/* Elf64_Ehdr 字段(小端读取) */
				uint64_t e_shoff = elf_get_u64_(elf_buf + 40);
				uint16_t e_shnum = elf_get_u16_(elf_buf + 60);
				uint16_t e_shstrndx = elf_get_u16_(elf_buf + 62);

				/* 段头表:文件内偏移未对齐(如 e_shoff=0x66f)时直接指针转换属未对齐访问 UB,
				 * 整体拷贝到对齐缓冲后再访问。 */
				uint64_t shdr_bytes = (uint64_t)e_shnum * sizeof(Elf64_Shdr_);
				Elf64_Shdr_ *shdrs = NULL;
				if (shdr_bytes && e_shoff <= (uint64_t)elf_fsize &&
				    shdr_bytes <= (uint64_t)elf_fsize - e_shoff) {
					shdrs = (Elf64_Shdr_ *)calloc(e_shnum, sizeof(Elf64_Shdr_));
					if (shdrs) memcpy(shdrs, elf_buf + e_shoff, (size_t)shdr_bytes);
				}
				if (shdrs) {
				const char *shstrtab = (const char *)(elf_buf +
					elf_get_u64_((const uint8_t *)&shdrs[e_shstrndx].sh_offset));

				/* 找 .symtab 及其 .strtab */
				int symtab_si = -1, strtab_si = -1;
				for (int si = 1; si < e_shnum; si++) {
					if (shdrs[si].sh_type == 2) { /* SHT_SYMTAB */
						symtab_si = si;
						strtab_si = (int)shdrs[si].sh_link;
						break;
					}
				}

				if (symtab_si >= 0) {
					/* 符号表同样拷贝到对齐缓冲 */
					uint64_t symtab_off2 = elf_get_u64_((const uint8_t *)&shdrs[symtab_si].sh_offset);
					uint64_t symtab_sz2 = elf_get_u64_((const uint8_t *)&shdrs[symtab_si].sh_size);
					uint32_t sym_cnt = (uint32_t)(symtab_sz2 / 24);
					Elf64_Sym_ *syms = (Elf64_Sym_ *)calloc(sym_cnt ? sym_cnt : 1, 24);
					if (syms && symtab_off2 <= (uint64_t)elf_fsize &&
					    symtab_sz2 <= (uint64_t)elf_fsize - symtab_off2)
						memcpy(syms, elf_buf + symtab_off2, (size_t)symtab_sz2);
					if (syms) {
					const char *strtab = (const char *)(elf_buf +
						elf_get_u64_((const uint8_t *)&shdrs[strtab_si].sh_offset));

					/* 引用位图:遍历所有 SHT_RELA 段 */
					unsigned char *referenced = (unsigned char *)calloc(sym_cnt ? sym_cnt : 1, 1);
					for (int si = 1; si < e_shnum; si++) {
						if (shdrs[si].sh_type != 4) continue; /* SHT_RELA */
						/* Rela 表拷贝到对齐缓冲后再访问 */
						uint64_t rela_off2 = elf_get_u64_((const uint8_t *)&shdrs[si].sh_offset);
						uint64_t rela_sz2 = elf_get_u64_((const uint8_t *)&shdrs[si].sh_size);
						uint64_t rc = rela_sz2 / 24;
						if (rc && rela_off2 <= (uint64_t)elf_fsize &&
						    rela_sz2 <= (uint64_t)elf_fsize - rela_off2) {
							Elf64_Rela_ *relas = (Elf64_Rela_ *)malloc((size_t)rela_sz2);
							if (relas) {
								memcpy(relas, elf_buf + rela_off2, (size_t)rela_sz2);
								for (uint64_t r = 0; r < rc; r++) {
									uint32_t sidx = (uint32_t)(elf_get_u64_((const uint8_t *)&relas[r].r_info) >> 32);
									if (sidx < sym_cnt) referenced[sidx] = 1;
								}
								free(relas);
							}
						}
					}

					/* 遍历符号,找被引用的未定义符号 */
					for (uint32_t i = 1; i < sym_cnt; i++) {
						uint16_t shndx = elf_get_u16_((const uint8_t *)&syms[i].st_shndx);
						if (shndx != 0) continue; /* 只看 SHN_UNDEF */
						if (!referenced[i]) continue;
						uint8_t bind = syms[i].st_info >> 4;
						if (bind == 0) continue; /* 跳过 STB_LOCAL */

						uint32_t name_off = elf_get_u32_((const uint8_t *)&syms[i].st_name);
						const char *name = strtab + name_off;
						if (!name[0]) continue;

						/* 查 sym_map */
						for (int s = 0; sym_map[s].sym; s++) {
							if (strcmp(name, sym_map[s].sym) == 0) {
								for (int m = 0; mod_names[m]; m++) {
									if (strcmp(sym_map[s].mod, mod_names[m]) == 0 && !mod_used[m]) {
										mod_used[m] = 1;
										static char mod_paths[MAX_RT_OBJS][512];
										snprintf(mod_paths[m], sizeof(mod_paths[m]), "%s%s", rt_dir, mod_names[m]);
										objs[obj_count++] = mod_paths[m];
										break;
									}
								}
								break;
							}
						}
					}
					free(referenced);
					free(syms);
					}
				}
				free(shdrs);
				}
			}
			free(elf_buf);
#endif
			fclose(f);
		}
	}

	/* Runtime dependency closure (transitive).  A module may depend on another
	 * even when the user program never references its symbols: rt_collection
	 * and the Win helpers allocate through rt_mem, channels park/wake tasks
	 * through the scheduler, and on POSIX the scheduler itself is built on
	 * fibers and sync primitives.  Iterate to a fixpoint so that
	 * rt_channel -> rt_sched -> rt_fiber/rt_sync fully expands. */
	static const struct { const char *mod; const char *dep; } mod_deps[] = {
		{ "rt_collection" RT_OBJ_EXT, "rt_mem" RT_OBJ_EXT },
		{ "rt_win" RT_OBJ_EXT, "rt_mem" RT_OBJ_EXT },
		{ "rt_win" RT_OBJ_EXT, "rt_chkstk_x86_64" RT_OBJ_EXT },
		{ "rt_channel" RT_OBJ_EXT, "rt_sched" RT_OBJ_EXT },
#ifndef _WIN32
		/* POSIX 运行时附加依赖:调度器与通道依赖 fiber 切换和同步原语,
		 * 即使程序本身没有 go/select 表达式也必须链接进来。 */
		{ "rt_sched" RT_OBJ_EXT, "rt_fiber" RT_OBJ_EXT },
		{ "rt_sched" RT_OBJ_EXT, "rt_sync" RT_OBJ_EXT },
		{ "rt_channel" RT_OBJ_EXT, "rt_sync" RT_OBJ_EXT },
#endif
		{ NULL, NULL }
	};
	for (int changed = 1; changed; ) {
		changed = 0;
		for (int m = 0; mod_names[m]; m++) {
			if (!mod_used[m]) continue;
			for (int d = 0; mod_deps[d].mod; d++) {
				if (strcmp(mod_names[m], mod_deps[d].mod) != 0) continue;
				for (int m2 = 0; mod_names[m2]; m2++) {
					if (strcmp(mod_names[m2], mod_deps[d].dep) != 0 || mod_used[m2])
						continue;
					mod_used[m2] = 1;
					/* 注意:objs[] 存的是指针,必须按模块索引用独立缓冲区,
					 * 否则后面的 snprintf 会覆盖前面的路径(此前 rt_fiber
					 * 被 rt_sync 覆盖导致 fiber 符号丢失、链接失败)。 */
					static char dep_paths[MAX_RT_OBJS][512];
					snprintf(dep_paths[m2], sizeof(dep_paths[m2]), "%s%s",
					         rt_dir, mod_names[m2]);
					/* 去重:同一路径(如 rt_sync 经多条依赖边)只加一次 */
					int dup = 0;
					for (int oi = 0; oi < obj_count; oi++)
						if (strcmp(objs[oi], dep_paths[m2]) == 0) { dup = 1; break; }
					if (!dup)
						objs[obj_count++] = dep_paths[m2];
					changed = 1;
				}
			}
		}
	}

do_link:;
	int ret = linker_run(objs, obj_count, exe_path);
	clock_t profile_link = profile_enabled ? clock() : 0;
	if (ret != 0) {
		mira_error_simple(1, "linking failed");
	}
	printf("Done. Run: %s\n", exe_path);
	if (profile_enabled)
		fprintf(stderr, "build-profile compile=%.3f link=%.3f total=%.3f\n",
		        mira_profile_ms(profile_begin, profile_compile),
		        mira_profile_ms(profile_compile, profile_link),
		        mira_profile_ms(profile_begin, profile_link));
}


#define MIRA_VERSION "5.13.4"
#define LINKER_VERSION "1.2.2"

static void print_version(void) {
	printf("mira %s\n", MIRA_VERSION);
	printf("  linker   %s\n", LINKER_VERSION);
#ifdef _WIN32
	printf("  target   x86_64-windows");
#else
	printf("  target   x86_64-linux");
#endif
	if (mira_target_features.avx2) printf("+avx2");
	if (mira_target_features.sse42) printf(" +sse4.2");
	if (mira_target_features.avx) printf(" +avx");
	if (mira_target_features.fma3) printf(" +fma");
	if (mira_target_features.bmi1) printf(" +bmi1");
	if (mira_target_features.bmi2) printf(" +bmi2");
	if (mira_target_features.lzcnt) printf(" +lzcnt");
	if (mira_target_features.popcnt) printf(" +popcnt");
	printf("\n");
	printf("  opt      -O%d\n", mira_opt_level);
}

static void print_usage(void) {
	printf("Mira Programming Language v%s\n\n", MIRA_VERSION);
	printf("Usage:\n");
	printf("  mira <file.mira>             Compile, assemble, link -> .exe\n");
	printf("  mira -S <file.mira> [o.asm]  Compile to assembly only\n");
	printf("  mira -l <a.obj> [b.obj] ...  Link .obj files -> .exe\n");
	printf("  mira -n <name>               Create a new Mira project\n");
	printf("  mira -O0|-O1|-O2|-O3         Set optimization level (default: -O2)\n");
	printf("  mira -mavx2|-mno-avx2         Enable/disable AVX2 target (default: enabled)\n");
	printf("  mira -v, --version           Show version information\n");
	printf("  mira -h, --help              Show this help message\n");
}

/* 鎲嶈悋馉綄锟借雹鑰拷璩㈡啢锟芥綌铻傝潳椁咃拷锟藉潝鍗斤拷鍗濓拷锟介枡锟斤拷绁嗭拷锟借雹楝茶潿稷爟锟斤拷搴忦　烇拷锟界绺戯拷鎷嶇肪锟借€曪拷瀹嬶拷绠忥拷锟姐殮绶ょ攬妯筹拷锟芥妴锟芥拸钀樺啫閵碘姤黏獤鐠婃枟锟界拤锟斤拷锟斤几妞拷鋫块槷鎲撹劑锟借彍韪庢喅妗吤拷缇擄拷鎲℃Ы锟斤拷鎽帮拷锟界爞锟?REPL 鐬堝垹悌炴啰鐞匡拷锟金滐拷锟界﹥锟斤拷鐮嶏拷锟金滃櫒锟借疇锟芥懓皎盎锟芥挊鍦掔笣鐬堬拷锟斤拷涑★拷锟斤拷楹潪鍦堥笜鐠嗭拷鐠夊棯绶わ拷绠忔櫠鎲块灍锟借潽椐侊拷锟藉敵閴勬啋鎬ワ拷铦虫激锟斤拷铦ゆ疆榇★拷鍫掞拷锟界兙锟斤拷皙榿鐓撅拷锟借尝浒荤瀴鍡嗭拷锟藉嚱锟芥埈鍓滐拷锟?*/

int main(int argc, char **argv) {
	init_libs_dir(argv[0]);
	if (!target_detect_native(&mira_target_features)) target_set_baseline(&mira_target_features);
	mira_target_avx2 = mira_target_features.avx2 ? 1 : 0;

	/* 浼樺厛鍏ㄥ眬瑙ｆ瀽 -O 浼樺寲绛夌骇鍙傛暟 */
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-' && argv[i][1] == 'O' && argv[i][2] >= '0' && argv[i][2] <= '3' && argv[i][3] == '\0') {
			mira_opt_level = argv[i][2] - '0';
		}
		if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--dump-asm") == 0) {
			fprintf(stderr, "error: unsupported option '%s'\n", argv[i]);
			return 1;
		}
		if (strcmp(argv[i], "-mavx2") == 0) {
			mira_target_features.avx = true;
			mira_target_features.avx2 = true;
			mira_target_avx2 = 1;
		}
		if (strcmp(argv[i], "-mno-avx2") == 0) {
			mira_target_features.avx2 = false;
			mira_target_avx2 = 0;
		}
		if (strncmp(argv[i], "-march=", 7) == 0) {
			if (!target_apply_march(&mira_target_features, argv[i] + 7)) {
				fprintf(stderr, "error: unknown target '%s'\n", argv[i] + 7);
				return 1;
			}
			mira_target_avx2 = mira_target_features.avx2 ? 1 : 0;
		}
		if (strcmp(argv[i], "--target=linux") == 0 || strcmp(argv[i], "--target=sysv") == 0) {
			mira_target_abi = MIRA_ABI_SYSV;
		}
		if (strcmp(argv[i], "--target=windows") == 0 || strcmp(argv[i], "--target=win64") == 0) {
			mira_target_abi = MIRA_ABI_WIN64;
		}
	}

	if (argc < 2) {
		printf("Usage: mira <file.mira>\n");
		return 1;
	}
	/* 锟斤拷锟斤拷璩婏拷锟斤拷锟界拪瑙佹紗锟斤拷锟界槥锟斤拷鐠嗗垹锟斤拷穑硣锕滐拷锛锋湵锟戒箳锟斤拷穑硣锟介浛璞㈤墑鎲胯櫕鎾堟拸钀勶拷锟藉ⅶ鎻拷濞拷鎾熻嚞稷倸锟介枛楫熸啳黏た鍠勭瀳鈭枼鎾熴殮锟斤拷鑸吉鐬堟锟界拪璩拷锟借锟芥喛鐞滐拷锟斤拷锟介浛绠囩洈鎲垮吀楹拷鎲嶈悇娲╄潪稹伝锟斤拷鍡咃拷锟斤拷锟芥拰鑴蹭腑锟斤拷锟藉柌锟斤拷浜も叀锟斤拷瀣曡潪鍠查础鎴€庯拷锟斤拷锟借澇鍨嶏拷锟斤拷锟介灳锟斤拷锟芥嵍锟戒穑嚎锟?*/
	bool wants_version = false;
	for (int i = 1; i < argc; ++i)
		if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-version") == 0)
			wants_version = true;
	if (wants_version) {
		print_version();
		return 0;
	}
	if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-help") == 0) {
		print_usage();
		return 0;
	}
	/* 锟斤拷锟斤拷璩婏拷锟斤拷锟界拪瑙佹紗锟斤拷锟界槥锟斤拷鐠嗗垹锟斤拷穑硣锕滐拷锛革拷锟芥妴锟芥啹闁椾粈锟芥⒍锟斤拷椁咃拷锟芥壋锟介殹婀旈皧锟斤拷锟斤拷皈籍瑕嬶拷鏀归紦鎲匡几锟斤拷鐓俱▋锟芥锟借澔锟借澊锟藉斁浠€锟借悇锟芥啞锟芥苟锟界礁楦橈拷韪癸拷锟金冿拷锟斤拷閼拷鍚诧拷锟藉敭锟芥喛闆磋澏铦炴毟锟界槰杌诧拷锟藉墫娲佃潩銟撅拷锟戒簷锟斤拷铔旓拷锟芥姌锟芥啳穑偧锟斤拷鎷欙拷锟介枛锟斤拷鏂囧Ε鎲¤帋锟斤拷闉撅拷锟斤拷锟斤拷鏇夛拷鐨濇锟? mira -n <name> */
	if (strcmp(argv[1], "-n") == 0 || strcmp(argv[1], "--new") == 0) {
		const char *proj = argc > 2 ? argv[2] : "project";
		char cmd[1024];
		snprintf(cmd, sizeof(cmd), "mkdir \"%s\" 2>nul", proj);
		system(cmd);
		snprintf(cmd, sizeof(cmd), "mkdir \"%s\\config\" 2>nul", proj);
		system(cmd);
		/* config/project.json */
		char cfg_path[512];
		snprintf(cfg_path, sizeof(cfg_path), "%s\\config\\project.json", proj);
		FILE *cfg = fopen(cfg_path, "w");
		if (cfg) {
			char exe_path[512] = {0};
			const char *lIR = strrchr(argv[0], '\\');
			if (!lIR) lIR = strrchr(argv[0], '/');
			if (lIR) {
				size_t dlen = (size_t)(lIR - argv[0]);
				memcpy(exe_path, argv[0], dlen);
				exe_path[dlen] = '\0';
			} else {
				strcpy(exe_path, ".");
			}
			char escaped[1024];
			size_t ei = 0;
			for (size_t i = 0; exe_path[i] && ei < sizeof(escaped) - 2; i++) {
				if (exe_path[i] == '\\') escaped[ei++] = '\\';
				escaped[ei++] = exe_path[i];
			}
			escaped[ei] = '\0';
			fprintf(cfg, "{\n  \"name\": \"%s\",\n  \"version\": \"0.1.0\",\n  \"platform\": \"win64\",\n  \"entry\": \"main.mira\",\n  \"miraPath\": \"%s\"\n}\n", proj, escaped);
			fclose(cfg);
		}
		/* main.mira */
		char main_path[512];
		snprintf(main_path, sizeof(main_path), "%s\\main.mira", proj);
		FILE *mf = fopen(main_path, "w");
		if (mf) {
			fprintf(mf, "# %s\n\ndouble: { n } n 2 *\n\nmain: {\n  \"Hello from %s!\" print\n  7 double print\n  \"Ready to code!\" print\n}\n", proj, proj);
			fclose(mf);
		}
		/* README.md */
		char readme_path[512];
		snprintf(readme_path, sizeof(readme_path), "%s\\README.md", proj);
		FILE *rf = fopen(readme_path, "w");
		if (rf) {
			fprintf(rf, "# %s\n\nA Mira project.\n\n## Build & Run\n\n```\nmira main.mira\nmain.exe\n```\n", proj);
			fclose(rf);
		}
		printf("Created project: %s\n", proj);
		printf("  cd %s && mira main.mira\n", proj);
		return 0;
	}
	/* mira -S <file> [out.asm]  锟斤拷锟斤拷璩婏拷锟斤拷锟界拪瑙佹紗锟斤拷锟界槥锟斤拷鐠嗗垹锟斤拷鑿燂拷锟介锟斤拷闉卞儺锟介绺戯拷鑺革拷銡氾拷鐠嗭拷锟斤拷鏆癸拷锟界爫悃ユ挊椐侊拷鍎泬缃侊拷锟斤拷锟金冿拷鐦ㄥ椀锟芥啞鐑愶拷閳█ｄ腑锟藉棩缈旇潖锟斤拷鏍硷拷锟界敱锟斤拷铦伙拷馉摷锟借埆锟界拡锟芥挓銡氾拷锟斤拷锟斤拷锟借澑锟芥姃锟金烇拷锟金牥存伃锟借垁锟斤拷瀵ヰ、胯澐锟斤拷锟芥江榘婏拷锟金８戯拷鐮傦拷锟借┄娲拷璩婏拷鎲獧涳拷鎲掕垚锟芥啰绠囷拷锟借喊锟界瀳妗冪挍鎾熺吘鎽氾拷锟斤拷?asm */
	if (strcmp(argv[1], "-S") == 0) {
		if (argc < 3) { fprintf(stderr, "mira -S: no input file\n"); return 1; }
		const char *in = argv[2];
		const char *out = argc > 3 ? argv[3] : "out.ir";
		compile_file_ir_dump(in, out);
		printf("IR dumped to %s\n", out);
		return 0;
	}
	/* mira -l <obj1> [obj2 ...] [-o output.exe]  璩婃綌铻傝潳瀛垫埈鑺ｇ挡鍓栫緟鍢ヰ牸荤瀼闉夎嚞濡ュ幇楦樼拝绠葛ぞ歌姺愦撴疆黏剟鎾椼殮琛€鎲块崷鎾ｈ几?*/
	if (strcmp(argv[1], "-l") == 0) {
		if (argc < 3) { fprintf(stderr, "mira -l: no input files\n"); return 1; }
		const char *out_exe = NULL;
		const char *obj_files[64];
		int obj_count = 0;
		for (int i = 2; i < argc; i++) {
			if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
				out_exe = argv[++i];
			} else {
				obj_files[obj_count++] = argv[i];
			}
		}
		if (!out_exe) out_exe = "a.exe";
		int ret = linker_run(obj_files, obj_count, out_exe);
		if (ret == 0) printf("Linked -> %s\n", out_exe);
		return ret;
	}

	/* Find lIR non-flag argument for full_build */
	int file_arg = 1;
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			file_arg = i;
		}
	}
	/* mira <file.mira> */
	full_build(argv[file_arg]);
	return 0;
}

