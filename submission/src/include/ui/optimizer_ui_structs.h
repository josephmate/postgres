/*
 * optimizer_ui_structs.h
 *
 *  Created on: 2014-04-10
 *      Author: jmate
 */

#ifndef OPTIMIZER_UI_STRUCTS_H_
#define OPTIMIZER_UI_STRUCTS_H_

#include <gtk/gtk.h>


typedef struct UIState UIState;
typedef struct PathWrapperTree PathWrapperTree;

/**
 * This structure holds all the state related to the user
 * creating his own path
 */
struct UIState {
	int height;
	PathWrapperTree *pwt;
	GtkGrid* grid;
	GtkWindow* window;
	GtkWidget* scrolledwindow;

	PlannerInfo * plannerinfo;
	List *tlist;
	double tuple_fraction;
	double limit_tuples;
	query_pathkeys_callback qp_callback;
	void *qp_extra;
	Path **cheapest_path;
	Path **sorted_path;
	double *num_groups;
};

/**
 * Wraps a bath to allow of backwards navigation. All manipulations will
 * occur on the path wrapper tree. When ui completes, we unwrap the wrapping
 * and place it back into Path**cheapest_path
 */
struct PathWrapperTree {
	UIState* ui;
	PathWrapperTree * parent;
	PathWrapperTree * left;  // by convention we make this the outer
	PathWrapperTree * right; // by convention we make this the inner
	                         // by convention this is the only child of the root
	int type; // one of PWT_*
	Path * path;
	bool cost_updated;
	double orig_startup_cost;
	double orig_total_cost;

	GtkComboBoxText * ddl;   //the ddl for selecting a different node type
	GtkSpinButton *spnEst;   //the text box for adjusting the estimated relation size
};


#endif /* OPTIMIZER_UI_STRUCTS_H_ */
