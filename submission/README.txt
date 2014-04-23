This command gets the original code that I started from:
git clone -b REL9_3_STABLE --single-branch https://github.com/postgres/postgres.git
I am using the 9.3 release branch, the same branch used in class.

This directory contains all the files that I modified. The directory structure
has not been flattened because I have multiple Makefiles that I needed to
change.

Files Changed (in alphabetical order):
src/backend/Makefile
src/backend/optimizer/path/costsize.c
src/backend/optimizer/plan/planmain.c
src/backend/optimizer/plan/planner.c
src/backend/ui/Makefile
src/backend/ui/cstr_out.c
src/backend/ui/join_path_helper.c
src/backend/ui/optimizer_ui.c
src/include/Makefile
src/include/nodes/relation.h
src/include/optimizer/planmain.h
src/include/ui/cstr_out.h
src/include/ui/join_path_helper.h
src/include/ui/optimizer_ui.h
src/include/ui/optimizer_ui_structs.h

Summary of Files Changed (in alphabetical order):

src/backend/Makefile
- lets the build know to include the ui directory added
- adds all of the GTK+ *.a libraries (you will not be able to build if your
  machines does not have GTK+3

src/backend/optimizer/path/costsize.c
- check hashtable to see if the cost was overridden where ever baserel->rows or
  param_info->ppi_rows was accessed for base relations
  - this is probably redundant with the set_baserel_size_estimates
    modifications, but I was too scared to remove it after I got it working
- for the following three functions check the hashtable to see if cost was
  overridden, if so use it
  - set_baserel_size_estimates:
		- the estimated number of rows is populated by this function into
		  baserel->rows to be used by functions that compute costs for base
      relations
  - set_joinrel_size_estimates
    - same as above, but for join relations
  - get_parameterized_joinrel_size
    - sometimes when the join cost functions retrieve the estimated number of
      rows, instead of going to rel->rows, it goes to param_info->ppi_rows. This
      function populates that field.

src/backend/optimizer/plan/planmain.c
 - entry point for UI called from here
 - after planning, passes best plan to the UI
 - if query_planner called from UI, it does not pass it the UI

src/backend/optimizer/plan/planner.c
 - when initializing the planner info, set the overridden estimates to null

src/backend/ui/Makefile
 - make file for all ui files and utility files I added
 - includes all the gtk+3 headers

src/include/ui/optimizer_ui_structs.h
 - contains all the data structures passed from optimizer_ui to helper functions
 - UIState: holds all of the state of the UI session
 - PathWrapperTree: wraps the plan in tree so that we can
   - navigate from child to parent
   - the table cell containing a node on the UI has all data it needs when it is
     manipulated by the user

src/include/ui/cstr_out.h
src/backend/ui/cstr_out.c
 - helper function writing to a cstring instead of standard out
 - a lot the code here is based on the print functions from nodes/print.h

src/include/ui/join_path_helper.h
src/backend/ui/join_path_helper.c
 - this is the ugliest file of the project
 - when changing a join from one to another, the costs need to be recomputed
   bottom up. this required recreating the paths. this are the functions that
   recreate those paths
 - it copies all the details on how to construct merge, nested loop join, and
   hash paths from joinpath.c, and joinrels.c

src/include/ui/optimizer_ui.h
src/backend/ui/optimizer_ui.c
 - this is the meat of the project
 - has the entry point that query_planner calls
 - contains the code the recursively constructs the table that displays the plan
 - also has the code that responds to the user clicking on the buttons of a node
   and initiating changes to the plan

src/include/Makefile
 - let the build system know that there is a new ui folder containing more
   header files

src/include/nodes/relation.h
 - added a hashtable to PlannerInfo for keeping track of the overridden
   estimated number of rows

src/include/optimizer/planmain.h
 - added a second entry point to the query_planner that does not start the UI


