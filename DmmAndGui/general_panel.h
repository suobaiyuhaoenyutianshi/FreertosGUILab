#ifndef GUI_GENERAL_PANEL_H
#define  GUI_GENERAL_PANEL_H

#include <stdint.h>
//这些是按钮按发出的
//取消选中，放在其他按钮
typedef enum{
    WaveformShape_Off, // 关闭波形输出
	WaveformShape_Sin, // 正弦波
	WaveformShape_Triangle, // 三角波
	WaveformShape_Square, // 方波

} waveShap_t;
void vCancel_general_panel(void);
//选中 这两个放在本按钮
void vSelected_general_panel(void);
//重花
void vRepaint_geneeral_panel(void);
//调整  放在key_down与up
void vadjust_general_panel(int8_t directNum);

#endif