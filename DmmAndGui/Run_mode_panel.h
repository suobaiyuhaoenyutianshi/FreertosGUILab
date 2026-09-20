#ifndef GUI_RUN_MODE_PANEL_H
#define GUI_RUN_MODE_PANEL_H

#include <stdint.h>
//取消
//选中
//调整
//重画
void vCancel_RUN_mode_panel(void);
void vSelected_RUN_mode_panel(void);
void vadjust_RUN_mode_panel(int8_t directNum);
void vRepaint_RUN_mode_panel(void);
#endif