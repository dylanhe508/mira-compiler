/* ssa_dom.c �?支配�?(Dominator Tree) 与支配边�?(Dominance Frontier) 计算
 *
 * 根据 Lengauer-Tarjan 算法或简单迭代法计算支配树�?
 * 为减低复杂性，本实现采用简单易懂的迭代定点算法�?
 */
#include "ir_ssa.h"
#include <stdlib.h>
#include <stdbool.h>

/* =============== 支配信息计算 (Dominator Analysis) =============== */

/* 
 * 集合表示方案：此处为了简单，可以�?bool 数组来表�?Block 的集合�?
 * 假设一�?Function 里最多只�?block_count �?BasicBlock�?
 */

static void dom_intersect_sets(bool *dst, bool *src, int count) {
	for(int i=0; i<count; i++) {
		if(!src[i]) dst[i] = false;
	}
}

static void compute_dominators(SsaFunction *func) {
	int n = func->block_count;
	if(n == 0) return;

	bool **dom_sets = malloc(n * sizeof(bool*));
	for(int i=0; i<n; i++) {
		dom_sets[i] = malloc(n * sizeof(bool));
		for(int j=0; j<n; j++) {
			dom_sets[i][j] = true; // 初始假设全集 (除入口块)
		}
	}

	/* 入口块仅支配自己 */
	SsaBasicBlock *entry = func->entry_block;
	for(int j=0; j<n; j++) dom_sets[entry->id][j] = false;
	dom_sets[entry->id][entry->id] = true;

	/* 迭代求精 DOM(n) = {n} U (n 的所有前�?p �?intersect DOM(p)) */
	bool changed = true;
	bool *temp_set = malloc(n * sizeof(bool));

	while(changed) {
		changed = false;
		for(int i=0; i<n; i++) {
			SsaBasicBlock *b = func->blocks[i];
			if(b == entry) continue;

			for(int j=0; j<n; j++) temp_set[j] = true;

			bool has_pred = false;
			for(int p=0; p<b->pred_count; p++) {
				SsaBasicBlock *pred = b->preds[p];
				dom_intersect_sets(temp_set, dom_sets[pred->id], n);
				has_pred = true;
			}
			
			if(!has_pred) {
				for(int j=0; j<n; j++) temp_set[j] = false;
			}

			// DOM(b) = {b} U intersect
			temp_set[b->id] = true;

			// Check if changed
			for(int j=0; j<n; j++) {
				if(dom_sets[b->id][j] != temp_set[j]) {
					dom_sets[b->id][j] = temp_set[j];
					changed = true;
				}
			}
		}
	}

	/* 计算立即支配�?(Immediate Dominator - idom)
	 * b �?idom �?DOM(b) 中除�?b 自己以外的结点，并且�?DOM(b) 中其他所有结点支配�?
	 * 即在支配者路径上�?b 最近的那个�?
	 */
	for(int i=0; i<n; i++) {
		SsaBasicBlock *b = func->blocks[i];
		if(b == entry) {
			b->idom = NULL;
			continue;
		}
		
		SsaBasicBlock *candidate = NULL;
		for(int j=0; j<n; j++) {
			if(j == b->id && dom_sets[b->id][j]) continue; 
			if(dom_sets[b->id][j]) { // j 支配 b
				bool is_closest = true;
				for(int k=0; k<n; k++) {
					if(k == b->id || k == j) continue;
					if(dom_sets[b->id][k] && dom_sets[k][j]) {
						// k 也支�?b，且 j 支配 k，说�?k �?j 更近
						is_closest = false;
						break;
					}
				}
				if(is_closest) {
					candidate = func->blocks[j];
					break;
				}
			}
		}
		b->idom = candidate;

		// 挂载到父 DOM �?children 列表�?
		if(candidate) {
			if(candidate->dom_child_count >= candidate->dom_child_cap) {
				candidate->dom_child_cap = candidate->dom_child_cap ? candidate->dom_child_cap*2 : 4;
				candidate->dom_children = realloc(candidate->dom_children, candidate->dom_child_cap * sizeof(SsaBasicBlock*));
			}
			candidate->dom_children[candidate->dom_child_count++] = b;
		}
	}

	for(int i=0; i<n; i++) free(dom_sets[i]);
	free(dom_sets);
	free(temp_set);
#if 0
	printf("Dominator Tree for function %s:\n", func->name);
	for(int i=0; i<n; i++) {
		SsaBasicBlock *b = func->blocks[i];
		printf("Block %s (id: %d) idom: %s\n", b->name?b->name:"", b->id, b->idom && b->idom->name ? b->idom->name : "none");
	}
#endif
}

/* =============== 支配边界 (Dominance Frontiers) =============== */
/*
 * 支配边界�?
 * 若结点的有多个前驱（汇聚点），对于每一个前�?p，顺着 idom(p) 一直往上爬�?
 * 直到遇到支配该汇聚点的块（或其本身），沿途所有的块的支配边界都包含该汇聚点�?
 */
static void compute_dominance_frontiers(SsaFunction *func) {
	int n = func->block_count;
	for(int i=0; i<n; i++) {
		SsaBasicBlock *b = func->blocks[i];
		if(b->pred_count >= 2) {
			for(int p=0; p<b->pred_count; p++) {
				SsaBasicBlock *runner = b->preds[p];
				while(runner != b->idom) {
					// Add b to runner's DF
					bool found = false;
					for(int d=0; d<runner->df_count; d++) {
						if(runner->df[d] == b) { found = true; break; }
					}
					if(!found) {
						if(runner->df_count >= runner->df_cap) {
							runner->df_cap = runner->df_cap ? runner->df_cap*2 : 4;
							runner->df = realloc(runner->df, runner->df_cap * sizeof(SsaBasicBlock*));
						}
						runner->df[runner->df_count++] = b;
					}
					runner = runner->idom;
					if(!runner) break; // 防止死循�?
				}
			}
		}
	}
}

/* 暴露出主计算函数 */
void ssa_compute_dom_info(SsaFunction *func) {
	if(!func) return;
	for(int i=0; i<func->block_count; i++) {
		SsaBasicBlock *b = func->blocks[i];
		b->idom = NULL;
		free(b->dom_children);
		b->dom_children = NULL;
		b->dom_child_count = 0;
		b->dom_child_cap = 0;
		free(b->df);
		b->df = NULL;
		b->df_count = 0;
		b->df_cap = 0;
	}
	compute_dominators(func);
	compute_dominance_frontiers(func);
}
