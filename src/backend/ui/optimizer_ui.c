/*
 * optimizer.c
 *
 *  Created on: 2014-04-05
 *      Author: jmate
 */

// standard libraries
#include <stdio.h>
#include <math.h>
#include <string.h>

// postgres libraries
#include "postgres.h"
#include "nodes/relation.h"
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
#include "ui/join_path_helper.h"

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

#define PWT_ROOT 0
#define PWT_JOIN 1
#define PWT_LEAF 2
#define PWT_UNKNOWN 3

static char * DDL_HASHJOIN_OPTION = "Hash Join";
static char * DDL_MERGEJOIN_OPTION = "Merge Join";
static char * DDL_NLJ_OPTION = "Nested Loop Join";


static bool isJoinPath(Path* path);
static bool isLeafPath(Path* path);
static PathWrapperTree* constructPWT_recurse(UIState* ui, Path*path, PathWrapperTree*parent) ;
static PathWrapperTree* constructPWT(UIState* ui, Path*root) ;
static void btn_change_join_clicked(GtkWidget* widget, gpointer data) ;
static void btn_change_scan_clicked(GtkWidget* widget, gpointer data) ;
static int delete_event_handler(GtkWidget* widget, GdkEvent* event, gpointer data) ;
static void destroy(GtkWidget* widget, gpointer data) ;
static int compute_height(PathWrapperTree* pwt) ;
static GtkGrid * create_grid_from_path(
    PathWrapperTree * pwt,
    char * typeStr,
    void (*onclick)(GtkWidget*, gpointer)
    ) ;
static void add_btn(
    PathWrapperTree * pwt,
    GtkGrid* grid,
    void (*onclick)(GtkWidget*, gpointer)) ;
static GtkWidget * create_join_widget_from_path(
    PathWrapperTree * pwt,
    char * typeStr,
    void (*onclick)(GtkWidget*, gpointer)
    ) ;
static GtkWidget * create_leaf_widget_from_path(
    PathWrapperTree * pwt,
    char * typeStr,
    void (*onclick)(GtkWidget*, gpointer)
    ) ;
static GtkWidget * create_hash_join_widget(PathWrapperTree* pwt) ;
static GtkWidget * create_merge_join_widget(PathWrapperTree* pwt) ;
static GtkWidget * create_nested_loop_widget(PathWrapperTree* pwt) ;
static GtkWidget * create_seq_scan_widget(PathWrapperTree* pwt) ;
static GtkWidget * create_idx_scan_widget(PathWrapperTree* pwt) ;
static GtkWidget * make_join_node_widget(PathWrapperTree* pwt) ;
static GtkWidget * make_leaf_node_widget(PathWrapperTree* pwt) ;
static void setup_grid_recurse(PathWrapperTree* pwt, GtkGrid *grid, int r, int c, int height) ;
static void setup_grid(UIState * state) ;



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

static PathWrapperTree* constructPWT_recurse(UIState* ui, Path*path, PathWrapperTree*parent) {
	PathWrapperTree* ret = malloc(sizeof(PathWrapperTree));
	ret->ui = ui;
	ret->left = NULL;
	ret->right = NULL;
	ret->parent = parent;
	ret->path = path;
	ret->cost_updated = false;
	ret->orig_startup_cost = path->startup_cost;
	ret->orig_total_cost = path->total_cost;
	if(isJoinPath(path)) {
		JoinPath* joinpath = (JoinPath*)path;
		ret->type = PWT_JOIN;
		ret->left = constructPWT_recurse(ui, joinpath->outerjoinpath, ret);
		ret->right = constructPWT_recurse(ui, joinpath->innerjoinpath, ret);
	} else if(isLeafPath(path)){
		ret->type = PWT_LEAF;
	} else {
		ret->type = PWT_UNKNOWN;
	}

	return ret;
}

static PathWrapperTree* constructPWT(UIState* ui, Path*root) {
	PathWrapperTree* ret = malloc(sizeof(PathWrapperTree));
	ret->parent = NULL;
	ret->path = NULL;
	ret->left = NULL;
	ret->right = constructPWT_recurse(ui, root, ret);
	ret->type = PWT_ROOT;
	ret->ui = ui;
	return ret;
}

static void btn_change_join_clicked(GtkWidget* widget, gpointer data) {
	char* selected;
	PathWrapperTree* pwt = (PathWrapperTree*)data;

	printf("btn_change_join_clicked CLICKED\n");
	fflush(stdout);

	selected = gtk_combo_box_text_get_active_text (pwt->ddl);

	if(strcmp(selected,DDL_NLJ_OPTION) == 0) {
		NestPath* nestpath;
		nestpath = create_nlj_path(pwt);
		pwt->path = (Path*)nestpath;
		pwt->cost_updated = true;
		//propogate_up(pwt->parent);
		setup_grid(pwt->ui);
	} else {
		printf("no value was selected\n");
		fflush(stdout);
		return;
	}


}

static void btn_change_scan_clicked(GtkWidget* widget, gpointer data) {
	//PathWrapperTree* pwt = (PathWrapperTree*)data;
	printf("btn_change_scan_clicked CLICKED\n");
	fflush(stdout);
}

static int delete_event_handler(GtkWidget* widget, GdkEvent* event, gpointer data) {
	g_print("The optimizer UI was shutdown.\n");
	return FALSE;
}

static void destroy(GtkWidget* widget, gpointer data) {
	gtk_main_quit();
}


static int compute_height(PathWrapperTree* pwt) {
	if(pwt == NULL) {
		return 0;
	} else if(pwt->type == PWT_ROOT) {
		return compute_height(pwt->right);
	} else if(pwt->type == PWT_JOIN) {
		return 1 + fmax(
				compute_height(pwt->left),
				compute_height(pwt->right));
	} else if(pwt->type == PWT_LEAF) {
		return 1;
	} else {
		return 0;
	}
}

#define CSTR_BUFF 2048

static GtkGrid * create_grid_from_path(
		PathWrapperTree * pwt,
		char * typeStr,
		void (*onclick)(GtkWidget*, gpointer)
		) {
	GtkGrid *grid;
	GtkWidget *lbl;
	GtkWidget *lblInfo;
	RelOptInfo *rel;
	char * buff;
	int pos;

	// title
	lbl = gtk_label_new( NULL );
	gtk_label_set_markup(GTK_LABEL(lbl), typeStr);

	// info
	pos = 0;
	buff = malloc(sizeof(char)*CSTR_BUFF);
	rel = pwt->path->parent;

	pos += snprintf(buff+pos, CSTR_BUFF-pos, "<span size=\"x-small\">");

	pos += snprintf(buff+pos, CSTR_BUFF-pos,
			"curr %.2f..%.2f\n", pwt->path->startup_cost,pwt->path->total_cost);

	pos = cstr_rel(buff, pos, CSTR_BUFF-1, pwt->ui->plannerinfo, rel);

	if(pwt->cost_updated) {
		pos += snprintf(buff+pos, CSTR_BUFF-pos,
				"orig %.2f..%.2f\n", pwt->orig_startup_cost, pwt->orig_total_cost);
	}

	pos += snprintf(buff+pos, CSTR_BUFF-pos, "</span>");
	lblInfo = gtk_label_new( NULL );
	gtk_label_set_markup(GTK_LABEL(lblInfo), buff);

	grid = (GtkGrid*)gtk_grid_new();
	gtk_grid_attach(grid, lbl, 0,0,1,1);
	gtk_grid_attach(grid, lblInfo, 0,1,1,1);

	gtk_widget_show(GTK_WIDGET(lbl));
	gtk_widget_show(GTK_WIDGET(lblInfo));
	gtk_widget_show(GTK_WIDGET(grid));
	return grid;
}

static void add_btn(
		PathWrapperTree * pwt,
		GtkGrid* grid,
		void (*onclick)(GtkWidget*, gpointer)) {
	GtkWidget *btn;
	btn = gtk_button_new_with_label("change");
	g_signal_connect(G_OBJECT(btn), "clicked", G_CALLBACK(*onclick), pwt);
	gtk_grid_attach(grid, btn, 0,3,1,1);
	gtk_widget_show(btn);
}

static GtkWidget * create_join_widget_from_path(
		PathWrapperTree * pwt,
		char * typeStr,
		void (*onclick)(GtkWidget*, gpointer)
		) {
	GtkComboBoxText * ddl;
	GtkGrid* grid = create_grid_from_path(
		pwt,
		typeStr,
		onclick
	);

	ddl = (GtkComboBoxText*)gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(ddl, DDL_HASHJOIN_OPTION);
	gtk_combo_box_text_append_text(ddl, DDL_MERGEJOIN_OPTION);
	gtk_combo_box_text_append_text(ddl, DDL_NLJ_OPTION);
	gtk_grid_attach(grid, GTK_WIDGET(ddl), 0,2,1,1);
	gtk_widget_show(GTK_WIDGET(ddl));
	pwt->ddl = ddl;

	add_btn(pwt, grid, onclick);
	return GTK_WIDGET(grid);
}


static char * DDL_SEQSCAN_OPTION = "Sequence Scan";
static char * DDL_IDXSCAN_OPTION = "Index Scan";

static GtkWidget * create_leaf_widget_from_path(
		PathWrapperTree * pwt,
		char * typeStr,
		void (*onclick)(GtkWidget*, gpointer)
		) {
	GtkComboBoxText * ddl;
	GtkGrid* grid = create_grid_from_path(
		pwt,
		typeStr,
		onclick
	);

	ddl = (GtkComboBoxText*)gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(ddl, DDL_SEQSCAN_OPTION);
	gtk_combo_box_text_append_text(ddl, DDL_IDXSCAN_OPTION);
	gtk_grid_attach(grid, GTK_WIDGET(ddl), 0,2,1,1);
	gtk_widget_show(GTK_WIDGET(ddl));
	pwt->ddl = ddl;

	add_btn(pwt, grid, onclick);
	return GTK_WIDGET(grid);
}


static GtkWidget * create_hash_join_widget(PathWrapperTree* pwt) {
	return GTK_WIDGET(
			create_join_widget_from_path(pwt, "<span size=\"x-small\">Hash Join</span>", &btn_change_join_clicked)
		);
}

static GtkWidget * create_merge_join_widget(PathWrapperTree* pwt) {
	return GTK_WIDGET(
			create_join_widget_from_path(pwt, "<span size=\"x-small\">Merge Join</span>", &btn_change_join_clicked)
		);
}

static GtkWidget * create_nested_loop_widget(PathWrapperTree* pwt) {
	return GTK_WIDGET(
			create_join_widget_from_path(pwt, "<span size=\"x-small\">Nested Loop</span>", &btn_change_join_clicked)
		);
}

static GtkWidget * create_seq_scan_widget(PathWrapperTree* pwt) {
	return GTK_WIDGET(
			create_leaf_widget_from_path(pwt, "<span size=\"x-small\">Sequence Scan</span>", &btn_change_scan_clicked)
		);
}

static GtkWidget * create_idx_scan_widget(PathWrapperTree* pwt) {
	return GTK_WIDGET(
			create_leaf_widget_from_path(pwt, "<span size=\"x-small\">Index Scan</span>", &btn_change_scan_clicked)
		);
}

static GtkWidget * make_join_node_widget(PathWrapperTree* pwt) {
	switch(pwt->path->pathtype){
		case T_HashJoin:
			return create_hash_join_widget(pwt);
		case T_MergeJoin:
			return create_merge_join_widget(pwt);
		case T_NestLoop:
			return create_nested_loop_widget(pwt);
		default:
			Assert(false);
			return NULL;
	}
}

static GtkWidget * make_leaf_node_widget(PathWrapperTree* pwt) {
	switch(pwt->path->pathtype){
			case T_SeqScan:
				return create_seq_scan_widget(pwt);
			case T_IndexOnlyScan :
			case T_IndexScan :
				return create_idx_scan_widget(pwt);
			default:
				Assert(false);
				return NULL;
	}
}

static void setup_grid_recurse(PathWrapperTree* pwt, GtkGrid *grid, int r, int c, int height) {
	if(pwt->type == PWT_JOIN) {
		int branchDistance;
		branchDistance = pow(2, height-2);
		gtk_grid_attach(grid, make_join_node_widget(pwt),c,r,1,1);
		setup_grid_recurse(pwt->left, grid, r+1, c-branchDistance, height-1);
		setup_grid_recurse(pwt->right, grid, r+1, c+branchDistance, height-1);
	} else if(pwt->type == PWT_LEAF) {
		gtk_grid_attach(grid, make_leaf_node_widget(pwt), c,r,1,1);
	}
}

static void setup_grid(UIState * state) {
	if(state->scrolledwindow != NULL) {
		gtk_container_remove(GTK_CONTAINER(state->window), GTK_WIDGET(state->scrolledwindow));
	}
	state->scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
	state->grid = (GtkGrid*)gtk_grid_new();

	setup_grid_recurse(
			state->pwt->right,
			state->grid,
			0,
			pow(2, state->height) -1,
			state->height);
	gtk_container_add(GTK_CONTAINER(state->scrolledwindow), GTK_WIDGET(state->grid));
	gtk_widget_show(GTK_WIDGET(state->grid));
	gtk_container_add(GTK_CONTAINER(state->window), GTK_WIDGET(state->scrolledwindow));
	gtk_widget_show(GTK_WIDGET(state->scrolledwindow));
}

void prompt_user_for_plan(PlannerInfo *root, Path **cheapest_path) {
	UIState state;
	GtkWindow *window;

	// setup GUI stuff
	gtk_init(NULL, NULL);
	window = (GtkWindow*)gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(delete_event_handler), NULL);
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(destroy), NULL);
	gtk_container_set_border_width(GTK_CONTAINER(window), 10);
	gtk_window_set_default_size(window, 1500, 900);

	// setup the grid and ui state
	state.plannerinfo = root;
	state.pwt = constructPWT(&state, *cheapest_path);
	state.height = compute_height(state.pwt);
	state.grid = NULL;
	state.scrolledwindow = NULL;
	state.window = window;
	setup_grid(&state);

	// show UI, and wait for the user to close it
	gtk_widget_show(GTK_WIDGET(window));
	gtk_main();
}
