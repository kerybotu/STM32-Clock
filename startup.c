#include <stdint.h>

extern int  main(void);
extern uint32_t _estack;

/* 由 linker.ld 提供 */
extern uint32_t _sidata;   /* .data 在 Flash 的起始（LMA） */
extern uint32_t _sdata;    /* .data 在 RAM 的起始（VMA）  */
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

void Reset_Handler(void);
void Default_Handler(void);

/* ---- 复位入口：先做 C 运行时初始化，再调 main ---- */
void Reset_Handler(void)
{
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;

    /* 1. 把 .data 的初值从 Flash 拷到 RAM
     *    用 volatile 访问，防止 -O2 把循环优化成对 memcpy 的调用 */
    while (dst < &_edata) {
        *(volatile uint32_t *)dst = *(volatile uint32_t *)src;
        dst++;
        src++;
    }

    /* 2. 清零 .bss（同样用 volatile 防止被优化成 memset） */
    dst = &_sbss;
    while (dst < &_ebss) {
        *(volatile uint32_t *)dst = 0;
        dst++;
    }

    /* 3. 进入 main */
    main();
    while (1) { }
}

/* ---- 默认异常/中断处理 ---- */
void Default_Handler(void)
{
    while (1) { }
}

/* ---- 向量表 ---- */
__attribute__((section(".isr_vector"), used))
void (* const g_pfnVectors[])(void) = {
    (void (*)(void))(&_estack),   /* 0  初始 MSP */
    Reset_Handler,                /* 1  Reset   */
    Default_Handler,              /* 2  NMI     */
    Default_Handler,              /* 3  HardFault */
    /* ... 后续与之前版本相同 ... */
};