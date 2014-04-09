/*
 * join_path_helper.c
 *
 *  Created on: 2014-04-09
 *      Author: jmate
 */
// standard libraries
#include <stdio.h>
#include <math.h>

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

#include "ui/optimizer_ui.h"
#include "ui/join_path_helper.h"


/**
 * This structure holds all the state related to the user
 * creating his own path
 */
typedef struct CommonJoinStuff CommonJoinStuff;
struct CommonJoinStuff {
	PlannerInfo * plannerinfo;
	JoinCostWorkspace workspace; // i don't think this is used
	Relids		joinrelids;
	JoinPath* joinpath;
	JoinType jointype;
	RelOptInfo *rel1;
	RelOptInfo *rel2;
	SpecialJoinInfo* sjinfo;
	RelOptInfo *joinrel;
	List	   *restrictlist;
	Path* outer_path;
	Path* inner_path;
	//TODO: semi and anti joins
	//TODO: see joinpath.c:add_paths_to_joinrel
	SemiAntiJoinFactors semifactors;
	Relids		required_outer;
	Relids		extra_lateral_rels;
};


static bool
join_is_legal(
		PlannerInfo *root,
		RelOptInfo *rel1,
		RelOptInfo *rel2,
		Relids joinrelids,
		SpecialJoinInfo **sjinfo_p,
		bool *reversed_p);
static SpecialJoinInfo* special_join_info(
		PathWrapperTree* pwt
		);
static inline bool
clause_sides_match_join(RestrictInfo *rinfo, RelOptInfo *outerrel,
						RelOptInfo *innerrel);
static List *
select_mergejoin_clauses(PlannerInfo *root,
						 RelOptInfo *joinrel,
						 RelOptInfo *outerrel,
						 RelOptInfo *innerrel,
						 List *restrictlist,
						 JoinType jointype,
						 bool *mergejoin_allowed);
static void getCommonJoinStuff(CommonJoinStuff * jointstuff, PathWrapperTree* pwt) ;


MergePath* recreate_mergejoin_path(PathWrapperTree* pwt) {
	CommonJoinStuff cjs;
	List *all_pathkeys;
	List *mergeclause_list = NIL;
	List *cur_mergeclauses;
	bool mergejoin_allowed;
	List	   *outerkeys;
	List	   *innerkeys;
	List	   *merge_pathkeys;
  List *mergeclauses;
  List *outersortkeys;
  List *innersortkeys;

	getCommonJoinStuff(&cjs, pwt);

	mergeclause_list = select_mergejoin_clauses(
			cjs.plannerinfo,
			cjs.joinrel,
			cjs.rel1,
			cjs.rel2,
			cjs.restrictlist,
			cjs.jointype,
			&mergejoin_allowed);
	if(!mergejoin_allowed) {
		printf("Abandon ship! Merge join was not allowed");
	}

	//TODO: consider more than just the default outer
	//TODO: handle unique-fication
	//TODO: more info in joinpath.c:sort_inner_and_outer

	all_pathkeys = select_outer_pathkeys_for_merge(
			cjs.plannerinfo,
			mergeclause_list,
			cjs.joinrel);
	outerkeys = all_pathkeys;

	/* Sort the mergeclauses into the corresponding ordering */
	cur_mergeclauses = find_mergeclauses_for_pathkeys(
			cjs.plannerinfo,
			outerkeys,
			true,
			mergeclause_list);

	/* Build sort pathkeys for the inner side */
	innerkeys = make_inner_pathkeys_for_merge(
			cjs.plannerinfo,
			cur_mergeclauses,
			outerkeys);

	/* Build pathkeys representing output sort order */
	merge_pathkeys = build_join_pathkeys(
			cjs.plannerinfo,
			cjs.joinrel,
			cjs.jointype,
			outerkeys);
  mergeclauses = cur_mergeclauses;
  outersortkeys = outerkeys;
  innersortkeys = innerkeys;

	/*
	 * If the given paths are already well enough ordered, we can skip doing
	 * an explicit sort.
	 */
	if (outersortkeys &&
		pathkeys_contained_in(outersortkeys, cjs.outer_path->pathkeys)) {
		outersortkeys = NIL;
	}
	if (innersortkeys &&
		pathkeys_contained_in(innersortkeys, cjs.inner_path->pathkeys)) {
		innersortkeys = NIL;
	}

	initial_cost_mergejoin(
			cjs.plannerinfo,
			&cjs.workspace,
			cjs.jointype,
			mergeclauses,
			cjs.outer_path,
			cjs.inner_path,
			outersortkeys,
			innersortkeys,
			cjs.sjinfo);

	return create_mergejoin_path(
			cjs.plannerinfo,
			cjs.joinrel,
			cjs.jointype,
			&cjs.workspace,
			cjs.sjinfo,
			cjs.outer_path,
			cjs.inner_path,
			cjs.restrictlist,
			merge_pathkeys,
		  cjs.required_outer,
		  mergeclauses,
		  outersortkeys,
		  innersortkeys);
}

HashPath* recreate_hashjoin_path(PathWrapperTree* pwt)  {
	CommonJoinStuff cjs;
	List	   *hashclauses;
	ListCell   *lc;
	bool		isouterjoin;

	getCommonJoinStuff(&cjs, pwt);
	isouterjoin = IS_OUTER_JOIN(cjs.jointype);

	hashclauses = NIL;
	foreach(lc, cjs.restrictlist)
	{
		RestrictInfo *restrictinfo = (RestrictInfo *) lfirst(lc);

		/*
		 * If processing an outer join, only use its own join clauses for
		 * hashing.  For inner joins we need not be so picky.
		 */
		if (isouterjoin && restrictinfo->is_pushed_down)
			continue;

		if (!restrictinfo->can_join ||
			restrictinfo->hashjoinoperator == InvalidOid)
			continue;			/* not hashjoinable */

		/*
		 * Check if clause has the form "outer op inner" or "inner op outer".
		 */
		if (!clause_sides_match_join(restrictinfo, cjs.rel1, cjs.rel2))
			continue;			/* no good for these input relations */

		hashclauses = lappend(hashclauses, restrictinfo);
	}

	initial_cost_hashjoin(
			cjs.plannerinfo,
			&cjs.workspace,
			cjs.jointype,
			hashclauses,
			cjs.outer_path,
			cjs.inner_path,
			cjs.sjinfo,
			&cjs.semifactors);

	return create_hashjoin_path(
		cjs.plannerinfo,
		cjs.joinrel,
		cjs.jointype,
		&cjs.workspace,
		cjs.sjinfo,
		&cjs.semifactors,
		cjs.outer_path,
		cjs.inner_path,
		cjs.restrictlist,
		cjs.required_outer,
		hashclauses
		);
}

NestPath* recreate_nlj_path(PathWrapperTree* pwt) {
	CommonJoinStuff cjs;
	NestPath* nextPath;
	List	   *merge_pathkeys;

	getCommonJoinStuff(&cjs, pwt);

	initial_cost_nestloop(
			cjs.plannerinfo,
			&cjs.workspace,
			cjs.jointype,
			cjs.outer_path,
			cjs.inner_path,
			cjs.sjinfo,
			&cjs.semifactors);


	merge_pathkeys = build_join_pathkeys(
			cjs.plannerinfo,
			cjs.joinrel,
			cjs.jointype,
			cjs.outer_path->pathkeys);

	nextPath = create_nestloop_path(
			cjs.plannerinfo,
			cjs.joinrel,
			cjs.jointype,
			&cjs.workspace,
			cjs.sjinfo,
			&cjs.semifactors,
			cjs.outer_path,
			cjs.inner_path,
			cjs.restrictlist,
			merge_pathkeys,
			cjs.required_outer
			);

	return nextPath;
}

static void getCommonJoinStuff(CommonJoinStuff * cjs, PathWrapperTree* pwt) {
	ListCell   *lc;

	cjs->rel1 = pwt->left->path->parent;
	cjs->rel2 = pwt->right->path->parent;
	cjs->plannerinfo = pwt->ui->plannerinfo;
	cjs->joinrelids = pwt->path->parent->relids;
	cjs->joinpath = (JoinPath*) pwt->path;
	cjs->jointype = cjs->joinpath->jointype;
	cjs->outer_path = pwt->left->path;
	cjs->inner_path = pwt->right->path;
	cjs->extra_lateral_rels = NULL;

	cjs->sjinfo = special_join_info(pwt);

	cjs->joinrel = build_join_rel(
			cjs->plannerinfo,
			cjs->joinrelids,
			cjs->rel1,
			cjs->rel2,
			cjs->sjinfo,
			&cjs->restrictlist);

	cjs->required_outer = calc_non_nestloop_required_outer(
			cjs->outer_path,
			cjs->inner_path);


	foreach(lc, cjs->plannerinfo->placeholder_list)
	{
		PlaceHolderInfo *phinfo = (PlaceHolderInfo *) lfirst(lc);

		/* PHVs without lateral refs can be skipped over quickly */
		if (phinfo->ph_lateral == NULL)
			continue;
		/* Is it due to be evaluated at this join, and not in either input? */
		if (bms_is_subset(phinfo->ph_eval_at, cjs->joinrel->relids) &&
			!bms_is_subset(phinfo->ph_eval_at, cjs->rel1->relids) &&
			!bms_is_subset(phinfo->ph_eval_at, cjs->rel2->relids))
		{
			/* Yes, remember its lateral rels */
			cjs->extra_lateral_rels = bms_add_members(cjs->extra_lateral_rels,
												 phinfo->ph_lateral);
		}
	}

	cjs->required_outer = bms_add_members(cjs->required_outer, cjs->extra_lateral_rels);
}

static SpecialJoinInfo* special_join_info(
		PathWrapperTree* pwt
		) {
	Relids		joinrelids;
	SpecialJoinInfo *sjinfo;
	bool		reversed;
	SpecialJoinInfo sjinfo_data; // in the case we get nothing back
	RelOptInfo *rel1;
	RelOptInfo *rel2;

	joinrelids = pwt->path->parent->relids;
	rel1 = pwt->left->path->parent;
	rel2 = pwt->right->path->parent;
	if (!join_is_legal(pwt->ui->plannerinfo, rel1, rel2, joinrelids,
					   &sjinfo, &reversed))
	{
		/* invalid join path */
		bms_free(joinrelids);
		return NULL;
	}

	/* Swap rels if needed to match the join info. */
	if (reversed)
	{
		RelOptInfo *trel = rel1;
		rel1 = rel2;
		rel2 = trel;
	}

	/*
	 * If it's a plain inner join, then we won't have found anything in
	 * join_info_list.	Make up a SpecialJoinInfo so that selectivity
	 * estimation functions will know what's being joined.
	 */
	if (sjinfo == NULL)
	{
		sjinfo = &sjinfo_data;
		sjinfo->type = T_SpecialJoinInfo;
		sjinfo->min_lefthand = rel1->relids;
		sjinfo->min_righthand = rel2->relids;
		sjinfo->syn_lefthand = rel1->relids;
		sjinfo->syn_righthand = rel2->relids;
		sjinfo->jointype = JOIN_INNER;
		/* we don't bother trying to make the remaining fields valid */
		sjinfo->lhs_strict = false;
		sjinfo->delay_upper_joins = false;
		sjinfo->join_quals = NIL;
	}

	return sjinfo;
}


/*
 * taken from joinrels.c
 *
 * join_is_legal
 *	   Determine whether a proposed join is legal given the query's
 *	   join order constraints; and if it is, determine the join type.
 *
 * Caller must supply not only the two rels, but the union of their relids.
 * (We could simplify the API by computing joinrelids locally, but this
 * would be redundant work in the normal path through make_join_rel.)
 *
 * On success, *sjinfo_p is set to NULL if this is to be a plain inner join,
 * else it's set to point to the associated SpecialJoinInfo node.  Also,
 * *reversed_p is set TRUE if the given relations need to be swapped to
 * match the SpecialJoinInfo node.
 */
static bool
join_is_legal(
		PlannerInfo *root,
		RelOptInfo *rel1,
		RelOptInfo *rel2,
		Relids joinrelids,
		SpecialJoinInfo **sjinfo_p,
		bool *reversed_p)
{
	SpecialJoinInfo *match_sjinfo;
	bool		reversed;
	bool		unique_ified;
	bool		is_valid_inner;
	bool		lateral_fwd;
	bool		lateral_rev;
	ListCell   *l;

	/*
	 * Ensure output params are set on failure return.	This is just to
	 * suppress uninitialized-variable warnings from overly anal compilers.
	 */
	*sjinfo_p = NULL;
	*reversed_p = false;

	/*
	 * If we have any special joins, the proposed join might be illegal; and
	 * in any case we have to determine its join type.	Scan the join info
	 * list for conflicts.
	 */
	match_sjinfo = NULL;
	reversed = false;
	unique_ified = false;
	is_valid_inner = true;

	foreach(l, root->join_info_list)
	{
		SpecialJoinInfo *sjinfo = (SpecialJoinInfo *) lfirst(l);

		/*
		 * This special join is not relevant unless its RHS overlaps the
		 * proposed join.  (Check this first as a fast path for dismissing
		 * most irrelevant SJs quickly.)
		 */
		if (!bms_overlap(sjinfo->min_righthand, joinrelids))
			continue;

		/*
		 * Also, not relevant if proposed join is fully contained within RHS
		 * (ie, we're still building up the RHS).
		 */
		if (bms_is_subset(joinrelids, sjinfo->min_righthand))
			continue;

		/*
		 * Also, not relevant if SJ is already done within either input.
		 */
		if (bms_is_subset(sjinfo->min_lefthand, rel1->relids) &&
			bms_is_subset(sjinfo->min_righthand, rel1->relids))
			continue;
		if (bms_is_subset(sjinfo->min_lefthand, rel2->relids) &&
			bms_is_subset(sjinfo->min_righthand, rel2->relids))
			continue;

		/*
		 * If it's a semijoin and we already joined the RHS to any other rels
		 * within either input, then we must have unique-ified the RHS at that
		 * point (see below).  Therefore the semijoin is no longer relevant in
		 * this join path.
		 */
		if (sjinfo->jointype == JOIN_SEMI)
		{
			if (bms_is_subset(sjinfo->syn_righthand, rel1->relids) &&
				!bms_equal(sjinfo->syn_righthand, rel1->relids))
				continue;
			if (bms_is_subset(sjinfo->syn_righthand, rel2->relids) &&
				!bms_equal(sjinfo->syn_righthand, rel2->relids))
				continue;
		}

		/*
		 * If one input contains min_lefthand and the other contains
		 * min_righthand, then we can perform the SJ at this join.
		 *
		 * Barf if we get matches to more than one SJ (is that possible?)
		 */
		if (bms_is_subset(sjinfo->min_lefthand, rel1->relids) &&
			bms_is_subset(sjinfo->min_righthand, rel2->relids))
		{
			if (match_sjinfo)
				return false;	/* invalid join path */
			match_sjinfo = sjinfo;
			reversed = false;
		}
		else if (bms_is_subset(sjinfo->min_lefthand, rel2->relids) &&
				 bms_is_subset(sjinfo->min_righthand, rel1->relids))
		{
			if (match_sjinfo)
				return false;	/* invalid join path */
			match_sjinfo = sjinfo;
			reversed = true;
		}
		else if (sjinfo->jointype == JOIN_SEMI &&
				 bms_equal(sjinfo->syn_righthand, rel2->relids) &&
				 create_unique_path(root, rel2, rel2->cheapest_total_path,
									sjinfo) != NULL)
		{
			/*----------
			 * For a semijoin, we can join the RHS to anything else by
			 * unique-ifying the RHS (if the RHS can be unique-ified).
			 * We will only get here if we have the full RHS but less
			 * than min_lefthand on the LHS.
			 *
			 * The reason to consider such a join path is exemplified by
			 *	SELECT ... FROM a,b WHERE (a.x,b.y) IN (SELECT c1,c2 FROM c)
			 * If we insist on doing this as a semijoin we will first have
			 * to form the cartesian product of A*B.  But if we unique-ify
			 * C then the semijoin becomes a plain innerjoin and we can join
			 * in any order, eg C to A and then to B.  When C is much smaller
			 * than A and B this can be a huge win.  So we allow C to be
			 * joined to just A or just B here, and then make_join_rel has
			 * to handle the case properly.
			 *
			 * Note that actually we'll allow unique-ified C to be joined to
			 * some other relation D here, too.  That is legal, if usually not
			 * very sane, and this routine is only concerned with legality not
			 * with whether the join is good strategy.
			 *----------
			 */
			if (match_sjinfo)
				return false;	/* invalid join path */
			match_sjinfo = sjinfo;
			reversed = false;
			unique_ified = true;
		}
		else if (sjinfo->jointype == JOIN_SEMI &&
				 bms_equal(sjinfo->syn_righthand, rel1->relids) &&
				 create_unique_path(root, rel1, rel1->cheapest_total_path,
									sjinfo) != NULL)
		{
			/* Reversed semijoin case */
			if (match_sjinfo)
				return false;	/* invalid join path */
			match_sjinfo = sjinfo;
			reversed = true;
			unique_ified = true;
		}
		else
		{
			/*----------
			 * Otherwise, the proposed join overlaps the RHS but isn't
			 * a valid implementation of this SJ.  It might still be
			 * a legal join, however.  If both inputs overlap the RHS,
			 * assume that it's OK.  Since the inputs presumably got past
			 * this function's checks previously, they can't overlap the
			 * LHS and their violations of the RHS boundary must represent
			 * SJs that have been determined to commute with this one.
			 * We have to allow this to work correctly in cases like
			 *		(a LEFT JOIN (b JOIN (c LEFT JOIN d)))
			 * when the c/d join has been determined to commute with the join
			 * to a, and hence d is not part of min_righthand for the upper
			 * join.  It should be legal to join b to c/d but this will appear
			 * as a violation of the upper join's RHS.
			 * Furthermore, if one input overlaps the RHS and the other does
			 * not, we should still allow the join if it is a valid
			 * implementation of some other SJ.  We have to allow this to
			 * support the associative identity
			 *		(a LJ b on Pab) LJ c ON Pbc = a LJ (b LJ c ON Pbc) on Pab
			 * since joining B directly to C violates the lower SJ's RHS.
			 * We assume that make_outerjoininfo() set things up correctly
			 * so that we'll only match to some SJ if the join is valid.
			 * Set flag here to check at bottom of loop.
			 *----------
			 */
			if (sjinfo->jointype != JOIN_SEMI &&
				bms_overlap(rel1->relids, sjinfo->min_righthand) &&
				bms_overlap(rel2->relids, sjinfo->min_righthand))
			{
				/* seems OK */
				Assert(!bms_overlap(joinrelids, sjinfo->min_lefthand));
			}
			else
				is_valid_inner = false;
		}
	}

	/*
	 * Fail if violated some SJ's RHS and didn't match to another SJ. However,
	 * "matching" to a semijoin we are implementing by unique-ification
	 * doesn't count (think: it's really an inner join).
	 */
	if (!is_valid_inner &&
		(match_sjinfo == NULL || unique_ified))
		return false;			/* invalid join path */

	/*
	 * We also have to check for constraints imposed by LATERAL references.
	 * The proposed rels could each contain lateral references to the other,
	 * in which case the join is impossible.  If there are lateral references
	 * in just one direction, then the join has to be done with a nestloop
	 * with the lateral referencer on the inside.  If the join matches an SJ
	 * that cannot be implemented by such a nestloop, the join is impossible.
	 */
	lateral_fwd = lateral_rev = false;
	foreach(l, root->lateral_info_list)
	{
		LateralJoinInfo *ljinfo = (LateralJoinInfo *) lfirst(l);

		if (bms_is_subset(ljinfo->lateral_rhs, rel2->relids) &&
			bms_overlap(ljinfo->lateral_lhs, rel1->relids))
		{
			/* has to be implemented as nestloop with rel1 on left */
			if (lateral_rev)
				return false;	/* have lateral refs in both directions */
			lateral_fwd = true;
			if (!bms_is_subset(ljinfo->lateral_lhs, rel1->relids))
				return false;	/* rel1 can't compute the required parameter */
			if (match_sjinfo &&
				(reversed || match_sjinfo->jointype == JOIN_FULL))
				return false;	/* not implementable as nestloop */
		}
		if (bms_is_subset(ljinfo->lateral_rhs, rel1->relids) &&
			bms_overlap(ljinfo->lateral_lhs, rel2->relids))
		{
			/* has to be implemented as nestloop with rel2 on left */
			if (lateral_fwd)
				return false;	/* have lateral refs in both directions */
			lateral_rev = true;
			if (!bms_is_subset(ljinfo->lateral_lhs, rel2->relids))
				return false;	/* rel2 can't compute the required parameter */
			if (match_sjinfo &&
				(!reversed || match_sjinfo->jointype == JOIN_FULL))
				return false;	/* not implementable as nestloop */
		}
	}

	/* Otherwise, it's a valid join */
	*sjinfo_p = match_sjinfo;
	*reversed_p = reversed;
	return true;
}

/* taken from
 * joinpath.c:clause_sides_match_join
 * clause_sides_match_join
 *	  Determine whether a join clause is of the right form to use in this join.
 *
 * We already know that the clause is a binary opclause referencing only the
 * rels in the current join.  The point here is to check whether it has the
 * form "outerrel_expr op innerrel_expr" or "innerrel_expr op outerrel_expr",
 * rather than mixing outer and inner vars on either side.	If it matches,
 * we set the transient flag outer_is_left to identify which side is which.
 */
static inline bool
clause_sides_match_join(RestrictInfo *rinfo, RelOptInfo *outerrel,
						RelOptInfo *innerrel)
{
	if (bms_is_subset(rinfo->left_relids, outerrel->relids) &&
		bms_is_subset(rinfo->right_relids, innerrel->relids))
	{
		/* lefthand side is outer */
		rinfo->outer_is_left = true;
		return true;
	}
	else if (bms_is_subset(rinfo->left_relids, innerrel->relids) &&
			 bms_is_subset(rinfo->right_relids, outerrel->relids))
	{
		/* righthand side is outer */
		rinfo->outer_is_left = false;
		return true;
	}
	return false;				/* no good for these input relations */
}

/* taken from joinpath.c:select_mergejoin_clauses
 * select_mergejoin_clauses
 *	  Select mergejoin clauses that are usable for a particular join.
 *	  Returns a list of RestrictInfo nodes for those clauses.
 *
 * *mergejoin_allowed is normally set to TRUE, but it is set to FALSE if
 * this is a right/full join and there are nonmergejoinable join clauses.
 * The executor's mergejoin machinery cannot handle such cases, so we have
 * to avoid generating a mergejoin plan.  (Note that this flag does NOT
 * consider whether there are actually any mergejoinable clauses.  This is
 * correct because in some cases we need to build a clauseless mergejoin.
 * Simply returning NIL is therefore not enough to distinguish safe from
 * unsafe cases.)
 *
 * We also mark each selected RestrictInfo to show which side is currently
 * being considered as outer.  These are transient markings that are only
 * good for the duration of the current add_paths_to_joinrel() call!
 *
 * We examine each restrictinfo clause known for the join to see
 * if it is mergejoinable and involves vars from the two sub-relations
 * currently of interest.
 */
static List *
select_mergejoin_clauses(PlannerInfo *root,
						 RelOptInfo *joinrel,
						 RelOptInfo *outerrel,
						 RelOptInfo *innerrel,
						 List *restrictlist,
						 JoinType jointype,
						 bool *mergejoin_allowed)
{
	List	   *result_list = NIL;
	bool		isouterjoin = IS_OUTER_JOIN(jointype);
	bool		have_nonmergeable_joinclause = false;
	ListCell   *l;

	foreach(l, restrictlist)
	{
		RestrictInfo *restrictinfo = (RestrictInfo *) lfirst(l);

		/*
		 * If processing an outer join, only use its own join clauses in the
		 * merge.  For inner joins we can use pushed-down clauses too. (Note:
		 * we don't set have_nonmergeable_joinclause here because pushed-down
		 * clauses will become otherquals not joinquals.)
		 */
		if (isouterjoin && restrictinfo->is_pushed_down)
			continue;

		/* Check that clause is a mergeable operator clause */
		if (!restrictinfo->can_join ||
			restrictinfo->mergeopfamilies == NIL)
		{
			/*
			 * The executor can handle extra joinquals that are constants, but
			 * not anything else, when doing right/full merge join.  (The
			 * reason to support constants is so we can do FULL JOIN ON
			 * FALSE.)
			 */
			if (!restrictinfo->clause || !IsA(restrictinfo->clause, Const))
				have_nonmergeable_joinclause = true;
			continue;			/* not mergejoinable */
		}

		/*
		 * Check if clause has the form "outer op inner" or "inner op outer".
		 */
		if (!clause_sides_match_join(restrictinfo, outerrel, innerrel))
		{
			have_nonmergeable_joinclause = true;
			continue;			/* no good for these input relations */
		}

		/*
		 * Insist that each side have a non-redundant eclass.  This
		 * restriction is needed because various bits of the planner expect
		 * that each clause in a merge be associatable with some pathkey in a
		 * canonical pathkey list, but redundant eclasses can't appear in
		 * canonical sort orderings.  (XXX it might be worth relaxing this,
		 * but not enough time to address it for 8.3.)
		 *
		 * Note: it would be bad if this condition failed for an otherwise
		 * mergejoinable FULL JOIN clause, since that would result in
		 * undesirable planner failure.  I believe that is not possible
		 * however; a variable involved in a full join could only appear in
		 * below_outer_join eclasses, which aren't considered redundant.
		 *
		 * This case *can* happen for left/right join clauses: the outer-side
		 * variable could be equated to a constant.  Because we will propagate
		 * that constant across the join clause, the loss of ability to do a
		 * mergejoin is not really all that big a deal, and so it's not clear
		 * that improving this is important.
		 */
		update_mergeclause_eclasses(root, restrictinfo);

		if (EC_MUST_BE_REDUNDANT(restrictinfo->left_ec) ||
			EC_MUST_BE_REDUNDANT(restrictinfo->right_ec))
		{
			have_nonmergeable_joinclause = true;
			continue;			/* can't handle redundant eclasses */
		}

		result_list = lappend(result_list, restrictinfo);
	}

	/*
	 * Report whether mergejoin is allowed (see comment at top of function).
	 */
	switch (jointype)
	{
		case JOIN_RIGHT:
		case JOIN_FULL:
			*mergejoin_allowed = !have_nonmergeable_joinclause;
			break;
		default:
			*mergejoin_allowed = true;
			break;
	}

	return result_list;
}
