//任务写有结构体   ——》保存上次的状态   按钮按下需要发生的回调函数  检测按钮的回调 主要检测按钮的替换
#ifndef OSCILLOSCOPE_KEYS_KEY_H
#define OSCILLOSCOPE_KEYS_KEY_H
#include "main.h"
#include <stdint.h>
//决定用队列还是信号量
// 路由宏
#define ROUTE_QUEUE   0
#define ROUTE_SEM     1
typedef struct{
     //检测按钮的回调 主要检测按钮的替换
    GPIO_PinState (*keyReadPin)(void);
    void (*ClickCallback)(void* arg);
    void* arg;//回调参数 这什么都有可能
} InitkeyTyped;//专门放回调的
typedef struct{
    uint8_t rounte;//路径  是记信号（几需要单独的任务不卡在队列的）  还是队列
    GPIO_PinState pre;
    InitkeyTyped Init;//存函数信息
    SemaphoreHandle_t xSem;
}key_Handle;




void vkeyOneSca(key_Handle* key);
void keyInit(key_Handle* key);



typedef void (*Vjob_fuc)(void* arg);
typedef struct{
    
    Vjob_fuc xfu;
    void* arg;//函数的参数
}job_t;
extern QueueHandle_t xKeysQue;//记得在负责所有键检测 需要为创建的队列给大小  别忘了
/*KEY1 -> PC9 -> IPU
KEY2 -> PA8 -> IPU
KEY3 -> PC15 ->IPU
KEY4 -> PC0 ->IPU
KEYU -> PC13 ->IPU
KEYD -> PC14 ->IPU*/
#define KEY1PORT GPIOC
#define  KEY1PIN GPIO_PIN_9

#define KEY2PORT GPIOA
#define KEY2PIN GPIO_PIN_8

#define KEY3PORT GPIOC
#define KEY3PIN GPIO_PIN_15


#define KEY4PORT GPIOC
#define KEY4PIN GPIO_PIN_0

#define KEYU GPIOC
#define KEYUPIN GPIO_PIN_13

#define KEYD GPIOC
#define KEYPIN GPIO_PIN_14
#endif