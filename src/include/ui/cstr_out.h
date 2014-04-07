/*
 * cstr_out.h
 *
 *  Created on: 2014-04-07
 *      Author: jmate
 */

#ifndef CSTR_OUT_H_
#define CSTR_OUT_H_

#include "nodes/relation.h"


extern int
cstr_rte(char* buff, int pos, int max, int i, const RangeTblEntry *rte);

extern int
cstr_relids(char* buff, int pos, int max, Relids relids);

extern int
cstr_relids_to_rte(char* buff, int pos, int max, List* rtable, Relids relids);


extern int cstr_expr(char* buff, int pos, int max, const Node *expr, const List *rtable);

extern int
cstr_restrictclauses(char* buff, int pos, int max, PlannerInfo *root, List *clauses);


extern int
cstr_pathkeys(char* buff, int pos, int max, const List *pathkeys, const List *rtable);

extern int
cstr_pathnode(char* buff, int pos, int max, PlannerInfo *root, Path *path);

extern int
cstr_rel(char* buff, int pos, int max, PlannerInfo *root, RelOptInfo *rel);

#endif /* CSTR_OUT_H_ */
