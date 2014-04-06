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

// GUI libraries
#include <gtk/gtk.h>

// the header I'm working on
#include "ui/optimizer_ui.h"

static void btn_clicked(GtkWidget* widget, gpointer data) {
	printf("%s\n", (char*)data);
	fflush(stdout);
}

static int delete_event_handler(GtkWidget* widget, GdkEvent* event, gpointer data) {
	g_print("delete event occured\n");
	return FALSE;
}

static void destroy(GtkWidget* widget, gpointer data) {
	gtk_main_quit();
}

static void setup_grid(GtkWidget *window) {
	int r,c;
	GtkGrid *grid;

	grid = (GtkGrid*)gtk_grid_new();

	for(r=0; r<2; r++) {
		for(c=0; c<3; c++) {
			char * cbuff;
			GtkWidget *button;
			cbuff = g_malloc0(sizeof(char)*128);
			sprintf(cbuff, "btn r:%d, c:%d", r,c);
			button = gtk_button_new_with_label(cbuff);
			g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(btn_clicked), cbuff);
			gtk_grid_attach(grid, button, c, r, 1, 1);
			gtk_widget_show(button);
		}
	}
	gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(grid));
	gtk_widget_show(GTK_WIDGET(grid));

}

void prompt_user_for_plan(Path **cheapest_path) {
	GtkWidget *window;
	gtk_init(NULL, NULL);
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(delete_event_handler), NULL);
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(destroy), NULL);
	gtk_container_set_border_width(GTK_CONTAINER(window), 10);

	setup_grid(window);

	gtk_widget_show(window);
	gtk_main();
}
