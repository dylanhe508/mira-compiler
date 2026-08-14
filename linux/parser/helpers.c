/* parser/helpers.c 閿?瀹搞儱鍙块崙鑺ユ殶 */
#include "parser.h"
/* 缂佸顭峰▍搴ㄦ煣閹规劑鈧啴寮甸埀顒勫触?n 濞戞搩浜风槐婵囨交閺傛寧绀€閻炴凹鍋嗕簺闂傚嫨鍊濋崕鎾礆閸℃瑦鐣卞鑸电暘閳ь剙鍊界€氥垺绋夊鍫濆枙 n 濞戞搩浜滈崹顖涙交閺傛寧绀€ NULL 濞戞挻鏌х粭澶愬绩閻熸澘袟??*/
static IrNode *pop_lIR_n(IrNode **head, IrNode **tail, int n) {
	if (!*head || n <= 0) return NULL;
	int len = 0;
	for (IrNode *p = *head; p; p = p->next) len++;
	if (len < n) return NULL;
	IrNode *removed;
	if (len == n) {
		removed = *head;
		*head = NULL;
		*tail = NULL;
	} else {
		IrNode *cut = *head;
		for (int i = 0; i < len - n - 1; i++) cut = cut->next;
		removed = cut->next;
		cut->next = NULL;
		*tail = cut;
	}
	return removed;
}

/* 婵炲濮撮柊锝夊箺閼测晜鍋橀柕濞垮€涢崢顒勬煕濞嗘劗澧柛銈庡弮瀵偆鈧稒蓱濞堝爼鏌涙繝鍕┾偓瀣濠靛洨顩?IR_INT 閿?IR_CONST(int) */
static int64_t IR_to_int(IrNode *o, Program *prog) {
	if (o->kind == IR_INT) return o->u.i;
	if (o->kind == IR_CONST) {
		int s = o->u.const_slot;
		if (prog->const_kinds[s] == CONST_INT) return prog->const_ints[s];
	}
	return 0;
}

/* 闂侀潻璐熼崝搴ㄥ煝閸忓吋鍎熼煫鍥ф唉閸橆剟鏌ｈ濡叉帡顢欓崶銊р枖闁逞屽墯缁嬪顢旈崟顐ょМ闂備焦褰冩蹇曟濠靛洦浜ら柡鍌涘缁€鈧┑鈥冲级閸ㄦ繄绱為崨顔锯枖閻庯綆鍋嗛敓?*/
int prog_add_var(Program *prog, char *name, size_t len) {
	for (int i = 0; i < prog->var_count; i++)
		if (prog->var_scopes[i] == current_var_scope &&
		    prog->var_lens[i] == len && memcmp(prog->var_names[i], name, len) == 0)
			return i;
	if (prog->var_count >= prog->var_cap) {
		int old_cap = prog->var_cap;
		prog->var_cap = prog->var_cap ? prog->var_cap * 2 : 8;
		prog->var_names = realloc(prog->var_names, (size_t)prog->var_cap * sizeof(char *));
		prog->var_lens = realloc(prog->var_lens, (size_t)prog->var_cap * sizeof(size_t));
		prog->var_scopes = realloc(prog->var_scopes, (size_t)prog->var_cap * sizeof(int));
		prog->var_structs = realloc(prog->var_structs, (size_t)prog->var_cap * sizeof(StructDef *));
		prog->var_mutable = realloc(prog->var_mutable, (size_t)prog->var_cap);
		memset(prog->var_structs + old_cap, 0, (size_t)(prog->var_cap - old_cap) * sizeof(StructDef *));
		memset(prog->var_mutable + old_cap, 0, (size_t)(prog->var_cap - old_cap));
	}
	prog->var_names[prog->var_count] = name;
	prog->var_lens[prog->var_count] = len;
	prog->var_scopes[prog->var_count] = current_var_scope;
	return prog->var_count++;
}

static int prog_new_var_scope(Program *prog, int parent) {
	if (prog->scope_count == 0) {
		prog->scope_cap = 8;
		prog->scope_parents = malloc((size_t)prog->scope_cap * sizeof(int));
		prog->scope_parents[0] = -1;
		prog->scope_count = 1;
	}
	if (prog->scope_count >= prog->scope_cap) {
		prog->scope_cap *= 2;
		prog->scope_parents = realloc(
			prog->scope_parents, (size_t)prog->scope_cap * sizeof(int));
	}
	int scope = prog->scope_count++;
	prog->scope_parents[scope] = parent;
	return scope;
}

static void prog_set_var_struct(Program *prog, int slot, StructDef *sd, bool is_mutable) {
	if (slot < 0 || slot >= prog->var_count) return;
	prog->var_structs[slot] = sd;
	prog->var_mutable[slot] = is_mutable ? 1 : 0;
}

static int prog_var_slot(Program *prog, const char *name, size_t len) {
	int scope = current_var_scope;
	for (;;) {
		for (int i = prog->var_count - 1; i >= 0; i--)
			if (prog->var_scopes[i] == scope &&
			    prog->var_lens[i] == len && memcmp(prog->var_names[i], name, len) == 0)
				return i;
		if (scope == 0 || scope < 0 || scope >= prog->scope_count) break;
		scope = prog->scope_parents[scope];
	}
	return -1;
}

/* 缂傚倷鐒﹂幐濠氭倵椤栨稒濯撮柟鎯х畭閿?*/
static int prog_add_struct(Program *prog, char *name, size_t name_len, char **fields, size_t *field_lens, int field_count) {
	if (prog->struct_count >= prog->struct_cap) {
		prog->struct_cap = prog->struct_cap ? prog->struct_cap * 2 : 8;
		prog->structs = realloc(prog->structs, (size_t)prog->struct_cap * sizeof(StructDef));
	}
	int idx = prog->struct_count++;
	prog->structs[idx].name = name;
	prog->structs[idx].name_len = name_len;
	prog->structs[idx].field_names = fields;
	prog->structs[idx].field_lens = field_lens;
	prog->structs[idx].field_count = field_count;
	return idx;
}

static StructDef *prog_find_struct(Program *prog, const char *name, size_t len) {
	for (int i = 0; i < prog->struct_count; i++)
		if (prog->structs[i].name_len == len && memcmp(prog->structs[i].name, name, len) == 0)
			return &prog->structs[i];
	return NULL;
}

static int prog_field_offset(StructDef *sd, const char *fname, size_t flen) {
	for (int i = 0; i < sd->field_count; i++)
		if (sd->field_lens[i] == flen && memcmp(sd->field_names[i], fname, flen) == 0)
			return i * 8;  /* 濠殿噯绲界换瀣煂濠婂應鍋撳☉娆樻畷閿?8 闁诲孩绋掗〃澶嬩繆?*/
	return -1;
}

static MethodDef *prog_find_method(Program *prog, StructDef *owner, const char *name, size_t len) {
	for (MethodDef *m = prog->methods; m; m = m->next)
		if (m->owner_len == owner->name_len && memcmp(m->owner, owner->name, owner->name_len) == 0 &&
		    m->name_len == len && memcmp(m->name, name, len) == 0)
			return m;
	return NULL;
}

static MethodDef *prog_add_method(Program *prog, StructDef *owner, char *name, size_t len,
	char *qualified_name, size_t qualified_len, bool mut_self) {
	if (prog_find_method(prog, owner, name, len))
		mira_error(comp->src, comp->filename, lexer_cur()->line, lexer_cur()->col, 1,
			"duplicate method '%.*s.%.*s'", (int)owner->name_len, owner->name, (int)len, name);
	MethodDef *m = arena_alloc(&prog->ir_arena, sizeof(*m));
	memset(m, 0, sizeof(*m));
	m->owner = owner->name; m->owner_len = owner->name_len;
	m->name = name; m->name_len = len;
	m->qualified_name = qualified_name; m->qualified_name_len = qualified_len;
	m->mut_self = mut_self;
	m->next = prog->methods; prog->methods = m;
	return m;
}

/* 閻㈩垱鎮傞崳铏规偘?*/
static int prog_add_const(Program *prog, char *name, size_t len, ConstKind k, int64_t vi, double vd, char *vs, size_t vslen) {
	for (int i = 0; i < prog->const_count; i++)
		if (prog->const_lens[i] == len && memcmp(prog->const_names[i], name, len) == 0)
			return i;
	if (prog->const_count >= prog->const_cap) {
		prog->const_cap = prog->const_cap ? prog->const_cap * 2 : 8;
		prog->const_names = realloc(prog->const_names, (size_t)prog->const_cap * sizeof(char *));
		prog->const_lens = realloc(prog->const_lens, (size_t)prog->const_cap * sizeof(size_t));
		prog->const_kinds = realloc(prog->const_kinds, (size_t)prog->const_cap * sizeof(ConstKind));
		prog->const_ints = realloc(prog->const_ints, (size_t)prog->const_cap * sizeof(int64_t));
		prog->const_doubles = realloc(prog->const_doubles, (size_t)prog->const_cap * sizeof(double));
		prog->const_strs = realloc(prog->const_strs, (size_t)prog->const_cap * sizeof(char *));
		prog->const_str_lens = realloc(prog->const_str_lens, (size_t)prog->const_cap * sizeof(size_t));
	}
	int i = prog->const_count++;
	prog->const_names[i] = name;
	prog->const_lens[i] = len;
	prog->const_kinds[i] = k;
	prog->const_ints[i] = vi;
	prog->const_doubles[i] = vd;
	prog->const_strs[i] = vs;
	prog->const_str_lens[i] = vslen;
	return i;
}

static int prog_const_slot(Program *prog, const char *name, size_t len) {
	for (int i = 0; i < prog->const_count; i++)
		if (prog->const_lens[i] == len && memcmp(prog->const_names[i], name, len) == 0)
			return i;
	return -1;
}

static IrNode *new_ir(IrKind k) {
	IrNode *o = arena_alloc(&comp->prog->ir_arena, sizeof(IrNode));
	memset(o, 0, sizeof(IrNode));
	o->kind = k;
	return o;
}

/* 闂佸搫顑呯€氫即鍩€?value_op -> var_addr -> ! 闂佹眹鍔岀€氫即骞楁總鍛婃櫖鐎光偓閳ь剟寮妶鍡欘洸?x: 123 / y: "hello" */
static IrNode *make_assign_chain(Program *prog, char *name, size_t len, IrNode *value_op) {
	int slot = prog_add_var(prog, name, len);
	IrNode *var_op = new_ir(IR_VAR);
	var_op->u.var_slot = slot;
	IrNode *store_op = new_ir(IR_WORD);
	store_op->u.word.name = "!";  /* 闂佸搫绉撮悧鍡欐閿熺姵鏅慨姗嗗厴閿?闂侀潻闄勫妯侯焽?! */
	store_op->u.word.len = 1;
	value_op->next = var_op;
	var_op->next = store_op;
	return value_op;
}

/* 濡偓閺屻儲妲搁崥锔芥Ц Mira 閸愬懐鐤嗛幙宥勭稊鐠囧稄绱欐稉宥呯安鐞氼偄缍嬫担婊冩儕閻滎垰褰夐柌蹇撴倳閿?*/
static int is_mira_builtin(const char *name, size_t len) {
	if (len == 1) {
		char c = name[0];
		if (c == '.' || c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
		    c == '@' || c == '!' || c == '>' || c == '<' || c == '=' || c == '&' ||
		    c == '|' || c == '^' || c == '~') return 1;
	}
	static const char *builtins[] = {
		"print", "println", "dup", "drop", "swap", "over", "rot",
		"and", "or", "not", "mod", "eq", "ne", "lt", "gt", "le", "ge",
		"emit", "cr", "str-concat", "to-str", "str-len", "str-eq",
		"list-new", "list-get", "list-set", "list-push", "list-len",
		"int->str", "str->int", "msgbox", "sleep", "input", "exit",
		"if", "while", "loop", "break", "continue", "return",
		"true", "false", "try", "catch", "throw", "assert",
		"lwmgl-init", "lwmgl-clear", "lwmgl-color", "lwmgl-rect",
		"lwmgl-swap", "lwmgl-sleep", "lwmgl-close",
		NULL
	};
	for (int i = 0; builtins[i]; i++) {
		size_t blen = strlen(builtins[i]);
		if (blen == len && memcmp(name, builtins[i], len) == 0) return 1;
	}
	return 0;
}

/* 閻熸瑱绲鹃悗浠嬪础閺囩喐钂嬮柨娑欑閺嗭綁寮懜顑藉亾娴ｇ鐧侀柣鎰畭閳ь兛绀侀悺褏绮敂鑳洬闁靛棔娴囬惁婵嬪Υ娴ｇ缍侀梺鎻掔箞閳ь兛绀侀悥鍫曟煂韫囧鍋撴担鍛婂仴闁靛棔绀侀崹顏嗘偘閵娿儳鎽熼梻鍫涘灲閸ｆ椽濡存担鐟扮仐 if */
static int get_op_precedence(const char *IrNode) {
	if (strcmp(IrNode, "neg") == 0) return 8;
	if (strcmp(IrNode, "*") == 0 || strcmp(IrNode, "/") == 0 || strcmp(IrNode, "%") == 0) return 7;
	if (strcmp(IrNode, "+") == 0 || strcmp(IrNode, "-") == 0) return 6;
	if (strcmp(IrNode, "<<") == 0 || strcmp(IrNode, ">>") == 0) return 5;
	if (strcmp(IrNode, "<") == 0 || strcmp(IrNode, ">") == 0 || strcmp(IrNode, "<=") == 0 || strcmp(IrNode, ">=") == 0 || strcmp(IrNode, "==") == 0 || strcmp(IrNode, "!=") == 0) return 4;
	if (strcmp(IrNode, "&") == 0) return 3;
	if (strcmp(IrNode, "^") == 0) return 2;
	if (strcmp(IrNode, "|") == 0) return 1;
	return 0; // Not an infix operator
}
