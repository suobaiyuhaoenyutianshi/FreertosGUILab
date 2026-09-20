#ifndef GUI_SCALE_PANEL_PANEL_H
#define GUI_SCALE_PANEL_PANEL_H
#include <stdint.h>
void vCancel_SCALE_panel(void);
void vSelected_SCALE_panel(void);
void vadjust_SCALE_panel(int8_t directNum);
void vRepaint_SCALE_panel(void);
float fGetYScale(void);
float fGetXScale(void);
#endif