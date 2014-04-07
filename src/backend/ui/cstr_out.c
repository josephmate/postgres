/*
 * cstr_out.c
 *
 *  Created on: 2014-04-07
 *      Author: jmate
 */

// standard libraries
#include <stdio.h>

// postgres libraries
#include "postgres.h"
#include "nodes/print.h"
#include "miscadmin.h"
#include "optimizer/cost.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/placeholder.h"
#include "optimizer/planmain.h"
#include "optimizer/tlist.h"
#include "utils/selfuncs.h"
#include "nodes/pg_list.h"
#include "parser/parsetree.h"
#include "utils/lsyscache.h"
#include "optimizer/clauses.h"

#include "ui/cstr_out.h"



int
cstr_rte(char* buff, int pos, int max, int i, const RangeTblEntry *rte)
{
	switch (rte->rtekind) {
	case RTE_RELATION:
		pos += snprintf(buff+pos, max-pos, "%d\t%s\t%u\t%c", i, rte->eref->aliasname, rte->relid, rte->relkind);
		break;
	case RTE_SUBQUERY:
		pos += snprintf(buff+pos, max-pos, "%d\t%s\t[subquery]", i, rte->eref->aliasname);
		break;
	case RTE_JOIN:
		pos += snprintf(buff+pos, max-pos, "%d\t%s\t[join]", i, rte->eref->aliasname);
		break;
	case RTE_FUNCTION:
		pos += snprintf(buff+pos, max-pos, "%d\t%s\t[rangefunction]", i, rte->eref->aliasname);
		break;
	case RTE_VALUES:
		pos += snprintf(buff+pos, max-pos, "%d\t%s\t[values list]", i, rte->eref->aliasname);
		break;
	case RTE_CTE:
		pos += snprintf(buff+pos, max-pos, "%d\t%s\t[cte]", i, rte->eref->aliasname);
		break;
	default:
		pos += snprintf(buff+pos, max-pos, "%d\t%s\t[unknown rtekind]", i, rte->eref->aliasname);
		break;
	}

	pos += snprintf(buff+pos, max-pos, "\t%s\t%s\n", (rte->inh ? "inh" : ""),
			(rte->inFromCl ? "inFromCl" : ""));

	return pos;
}

int
cstr_relids(char* buff, int pos, int max, Relids relids)
{
	Relids		tmprelids;
	int			x;
	bool		first = true;

	tmprelids = bms_copy(relids);
	while ((x = bms_first_member(tmprelids)) >= 0)
	{
		if (!first) {
			pos += snprintf(buff+pos, max-pos, " ");
		}
		pos += snprintf(buff+pos, max-pos, "%d", x);
		first = false;
	}
	bms_free(tmprelids);

	return pos;
}

int
cstr_relids_to_rte(char* buff, int pos, int max, List* rtable, Relids relids)
{
	Relids		tmprelids;
	int			x;

	tmprelids = bms_copy(relids);
	while ((x = bms_first_member(tmprelids)) >= 0)
	{
		RangeTblEntry * rte;
		rte = rt_fetch(x, rtable);
		pos = cstr_rte(buff, pos, max, x, rte);
	}
	bms_free(tmprelids);

	return pos;
}

int cstr_expr(char* buff, int pos, int max, const Node *expr, const List *rtable)
{
	if (expr == NULL)
	{
		pos += snprintf(buff+pos, max-pos, "<>");
		return pos;
	}

	if (IsA(expr, Var))
	{
		const Var  *var = (const Var *) expr;
		char	   *relname,
				   *attname;

		switch (var->varno)
		{
			case INNER_VAR:
				relname = "INNER";
				attname = "?";
				break;
			case OUTER_VAR:
				relname = "OUTER";
				attname = "?";
				break;
			case INDEX_VAR:
				relname = "INDEX";
				attname = "?";
				break;
			default:
				{
					RangeTblEntry *rte;

					Assert(var->varno > 0 &&
						   (int) var->varno <= list_length(rtable));
					rte = rt_fetch(var->varno, rtable);
					relname = rte->eref->aliasname;
					attname = get_rte_attribute_name(rte, var->varattno);
				}
				break;
		}
		pos += snprintf(buff+pos, max-pos, "%s.%s", relname, attname);
	}
	else if (IsA(expr, Const))
	{
		const Const *c = (const Const *) expr;
		Oid			typoutput;
		bool		typIsVarlena;
		char	   *outputstr;

		if (c->constisnull)
		{
			pos += snprintf(buff+pos, max-pos, "NULL");
			return pos;
		}

		getTypeOutputInfo(c->consttype,
						  &typoutput, &typIsVarlena);

		outputstr = OidOutputFunctionCall(typoutput, c->constvalue);
		pos += snprintf(buff+pos, max-pos, "%s", outputstr);
		pfree(outputstr);
	}
	else if (IsA(expr, OpExpr))
	{
		const OpExpr *e = (const OpExpr *) expr;
		char	   *opname;

		opname = get_opname(e->opno);
		if (list_length(e->args) > 1)
		{
			pos = cstr_expr(buff,  pos,  max, get_leftop((const Expr *) e), rtable);

			pos += snprintf(buff+pos, max-pos, " %s ", ((opname != NULL) ? opname : "(invalid operator)"));
			pos = cstr_expr(buff,  pos,  max, get_rightop((const Expr *) e), rtable);
		}
		else
		{
			/* we print prefix and postfix ops the same... */
			pos += snprintf(buff+pos, max-pos, "%s ", ((opname != NULL) ? opname : "(invalid operator)"));
			pos = cstr_expr(buff,  pos,  max, get_leftop((const Expr *) e), rtable);
		}
	}
	else if (IsA(expr, FuncExpr))
	{
		const FuncExpr *e = (const FuncExpr *) expr;
		char	   *funcname;
		ListCell   *l;

		funcname = get_func_name(e->funcid);

		pos += snprintf(buff+pos, max-pos, "%s(", ((funcname != NULL) ? funcname : "(invalid function)"));
		foreach(l, e->args)
		{
			pos = cstr_expr(buff,  pos,  max, lfirst(l), rtable);
			if (lnext(l))
				pos += snprintf(buff+pos, max-pos, ",");
		}
		pos += snprintf(buff+pos, max-pos, ")");
	}
	else
		pos += snprintf(buff+pos, max-pos, "unknown expr");

	return pos;
}

int
cstr_restrictclauses(char* buff, int pos, int max, PlannerInfo *root, List *clauses)
{
	ListCell   *l;

	foreach(l, clauses)
	{
		RestrictInfo *c = lfirst(l);

		pos = cstr_expr(buff, pos, max, (Node *) c->clause, root->parse->rtable);
		if (lnext(l))
			pos += snprintf(buff+pos, max-pos, ", ");
	}
	return pos;
}


int
cstr_pathkeys(char* buff, int pos, int max, const List *pathkeys, const List *rtable)
{
	const ListCell *i;

	pos += snprintf(buff+pos, max-pos, "(");
	foreach(i, pathkeys)
	{
		PathKey    *pathkey = (PathKey *) lfirst(i);
		EquivalenceClass *eclass;
		ListCell   *k;
		bool		first = true;

		eclass = pathkey->pk_eclass;
		/* chase up, in case pathkey is non-canonical */
		while (eclass->ec_merged)
			eclass = eclass->ec_merged;

		pos += snprintf(buff+pos, max-pos, "(");
		foreach(k, eclass->ec_members)
		{
			EquivalenceMember *mem = (EquivalenceMember *) lfirst(k);

			if (first)
				first = false;
			else
				pos += snprintf(buff+pos, max-pos, ", ");
			pos = cstr_expr(buff,  pos,  max, (Node *) mem->em_expr, rtable);
		}
		pos += snprintf(buff+pos, max-pos, ")");
		if (lnext(i))
			pos += snprintf(buff+pos, max-pos, ", ");
	}
	pos += snprintf(buff+pos, max-pos, ")\n");

	return pos;
}

int
cstr_pathnode(char* buff, int pos, int max, PlannerInfo *root, Path *path)
{
	const char *ptype;
	bool		join = false;

	switch (nodeTag(path))
	{
		case T_Path:
			ptype = "SeqScan";
			break;
		case T_IndexPath:
			ptype = "IdxScan";
			break;
		case T_BitmapHeapPath:
			ptype = "BitmapHeapScan";
			break;
		case T_BitmapAndPath:
			ptype = "BitmapAndPath";
			break;
		case T_BitmapOrPath:
			ptype = "BitmapOrPath";
			break;
		case T_TidPath:
			ptype = "TidScan";
			break;
		case T_ForeignPath:
			ptype = "ForeignScan";
			break;
		case T_AppendPath:
			ptype = "Append";
			break;
		case T_MergeAppendPath:
			ptype = "MergeAppend";
			break;
		case T_ResultPath:
			ptype = "Result";
			break;
		case T_MaterialPath:
			ptype = "Material";
			break;
		case T_UniquePath:
			ptype = "Unique";
			break;
		case T_NestPath:
			ptype = "NestLoop";
			join = true;
			break;
		case T_MergePath:
			ptype = "MergeJoin";
			join = true;
			break;
		case T_HashPath:
			ptype = "HashJoin";
			join = true;
			break;
		default:
			ptype = "???Path";
			break;
	}
	pos += snprintf(buff+pos, max-pos, "%s", ptype);

	if (path->parent)
	{
		pos += snprintf(buff+pos, max-pos, "(");
		pos = cstr_relids(buff,  pos,  max, path->parent->relids);
		pos += snprintf(buff+pos, max-pos, ") rows=%.0f", path->parent->rows);
	}
	pos += snprintf(buff+pos, max-pos," cost=%.2f..%.2f\n", path->startup_cost, path->total_cost);

	if (path->pathkeys)
	{
		pos += snprintf(buff+pos, max-pos, "  pathkeys: ");
		pos = cstr_pathkeys(buff,  pos,  max, path->pathkeys, root->parse->rtable);
	}

	if (join)
	{
		JoinPath   *jp = (JoinPath *) path;

		pos += snprintf(buff+pos, max-pos, "  clauses: ");
		pos = cstr_restrictclauses(buff,  pos,  max, root, jp->joinrestrictinfo);
		pos += snprintf(buff+pos, max-pos, "\n");

		if (IsA(path, MergePath))
		{
			MergePath  *mp = (MergePath *) path;

			pos += snprintf(buff+pos, max-pos,  "  sortouter=%d sortinner=%d materializeinner=%d\n",
				   ((mp->outersortkeys) ? 1 : 0),
				   ((mp->innersortkeys) ? 1 : 0),
				   ((mp->materialize_inner) ? 1 : 0));
		}

	}
	return pos;
}

int
cstr_rel(char* buff, int pos, int max, PlannerInfo *root, RelOptInfo *rel)
{
	ListCell   *l;

	pos += snprintf(buff+pos, max-pos, "RELOPTINFO (");
	pos = cstr_relids(buff, pos, max, rel->relids);
	pos += snprintf(buff+pos, max-pos, "): rows=%.0f width=%d\n", rel->rows, rel->width);
	pos = cstr_relids_to_rte(buff, pos, max, root->parse->rtable, rel->relids);

	if (rel->baserestrictinfo)
	{
		pos += snprintf(buff+pos, max-pos, "\tbaserestrictinfo: ");
		pos = cstr_restrictclauses(buff, pos, max, root, rel->baserestrictinfo);
		pos += snprintf(buff+pos, max-pos, "\n");
	}

	if (rel->joininfo)
	{
		pos += snprintf(buff+pos, max-pos, "\tjoininfo: ");
		pos = cstr_restrictclauses(buff, pos, max, root, rel->joininfo);
		pos += snprintf(buff+pos, max-pos, "\n");
	}

	pos += snprintf(buff+pos, max-pos, "\tpath list:\n");
	foreach(l, rel->pathlist)
		pos = cstr_pathnode(buff, pos, max, root, lfirst(l));
	if (rel->cheapest_startup_path)
	{
		pos += snprintf(buff+pos, max-pos, "\n\tcheapest startup path:\n");
		pos = cstr_pathnode(buff, pos, max, root, rel->cheapest_startup_path);
	}
	if (rel->cheapest_total_path)
	{
		pos += snprintf(buff+pos, max-pos, "\n\tcheapest total path:\n");
		pos = cstr_pathnode(buff, pos, max, root, rel->cheapest_total_path);
	}
	pos += snprintf(buff+pos, max-pos, "\n");
	fflush(stdout);

	return pos;
}
