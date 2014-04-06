/*
 * optimizer.c
 *
 *  Created on: 2014-04-05
 *      Author: jmate
 */

// standard libraries
#include <stdio.h>

// postgres libraries
#include "postgres.h"
#include "miscadmin.h"
#include "optimizer/cost.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/placeholder.h"
#include "optimizer/planmain.h"
#include "optimizer/tlist.h"
#include "utils/selfuncs.h"

// the header I'm working on
#include "ui/optimizer_ui.h"

void prompt_user_for_plan(Path **cheapest_path) {
	printf("CS648:**********************************\n");
	printf("CS648:\tthis is where the UI will launch\n");
	printf("CS648:**********************************\n");
}
