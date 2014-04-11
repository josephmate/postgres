/*
 * optimizer_ui.h
 *
 *  Created on: 2014-04-05
 *      Author: jmate
 */

#ifndef OPTIMIZER_UI_H_
#define OPTIMIZER_UI_H_

#include "nodes/relation.h"


extern void prompt_user_for_plan(
		PlannerInfo *root,
		List *tlist,
		double tuple_fraction,
		double limit_tuples,
		query_pathkeys_callback qp_callback,
		void *qp_extra,
		Path **cheapest_path,
		Path **sorted_path,
		double *num_groups
		);


#endif /* OPTIMIZER_UI_H_ */
