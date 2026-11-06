#include <stdint.h>
#include "button_state.h"
#include "display_logic.h"

// --- 寄存器 ---
#define RCC_IOPENR   (*(volatile uint32_t *)0x4002102C)
#define RCC_APB1ENR  (*(volatile uint32_t *)0x40021038)
#define RCC_CSR      (*(volatile uint32_t *)0x40021050)
#define RCC_CR       (*(volatile uint32_t *)0x40021000)
#define RCC_CFGR     (*(volatile uint32_t *)0x4002100C)
#define RCC_CSR_RTCRST (1u << 19)
#define RCC_CSR_RTCEN  (1u << 18)
#define RCC_CSR_RTCSEL (3u << 16)
#define RCC_CSR_LSEDRV (3u << 11)
#define RCC_CSR_LSEBYP (1u << 10)
#define RCC_CSR_LSERDY (1u << 9)
#define RCC_CSR_LSEON  (1u << 8)
#define PWR_CR       (*(volatile uint32_t *)0x40007000)

#define GPIOA_MODER  (*(volatile uint32_t *)0x50000000)
#define GPIOA_OTYPER (*(volatile uint32_t *)0x50000004)
#define GPIOA_PUPDR  (*(volatile uint32_t *)0x5000000C)
#define GPIOA_IDR    (*(volatile uint32_t *)0x50000010)
#define GPIOA_ODR    (*(volatile uint32_t *)0x50000014)

#define RTC_TR       (*(volatile uint32_t *)0x40002800)
#define RTC_DR       (*(volatile uint32_t *)0x40002804)
#define RTC_ISR      (*(volatile uint32_t *)0x4000280C)
#define RTC_PRER     (*(volatile uint32_t *)0x40002810)
#define RTC_WPR      (*(volatile uint32_t *)0x40002824)
#define RTC_SSR      (*(volatile uint32_t *)0x40002828)
#define SYST_CSR     (*(volatile uint32_t *)0xE000E010)
#define SYST_RVR     (*(volatile uint32_t *)0xE000E014)
#define SYST_CVR     (*(volatile uint32_t *)0xE000E018)

#define LED_TOGGLE() (GPIOA_ODR ^= (1<<1))
#define SCL_H() (GPIOA_ODR |= (1<<9))
#define SCL_L() (GPIOA_ODR &= ~(1<<9))
#define SDA_H() (GPIOA_ODR |= (1<<10))
#define SDA_L() (GPIOA_ODR &= ~(1<<10))
#define SDA_READ() ((GPIOA_IDR & (1<<10)) ? 1 : 0)
#define I2C_DELAY_COUNT 4
#define BUTTON_WAKE_MASK (1u << 0)
#define BUTTON_SET_MASK  (1u << 4)

volatile uint32_t g_ms;
volatile uint32_t rtc_debug_rcc_csr, rtc_debug_prer, rtc_debug_ssr;

void SysTick_Handler(void) { g_ms++; }

static void Clock_Init_16MHz(void) {
    RCC_CR |= (1u << 8);                         // HSI16 ON (STM32L0 RCC_CR)
    while (!(RCC_CR & (1u << 10))) {}            // wait HSIRDY
    RCC_CFGR = (RCC_CFGR & ~3u) | 1u;            // SYSCLK = HSI16
    while ((RCC_CFGR & (3u << 2)) != (1u << 2)) {}
}

static void SysTick_Init_1ms(void) {
    SYST_CSR = 0;
    SYST_RVR = 16000u - 1u;
    SYST_CVR = 0;
    SYST_CSR = 7u;                               // processor clock, interrupt, enable
}

void delay(volatile uint32_t count) { while(count--); }

static uint8_t ButtonDown(uint32_t mask) {
    return (GPIOA_IDR & mask) == 0;
}

const uint8_t Font6x8[16][6] = {
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00}, {0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4B,0x31}, {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03}, {0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E}, {0x00,0x36,0x36,0x00,0x00},
    {0x7F,0x40,0x40,0x40,0x40}, {0x3E,0x41,0x49,0x49,0x32}, {0x7F,0x49,0x49,0x49,0x41},
    {0x00,0x60,0x60,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00}
};

typedef struct { uint8_t x, page, glyph, scale, offset; } DisplayJob;
static DisplayJob display_jobs[64];
static uint8_t display_head, display_tail;
volatile uint8_t oled_i2c_error;
static uint8_t progress_target, progress_current, progress_scan, progress_initial, progress_started;
static uint8_t drawn_time[4]={0xFF,0xFF,0xFF,0xFF};
static uint8_t drawn_seconds[2]={0xFF,0xFF};
static uint8_t colon_drawn;

static void OLED_QueueGlyph(uint8_t x, uint8_t page, uint8_t glyph, uint8_t scale) {
    if (!scale || (8u % scale)) return;
    for (uint8_t p=0; p<scale; p++) {
        uint8_t next=(uint8_t)((display_head+1u)&63u);
        if (next==display_tail) { oled_i2c_error=1; return; }
        display_jobs[display_head]=(DisplayJob){x,(uint8_t)(page+p),glyph,scale,p};
        display_head=next;
    }
}

static void OLED_ClearQueue(void) { display_head=0; display_tail=0; }

void I2C_Start() { SDA_H(); SCL_H(); delay(I2C_DELAY_COUNT); SDA_L(); delay(I2C_DELAY_COUNT); SCL_L(); }
void I2C_Stop()  { SCL_L(); SDA_L(); delay(I2C_DELAY_COUNT); SCL_H(); delay(I2C_DELAY_COUNT); SDA_H(); }
uint8_t I2C_WriteByte(uint8_t dat) {
    for(uint8_t i=0; i<8; i++) {
        if(dat & 0x80) SDA_H(); else SDA_L();
        SCL_H(); delay(I2C_DELAY_COUNT); SCL_L(); delay(I2C_DELAY_COUNT);
        dat <<= 1;
    }
    SDA_H(); SCL_H(); delay(I2C_DELAY_COUNT);
    uint8_t ack = SDA_READ();
    SCL_L(); delay(I2C_DELAY_COUNT);
    return ack;
}
static uint8_t OLED_Tx(uint8_t value) {
    if (I2C_WriteByte(value)) { oled_i2c_error=1; return 0; }
    return 1;
}
void OLED_WriteCmd(uint8_t cmd) {
    I2C_Start();
    if (!OLED_Tx(0x78) || !OLED_Tx(0x00) || !OLED_Tx(cmd)) oled_i2c_error=1;
    I2C_Stop();
}
static void OLED_WriteCmds(const uint8_t *cmds, uint8_t count) {
    I2C_Start();
    if (!OLED_Tx(0x78) || !OLED_Tx(0x00)) { I2C_Stop(); return; }
    while (count--) if (!OLED_Tx(*cmds++)) break;
    I2C_Stop();
}
void OLED_WriteData(uint8_t data) {
    I2C_Start();
    if (!OLED_Tx(0x78) || !OLED_Tx(0x40) || !OLED_Tx(data)) oled_i2c_error=1;
    I2C_Stop();
}
void OLED_WriteDataBuffer(const uint8_t *data, uint8_t length) {
    I2C_Start();
    if (!OLED_Tx(0x78) || !OLED_Tx(0x40)) { I2C_Stop(); return; }
    while (length--) if (!OLED_Tx(*data++)) break;
    I2C_Stop();
}
void OLED_WriteDataFill(uint8_t value, uint8_t length) {
    I2C_Start();
    if (!OLED_Tx(0x78) || !OLED_Tx(0x40)) { I2C_Stop(); return; }
    while (length--) if (!OLED_Tx(value)) break;
    I2C_Stop();
}
static uint8_t OLED_WriteAt(uint8_t page, uint8_t x, const uint8_t *data, uint8_t length) {
    I2C_Start();
    if (!OLED_Tx(0x78) || !OLED_Tx(0x00) || !OLED_Tx((uint8_t)(0xB0+page)) ||
        !OLED_Tx((uint8_t)(((x>>4)&0x0F)|0x10)) || !OLED_Tx((uint8_t)(x&0x0F))) {
        I2C_Stop(); return 0;
    }
    I2C_Start();
    if (!OLED_Tx(0x78) || !OLED_Tx(0x40)) { I2C_Stop(); return 0; }
    while (length--) if (!OLED_Tx(*data++)) { I2C_Stop(); return 0; }
    I2C_Stop();
    return 1;
}
void OLED_Init() {
    static const uint8_t init_cmds[] = {0xAE,0x20,0x00,0xB0,0xC8,0x00,0x10,0x40,0x81,0xCF,0xA1,0xA6,0xA8,0x3F,0xA4,0xD3,0x00,0xD5,0x80,0xD9,0xF1,0xDA,0x12,0xDB,0x40,0x8D,0x14,0xAF};
    OLED_WriteCmds(init_cmds, sizeof(init_cmds));
}
void OLED_ShowChar(uint8_t x, uint8_t page_y, uint8_t char_idx) {
    OLED_QueueGlyph(x,page_y,char_idx,1);
}
void OLED_ShowText(uint8_t x, uint8_t page_y, const uint8_t *text) {
    while (*text) {
        uint8_t idx;
        if (*text >= '0' && *text <= '9') idx = (uint8_t)(*text - '0');
        else if (*text == ':') idx = 10;
        else if (*text == 'L') idx = 11;
        else if (*text == 'S') idx = 12;
        else if (*text == 'E') idx = 13;
        else if (*text == '.') idx = 14;
        else { x += 6; text++; continue; }
        OLED_ShowChar(x, page_y, idx);
        x += 6;
        text++;
    }
}
uint8_t BCD_To_Dec(uint8_t value) {
    return (uint8_t)(((value >> 4) & 0x0F) * 10 + (value & 0x0F));
}
void OLED_ShowDate(uint32_t dr) {
    uint8_t year = BCD_To_Dec((uint8_t)(dr >> 16));
    uint8_t month = BCD_To_Dec((uint8_t)((dr >> 8) & 0x1F));
    uint8_t day = BCD_To_Dec((uint8_t)(dr & 0x3F));
    OLED_ShowText(2, 0, (const uint8_t *)"20");
    OLED_ShowChar(14, 0, (uint8_t)(year / 10));
    OLED_ShowChar(20, 0, (uint8_t)(year % 10));
    OLED_ShowText(26, 0, (const uint8_t *)".");
    OLED_ShowChar(32, 0, (uint8_t)(month / 10));
    OLED_ShowChar(38, 0, (uint8_t)(month % 10));
    OLED_ShowText(44, 0, (const uint8_t *)".");
    OLED_ShowChar(50, 0, (uint8_t)(day / 10));
    OLED_ShowChar(56, 0, (uint8_t)(day % 10));
    OLED_ShowText(96, 0, (const uint8_t *)"LSE");
}
void OLED_ShowProgress(uint8_t sec) {
    progress_target=ProgressTarget(sec);
    if (!progress_started) { progress_started=1; progress_initial=1; progress_scan=0; }
}
void OLED_DrawHeaderRule(void) {
    static const uint8_t cmds[] = {0xB1,0x10,0x02};
    OLED_WriteCmds(cmds, sizeof(cmds)); OLED_WriteDataFill(0x08, 124);
}
void OLED_ShowChar_Big(uint8_t x, uint8_t page_y, uint8_t char_idx, uint8_t scale) {
    OLED_QueueGlyph(x,page_y,char_idx,scale);
}
void OLED_ShowTime_Big(uint8_t h, uint8_t m) {
    uint8_t digits[4]={(uint8_t)((h>>4)&0x0F),(uint8_t)(h&0x0F),
                       (uint8_t)((m>>4)&0x0F),(uint8_t)(m&0x0F)};
    static const uint8_t x[4]={4,28,76,100};
    for(uint8_t i=0;i<4;i++) if(drawn_time[i]!=digits[i]) {
        drawn_time[i]=digits[i]; OLED_ShowChar_Big(x[i],2,digits[i],4);
    }
    if(!colon_drawn){colon_drawn=1;OLED_ShowChar_Big(52,2,10,4);}
}

static void OLED_ShowSeconds(uint8_t second) {
    uint8_t digits[2]={(uint8_t)(second/10u),(uint8_t)(second%10u)};
    static const uint8_t x[2]={4,16};
    for(uint8_t i=0;i<2;i++) if(drawn_seconds[i]!=digits[i]) {
        drawn_seconds[i]=digits[i]; OLED_ShowChar_Big(x[i],6,digits[i],2);
    }
}

static void OLED_ResetRendered(void) {
    for(uint8_t i=0;i<4;i++) drawn_time[i]=0xFF;
    drawn_seconds[0]=drawn_seconds[1]=0xFF;
    colon_drawn=0;
}

static uint8_t OLED_DisplayTask(void) {
    if(display_tail!=display_head) {
        DisplayJob job=display_jobs[display_tail];
        uint8_t data[24],count=0,bpp=(uint8_t)(8u/job.scale),sb=(uint8_t)(job.offset*bpp);
        for(uint8_t col=0;col<6;col++) {
            uint8_t bits=Font6x8[job.glyph][col],out=0;
            for(uint8_t b=0;b<bpp;b++) if(bits&(1u<<(sb+b)))
                for(uint8_t s=0;s<job.scale;s++) out|=(uint8_t)(1u<<(b*job.scale+s));
            for(uint8_t s=0;s<job.scale;s++) data[count++]=out;
        }
        (void)OLED_WriteAt(job.page,job.x,data,count);
        display_tail=(uint8_t)((display_tail+1u)&63u);
        return 1;
    }
    uint8_t start,length;
    if(progress_initial) {
        start=progress_scan; length=(uint8_t)((80u-start>8u)?8u:80u-start);
    } else if(progress_current<progress_target) {
        uint8_t delta=(uint8_t)(progress_target-progress_current);
        start=progress_current; length=(delta>8u)?8u:delta;
    } else if(progress_current>progress_target) {
        uint8_t delta=(uint8_t)(progress_current-progress_target);
        length=(delta>8u)?8u:delta;
        start=(uint8_t)(progress_current-length);
    } else return 0;
    uint8_t bytes[8];
    for(uint8_t i=0;i<length;i++) bytes[i]=ProgressByte((uint8_t)(start+i),progress_target);
    (void)OLED_WriteAt(7,(uint8_t)(34u+start),bytes,length);
    if(progress_initial) {
        progress_scan=(uint8_t)(progress_scan+length);
        if(progress_scan>=80) { progress_initial=0; progress_scan=0; progress_current=progress_target; }
    } else if(progress_current<progress_target) progress_current=(uint8_t)(progress_current+length);
    else progress_current=(uint8_t)(progress_current-length);
    return 1;
}

/* --- RTC 初始化：使用外部 LSE --- */
uint8_t RTC_Init_LSE(void) {
    volatile uint32_t timeout;
    uint8_t initialize_calendar = 0;
    uint8_t initialize_date = 0;

    // 1. 使能 PWR 和备份域访问
    RCC_APB1ENR |= (1 << 28);       // PWREN
    PWR_CR |= (1 << 8);             // DBP = 1
    for (volatile int i = 0; i < 500; i++);

    // RTC 时钟源一经选择便锁定；仅在尚未选择 LSE 时复位备份域。
    if ((RCC_CSR & RCC_CSR_RTCSEL) != (1u << 16)) {
        RCC_CSR |= RCC_CSR_RTCRST;
        for (volatile int i = 0; i < 500; i++);
        RCC_CSR &= ~RCC_CSR_RTCRST;
        for (volatile int i = 0; i < 500; i++);
        initialize_calendar = 1;
    }

    // 试用中高驱动档；驱动档仅能在 LSE 关闭时修改。
    if ((RCC_CSR & RCC_CSR_LSEON) &&
        (RCC_CSR & RCC_CSR_LSEDRV) != (2u << 11)) {
        RCC_CSR &= ~RCC_CSR_LSEON;
        timeout = 0xFFFFFF;
        while ((RCC_CSR & RCC_CSR_LSERDY) && --timeout) {}
        if (!timeout) return 6;
    }
    if (!(RCC_CSR & RCC_CSR_LSEON)) {
        RCC_CSR &= ~RCC_CSR_LSEBYP;
        RCC_CSR &= ~RCC_CSR_LSEDRV;
        RCC_CSR |= (2u << 11);
        RCC_CSR |= RCC_CSR_LSEON;
    }

    // 等待 LSE 就绪
    timeout = 0xFFFFFF;
    while (!(RCC_CSR & RCC_CSR_LSERDY) && --timeout);
    if (timeout == 0) return 1;

    // 选择并使能 RTC 时钟；已在使用 LSE 时保留原有计时状态。
    if ((RCC_CSR & RCC_CSR_RTCSEL) == 0) {
        RCC_CSR |= (1u << 16);      // RTCSEL = 01 (LSE)
    }
    RCC_CSR |= RCC_CSR_RTCEN;

    // 同步 RTC 寄存器后，仅在日历尚未初始化時设置初始时间。
    RTC_ISR &= ~(1u << 5);
    timeout = 0xFFFFFF;
    while (!(RTC_ISR & (1 << 5)) && --timeout);
    if (timeout == 0) return 4;

    if (!initialize_calendar && (RTC_ISR & (1u << 4))) {
        uint32_t dr = RTC_DR;
        if ((dr & 0x1F00u) != 0 && (dr & 0x3Fu) != 0) return 0;
        initialize_date = 1;
    } else {
        initialize_calendar = 1;
    }

    // 解锁 RTC，进入初始化模式
    RTC_WPR = 0xCA; RTC_WPR = 0x53;
    RTC_ISR |= (1 << 7);            // INIT = 1
    timeout = 0xFFFFFF;
    while (!(RTC_ISR & (1 << 6)) && --timeout);   // 等 INITF
    if (timeout == 0) return 5;

    if (initialize_calendar) {
        // LSE 32768 / ((127 + 1) * (255 + 1)) = 1 Hz。
        RTC_PRER = (127 << 16) | 255;
        RTC_TR = (0x16u << 16) | (0x20u << 8); // 首次初始化为 16:20:00 (BCD)
    }
    if (initialize_calendar || initialize_date) {
        RTC_DR = (0x26u << 16) | (7u << 13) | (0x10u << 8) | 0x18u;
    }

    // 退出初始化模式并等待同步完成。
    RTC_ISR &= ~(1 << 7);           // INIT = 0
    timeout = 0xFFFFFF;
    while ((RTC_ISR & (1 << 6)) && --timeout);
    RTC_ISR &= ~(1u << 5);
    timeout = 0xFFFFFF;
    while (!(RTC_ISR & (1u << 5)) && --timeout);
    RTC_WPR = 0xFF;
    if (timeout == 0) return 4;

    return 0;
}


static uint8_t Dec_To_BCD(uint8_t v) { return (uint8_t)(((v / 10u) << 4) | (v % 10u)); }
static void RTC_WriteCalendar(uint8_t year, uint8_t month, uint8_t day, uint8_t weekday, uint8_t hour, uint8_t minute, uint8_t second) {
    volatile uint32_t timeout = 0xFFFFFF; RTC_WPR=0xCA; RTC_WPR=0x53; RTC_ISR |= (1u<<7);
    while (!(RTC_ISR&(1u<<6)) && --timeout) {}
    if (timeout) { RTC_TR=((uint32_t)Dec_To_BCD(hour)<<16)|((uint32_t)Dec_To_BCD(minute)<<8)|Dec_To_BCD(second); RTC_DR=((uint32_t)Dec_To_BCD(year)<<16)|((uint32_t)(weekday&7u)<<13)|((uint32_t)Dec_To_BCD(month)<<8)|Dec_To_BCD(day); RTC_ISR&=~(1u<<7); timeout=0xFFFFFF; while((RTC_ISR&(1u<<6))&&--timeout){} RTC_ISR&=~(1u<<5); timeout=0xFFFFFF; while(!(RTC_ISR&(1u<<5))&&--timeout){} } RTC_WPR=0xFF;
}
static void RTC_ReadSnapshot(uint32_t *tr, uint32_t *dr, uint32_t *ssr) {
    uint32_t first, last;
    do {
        first = RTC_TR;
        *dr = RTC_DR;
        *ssr = RTC_SSR;
        last = RTC_TR;
    } while (first != last);
    *tr = first;
}

static void RTC_DecodeSnapshot(uint32_t tr, uint32_t dr,
                               uint8_t *year,uint8_t *month,uint8_t *day,uint8_t *weekday,
                               uint8_t *hour,uint8_t *minute,uint8_t *second) {
    *year=BCD_To_Dec((uint8_t)(dr>>16)); *month=BCD_To_Dec((uint8_t)((dr>>8)&0x1F));
    *day=BCD_To_Dec((uint8_t)(dr&0x3F)); *weekday=(uint8_t)((dr>>13)&7);
    *hour=BCD_To_Dec((uint8_t)(tr>>16)); *minute=BCD_To_Dec((uint8_t)(tr>>8));
    *second=BCD_To_Dec((uint8_t)tr);
}

static void OLED_BlinkSettingField(uint8_t item, uint8_t visible,
                                   uint8_t year, uint8_t month, uint8_t day,
                                   uint8_t weekday, uint8_t hour, uint8_t minute, uint8_t second) {
    uint8_t a, c;
    if (item == 0) { a = year / 10; c = year % 10; OLED_ShowChar(14,0,visible?a:15); OLED_ShowChar(20,0,visible?c:15); }
    else if (item == 1) { a = month / 10; c = month % 10; OLED_ShowChar(32,0,visible?a:15); OLED_ShowChar(38,0,visible?c:15); }
    else if (item == 2) { a = day / 10; c = day % 10; OLED_ShowChar(50,0,visible?a:15); OLED_ShowChar(56,0,visible?c:15); }
    else if (item == 3) { OLED_ShowChar(96,0,15); OLED_ShowChar(102,0,visible?weekday:15); }
    else if (item == 4) { a = hour / 10; c = hour % 10; OLED_ShowChar_Big(4,2,visible?a:15,4); OLED_ShowChar_Big(28,2,visible?c:15,4); }
    else if (item == 5) { a = minute / 10; c = minute % 10; OLED_ShowChar_Big(76,2,visible?a:15,4); OLED_ShowChar_Big(100,2,visible?c:15,4); }
    else { a = second / 10; c = second % 10; OLED_ShowChar_Big(4,6,visible?a:15,2); OLED_ShowChar_Big(16,6,visible?c:15,2); }
}
static void OLED_Sleep(void){OLED_WriteCmd(0xAE);} static void OLED_Wake(void){OLED_WriteCmd(0xAF);}
static uint8_t DateDays(uint8_t y,uint8_t m){static const uint8_t d[12]={31,28,31,30,31,30,31,31,30,31,30,31};uint8_t n=d[(m-1)%12];if(m==2&&(y%4)==0)n=29;return n;}

int main(void) {
    Clock_Init_16MHz();
    SysTick_Init_1ms();
    RCC_IOPENR|=(1<<0); GPIOA_MODER&=~((3u<<0)|(3u<<2)|(3u<<8)); GPIOA_MODER|=(1u<<2); GPIOA_MODER&=~((3u<<18)|(3u<<20)); GPIOA_MODER|=((1u<<18)|(1u<<20)); GPIOA_OTYPER|=(1<<9)|(1<<10); GPIOA_PUPDR&=~((3u<<0)|(3u<<8)|(3u<<18)|(3u<<20)); GPIOA_PUPDR|=(1u<<0)|(1u<<8)|(1u<<18)|(1u<<20);
    for(volatile int i=0;i<2000000;i++);
    OLED_Init();
    for(uint8_t i=0;i<8;i++){
        uint8_t cmds[] = {(uint8_t)(0xB0+i),0x00,0x10};
        OLED_WriteCmds(cmds, sizeof(cmds)); OLED_WriteDataFill(0,128);
    }
    OLED_DrawHeaderRule();
    uint8_t err=RTC_Init_LSE(); if(err){OLED_ShowChar(0,0,10);OLED_ShowChar(8,0,err);while(1){LED_TOGGLE();delay(500000);}}
    rtc_debug_rcc_csr=RCC_CSR; rtc_debug_prer=RTC_PRER;
    uint8_t last_sec=0xFF,last_day=0xFF,setting=0,asleep=0,item=0,last_blink=0xFF;
    uint8_t edit_y=0,edit_mo=1,edit_da=1,edit_wd=1,edit_h=0,edit_mi=0,edit_se=0,edit_se_changed=0;
    uint32_t last_key_sample=0;
    static ButtonState wake_button, set_button;
    while(1) {
        uint32_t tr,dr,ssr;
        uint8_t y,mo,da,wd,h,mi,se;
        RTC_ReadSnapshot(&tr,&dr,&ssr);
        rtc_debug_ssr=ssr;
        RTC_DecodeSnapshot(tr,dr,&y,&mo,&da,&wd,&h,&mi,&se);
        uint32_t now=g_ms;
        uint8_t wake_evt=0,set_evt=0;
        if ((uint32_t)(now-last_key_sample)>=5u) {
            last_key_sample=now;
            wake_evt=Key_Update(&wake_button,ButtonDown(BUTTON_WAKE_MASK),now);
            set_evt=Key_Update(&set_button,ButtonDown(BUTTON_SET_MASK),now);
        }

        if (asleep) {
            if (wake_evt==1) { asleep=0; OLED_Wake(); OLED_ResetRendered(); last_sec=0xFF; last_day=0xFF; }
            __asm volatile("wfi");
            continue;
        }
        if (wake_evt==1) { OLED_ClearQueue(); OLED_Sleep(); asleep=1; continue; }

        if (!setting && set_evt==2) {
            OLED_ClearQueue();
            setting=1; item=0;
            edit_y=y; edit_mo=mo; edit_da=da; edit_wd=wd; edit_h=h; edit_mi=mi; edit_se=se; edit_se_changed=0;
            last_day=0xFF; last_blink=0xFF;
        } else if (setting && set_evt) {
            if (set_evt==1) {
                if(item==0)edit_y=(edit_y>=99)?0:edit_y+1;
                else if(item==1)edit_mo=(edit_mo>=12)?1:edit_mo+1;
                else if(item==2){uint8_t mx=DateDays(edit_y,edit_mo);edit_da=(edit_da>=mx)?1:edit_da+1;}
                else if(item==3)edit_wd=(edit_wd>=7)?1:edit_wd+1;
                else if(item==4)edit_h=(edit_h>=23)?0:edit_h+1;
                else if(item==5)edit_mi=(edit_mi>=59)?0:edit_mi+1;
                else {edit_se=0;edit_se_changed=1;}
                if(item<3)last_day=0xFF;
                last_blink=0xFF;
            } else if(item>=6) {
                OLED_ClearQueue();
                setting=0; item=0;
                RTC_WriteCalendar(edit_y,edit_mo,edit_da,edit_wd,edit_h,edit_mi,edit_se_changed?edit_se:se);
                last_sec=0xFF; last_day=0xFF; OLED_ResetRendered();
            } else { item++; last_blink=0xFF; }
        }

        if(setting && set_evt==2 && item<6) { OLED_ClearQueue(); last_blink=0xFF; }
        uint8_t show_da=setting?edit_da:da;
        uint8_t show_y=setting?edit_y:y,show_mo=setting?edit_mo:mo;
        uint8_t show_h=setting?edit_h:h,show_mi=setting?edit_mi:mi;
        uint8_t show_se=(setting&&edit_se_changed)?edit_se:se;
        if(se!=last_sec) { last_sec=se; LED_TOGGLE(); }
        OLED_ShowTime_Big(Dec_To_BCD(show_h),Dec_To_BCD(show_mi));
        OLED_ShowSeconds(show_se);
        OLED_ShowProgress(show_se);
        if(show_da!=last_day) {
            uint32_t display_dr=((uint32_t)Dec_To_BCD(show_y)<<16)|((uint32_t)(setting?edit_wd:wd)<<13)|
                                ((uint32_t)Dec_To_BCD(show_mo)<<8)|Dec_To_BCD(show_da);
            last_day=show_da; OLED_ShowDate(display_dr);
        }
        if(setting) {
            uint8_t blink=(uint8_t)(((now/500u)&1u));
            if(blink!=last_blink){last_blink=blink;OLED_BlinkSettingField(item,blink,edit_y,edit_mo,edit_da,edit_wd,edit_h,edit_mi,show_se);}
        }
        (void)OLED_DisplayTask();
        __asm volatile("wfi");
    }
}

