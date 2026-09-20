#ifndef DMM_Gui_H
#define DMM_Gui_H
#include "main.h"

typedef enum
{
	eDMMRange2V,      // 2V电压挡
	eDMMRange10V,     // 10V电压挡
	eDMMRange50V,     // 50V电压挡
	eDMMRange1kOhm,   // 1k电阻档
	eDMMRange10kOhm,  // 10k电阻档
	eDMMRange100kOhm, // 100k电阻档
	eDMMRange1MOhm,   // 1M电阻档
} DMMRange_t;

typedef struct
{
	DMMRange_t eDMMRange; // 万用表的挡位
	float fDMMValue; // 万用表的测量结果
	float fVpwr; // 稳压电源模块的电压
} DMMData_t;

void vDMMInit(void);
DMMData_t xDMMGetData(void);

#endif