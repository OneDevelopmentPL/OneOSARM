/* OneOS-ARM GUI Module */

#ifndef GUI_H
#define GUI_H

/* Set the initial clock time (call before gui_init) */
void gui_set_time(int hour, int minute);

/* Initialize GUI mode (draws desktop, enables mouse) */
void gui_init(void);

/* Run the GUI event loop (never returns unless user exits) */
void gui_run(void);

#endif
