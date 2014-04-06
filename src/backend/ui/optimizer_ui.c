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

static void hello(GtkWidget* widget, gpointer data) {
	g_print("Hello World\n");
}

static int delete_event_handler(GtkWidget* widget, GdkEvent* event, gpointer data) {
	g_print("delete event occured\n");
	return FALSE;
}

static void destroy(GtkWidget* widget, gpointer data) {
	gtk_main_quit();
}

void prompt_user_for_plan(Path **cheapest_path) {
	GtkWidget *window;
	GtkWidget *button;
	gtk_init(NULL, NULL);
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(delete_event_handler), NULL);
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(destroy), NULL);
	gtk_container_set_border_width(GTK_CONTAINER(window), 10);
	button = gtk_button_new_with_label("Hello World");
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(hello), NULL);
	gtk_container_add(GTK_CONTAINER(window), button);
	gtk_widget_show(button);
	gtk_widget_show(window);
	gtk_main();
	printf("CS648:**********************************\n");
	printf("CS648:\tthis is where the UI will launch\n");
	printf("CS648:**********************************\n");
}
