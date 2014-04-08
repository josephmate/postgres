/*
 * optimizer.c
 *
 *  Created on: 2014-04-05
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

// GUI libraries
#include <gtk/gtk.h>

// headers I introduced:
#include "ui/cstr_out.h"

// the header I'm working on
#include "ui/optimizer_ui.h"

/**
 * References:
 * 1) started gtk GUI code from
 *    http://recolog.blogspot.ca/2011/08/gui-programming-with-c-language-on.html
 * 2) Getting Makefiles to work with gtk
 *    http://resources.esri.com/help/9.3/arcgisengine/com_cpp/Cpp/reference/Makefile.LinuxGTK.htm
 */

/**
 * Each node of the path will be placed as an element of the grid/table.
 * For instance:
 * 1) with no join, the grid will just be 1 by 1 with the only node placed at 0,0.
 * 2) with a single join, the grid will be 2 rows by 3 columns.
 *    i)   the join operator will be at 0,1
 *    ii)  the left  child node will be at 1,0
 *    iii) the right child node will be at 1,2
 *    iv)  ex:
 *         EMPTY    MERGEJOIN EMPTY
 *         LEFTSCAN EMPTY     RIGHTSCAN
 */





/**
 * This structure holds all the state related to the user
 * creating his own path
 */
struct UIState {
	int height;
	PlannerInfo *root;
	Path **cheapest_path;
	List* nodeStatesToFree;
};
typedef struct UIState UIState;

/**
 * This structure holds the state directly related to each node.
 */
struct NodeGridState {
	UIState* state;
	Path* path;
	int row;
	int col;
	char * labelStr;
};
typedef struct GridState UIStGridStateate;



static void btn_clicked(GtkWidget* widget, gpointer data) {
	printf("CLICKED\n");
	fflush(stdout);
}

static int delete_event_handler(GtkWidget* widget, GdkEvent* event, gpointer data) {
	g_print("The optimizer UI was shutdown.\n");
	return FALSE;
}

static void destroy(GtkWidget* widget, gpointer data) {
	gtk_main_quit();
}

static bool isJoinPath(Path* path) {
	if(path == NULL) {
		return false;
	}
	switch(path->pathtype){
		case T_HashJoin:
		case T_MergeJoin:
		case T_NestLoop:
			return true;
		default:
			return false;
	}
}

static bool isLeafPath(Path* path) {
	if(path == NULL) {
		return false;
	}
	switch(path->pathtype){
		case T_SeqScan:
		case T_IndexOnlyScan :
		case T_IndexScan :
			return true;
		default:
			return false;
	}
}

static int compute_height(Path* path_so_far) {
	if(path_so_far == NULL) {
		return 0;
	} else if(isJoinPath(path_so_far)) {
		JoinPath * joinPath;
		joinPath = (JoinPath *)path_so_far;
		return 1 + fmax(
				compute_height(joinPath->outerjoinpath),
				compute_height(joinPath->innerjoinpath));
	} else if(isLeafPath(path_so_far)) {
		return 1;
	} else {
		return -10000000;
	}
}

#define CSTR_BUFF 2048

static GtkWidget * create_widget_from_path(
		UIState * state,
		Path * seqscan,
		char * typeStr,
		void (*onclick)(GtkWidget*, gpointer)
		) {
	GtkGrid *grid;
	GtkWidget *lbl;
	GtkWidget *lblInfo;
	GtkWidget *btn;
	RelOptInfo *rel;
	char * buff;

	buff = malloc(sizeof(char)*CSTR_BUFF);
	rel = seqscan->parent;
	cstr_rel(buff, 0, CSTR_BUFF-1, state->root, rel);
	grid = (GtkGrid*)gtk_grid_new();
	lbl = gtk_label_new( typeStr );
	lblInfo = gtk_label_new( buff );
	btn = gtk_button_new_with_label("update");
	g_signal_connect(G_OBJECT(btn), "clicked", G_CALLBACK(*onclick), NULL);
	gtk_grid_attach(grid, lbl, 0,0,1,1);
	gtk_grid_attach(grid, lblInfo, 0,1,1,1);
	gtk_grid_attach(grid, btn, 0,2,1,1);

	gtk_widget_show(GTK_WIDGET(lbl));
	gtk_widget_show(GTK_WIDGET(lblInfo));
	gtk_widget_show(GTK_WIDGET(btn));
	gtk_widget_show(GTK_WIDGET(grid));
	return GTK_WIDGET(grid);
}

static GtkWidget * create_hash_join_widget(UIState * state, JoinPath * joinpath) {
	return GTK_WIDGET(
			create_widget_from_path(state, &joinpath->path, "Hash Join", &btn_clicked)
		);
}

static GtkWidget * create_merge_join_widget(UIState * state, JoinPath * joinpath) {
	return GTK_WIDGET(
			create_widget_from_path(state, &joinpath->path, "Merge Join", &btn_clicked)
		);
}

static GtkWidget * create_nested_loop_widget(UIState * state, JoinPath * joinpath) {
	return GTK_WIDGET(
			create_widget_from_path(state, &joinpath->path, "Nested Loop", &btn_clicked)
		);
}

static GtkWidget * create_seq_scan_widget(UIState * state, Path * seqscan) {
	return GTK_WIDGET(
			create_widget_from_path(state, seqscan, "Sequence Scan", &btn_clicked)
		);
}

static GtkWidget * create_idx_scan_widget(UIState * state, Path * idxscan) {
	return GTK_WIDGET(
			create_widget_from_path(state, idxscan, "Index Scan", &btn_clicked)
		);
}

static GtkWidget * make_join_node_widget(UIState* state, JoinPath* joinpath) {
	switch(joinpath->path.pathtype){
		case T_HashJoin:
			return create_hash_join_widget(state, joinpath);
		case T_MergeJoin:
			return create_merge_join_widget(state, joinpath);
		case T_NestLoop:
			return create_nested_loop_widget(state, joinpath);
		default:
			Assert(false);
			return NULL;
	}
}

static GtkWidget * make_leaf_node_widget(UIState* state, Path* path) {
	switch(path->pathtype){
			case T_SeqScan:
				return create_seq_scan_widget(state, path);
			case T_IndexOnlyScan :
			case T_IndexScan :
				return create_idx_scan_widget(state,path);
			default:
				Assert(false);
				return NULL;
	}
}

static void setup_grid_recurse(UIState * state, Path* path, GtkGrid *grid, int r, int c, int height) {
	if(isJoinPath(path)) {
		int branchDistance;
		JoinPath* joinpath;
		joinpath = (JoinPath*)path;
		branchDistance = pow(2, height-2);
		gtk_grid_attach(grid, make_join_node_widget(state,joinpath),c,r,1,1);
		setup_grid_recurse(state, joinpath->outerjoinpath, grid, r+1, c-branchDistance, height-1);
		setup_grid_recurse(state, joinpath->innerjoinpath, grid, r+1, c+branchDistance, height-1);
	} else if(isLeafPath(path)) {
		gtk_grid_attach(grid, make_leaf_node_widget(state,path), c,r,1,1);
	}
}

static void setup_grid(GtkWidget *window, UIState * state) {
	GtkGrid *grid;
	grid = (GtkGrid*)gtk_grid_new();


	setup_grid_recurse(
			state,
			*state->cheapest_path,
			grid,
			0,
			pow(2, state->height) -1,
			state->height);

	gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(grid));
	gtk_widget_show(GTK_WIDGET(grid));

}

void prompt_user_for_plan(PlannerInfo *root, Path **cheapest_path) {
	UIState state;
	GtkWidget *window;
	GtkWidget *scrolledWindow;

	// setup GUI stuff
	gtk_init(NULL, NULL);
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	scrolledWindow = gtk_scrolled_window_new(NULL, NULL);
	g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(delete_event_handler), NULL);
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(destroy), NULL);
	gtk_container_set_border_width(GTK_CONTAINER(window), 10);

	// setup the grid and ui state
	state.root = root;
	state.cheapest_path = cheapest_path;
	state.height = compute_height(*cheapest_path);
	setup_grid(scrolledWindow, &state);

	gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(scrolledWindow));
	// show UI, and wait for the user to close it
	gtk_widget_show(scrolledWindow);
	gtk_widget_show(window);
	gtk_main();
}
