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
	int rows;
	int cols;
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

static int compute_height(Path* path_so_far) {
	if(path_so_far == NULL) {
		return 0;
	}
	switch(path_so_far->pathtype){
		case T_SeqScan:
		case T_IndexOnlyScan :
		case T_IndexScan :
			return 1;
		default:
			return -1;
	}
}

#define CSTR_BUFF 2048

static GtkWidget * create_seq_scan_widget(UIState * state, Path * seqscan) {
	GtkGrid *grid;
	GtkWidget *lblSeqScan;
	GtkWidget *lblInfo;
	GtkWidget *btnSeqScan;
	RelOptInfo *rel;
	char * buff;

	buff = malloc(sizeof(char)*CSTR_BUFF);
	rel = seqscan->parent;
	cstr_rel(buff, 0, CSTR_BUFF-1, state->root, rel);
	printf("Sequence Scan\n");
	printf("%s\n", buff);
	grid = (GtkGrid*)gtk_grid_new();
	lblSeqScan = gtk_label_new( "Sequence Scan" );
	lblInfo = gtk_label_new( buff );
	btnSeqScan = gtk_button_new_with_label("update");
	g_signal_connect(G_OBJECT(btnSeqScan), "clicked", G_CALLBACK(btn_clicked), NULL);
	gtk_grid_attach(grid, lblSeqScan, 0,0,1,1);
	gtk_grid_attach(grid, lblInfo, 0,1,1,1);
	gtk_grid_attach(grid, btnSeqScan, 0,2,1,1);

	gtk_widget_show(GTK_WIDGET(lblSeqScan));
	gtk_widget_show(GTK_WIDGET(lblInfo));
	gtk_widget_show(GTK_WIDGET(btnSeqScan));
	gtk_widget_show(GTK_WIDGET(grid));
	return GTK_WIDGET(grid);
}

static GtkWidget * create_idx_scan_widget(UIState * state, Path * idxscan) {
	GtkGrid *grid;
	GtkWidget *lblIndexScan;
	GtkWidget *lblInfo;
	GtkWidget *btnIndexScan;
	RelOptInfo *rel;
	char * buff;

	buff = malloc(sizeof(char)*CSTR_BUFF);
	rel = idxscan->parent;
	cstr_rel(buff, 0, CSTR_BUFF, state->root, rel);
	printf("Index Scan\n");
	printf("%s\n", buff);

	grid = (GtkGrid*)gtk_grid_new();
	lblIndexScan = gtk_label_new( "Index Scan" );
	lblInfo = gtk_label_new( buff );
	btnIndexScan = gtk_button_new_with_label("update");
	g_signal_connect(G_OBJECT(btnIndexScan), "clicked", G_CALLBACK(btn_clicked), NULL);
	gtk_grid_attach(grid, lblIndexScan, 0,0,1,1);
	gtk_grid_attach(grid, lblInfo, 0,1,1,1);
	gtk_grid_attach(grid, btnIndexScan, 0,2,1,1);

	gtk_widget_show(GTK_WIDGET(lblIndexScan));
	gtk_widget_show(GTK_WIDGET(lblInfo));
	gtk_widget_show(GTK_WIDGET(btnIndexScan));
	gtk_widget_show(GTK_WIDGET(grid));
	return GTK_WIDGET(grid);
}


static void setup_grid(GtkWidget *window, UIState * state) {
	GtkGrid *grid;

	state->rows = 2;
	state->cols = 3;
	grid = (GtkGrid*)gtk_grid_new();

	if(state->height != 1) {
		//TODO: support 1 join
		//TODO: support 2 joins
		return;
	}

	switch((*state->cheapest_path)->pathtype){
			case T_SeqScan:
				gtk_grid_attach(grid, create_seq_scan_widget(state,*state->cheapest_path), 0,0,1,1);
				break;
			case T_IndexOnlyScan :
			case T_IndexScan :
				gtk_grid_attach(grid, create_idx_scan_widget(state,*state->cheapest_path), 0,0,1,1);
				break;
			default:
				return;
	}

	gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(grid));
	gtk_widget_show(GTK_WIDGET(grid));

}

void prompt_user_for_plan(PlannerInfo *root, Path **cheapest_path) {
	UIState state;
	GtkWidget *window;

	// setup GUI stuff
	gtk_init(NULL, NULL);
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(delete_event_handler), NULL);
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(destroy), NULL);
	gtk_container_set_border_width(GTK_CONTAINER(window), 10);

	// setup the grid and ui state
	state.root = root;
	state.cheapest_path = cheapest_path;
	state.height = compute_height(*cheapest_path);
	setup_grid(window, &state);

	// show UI, and wait for the user to close it
	gtk_widget_show(window);
	gtk_main();
}
