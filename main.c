#include <stdint.h>

// --- 寄存器 ---
#define RCC_IOPENR   (*(volatile uint32_t *)0x4002102C)
#define RCC_APB1ENR  (*(volatile uint32_t *)0x40021038)
#define RCC_CSR      (*(volatile uint32_t *)0x40021050)
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

#define LED_TOGGLE() (GPIOA_ODR ^= (1<<1))
#define SCL_H() (GPIOA_ODR |= (1<<9))
#define SCL_L() (GPIOA_ODR &= ~(1<<9))
#define SDA_H() (GPIOA_ODR |= (1<<10))
#define SDA_L() (GPIOA_ODR &= ~(1<<10))
#define SDA_READ() ((GPIOA_IDR & (1<<10)) ? 1 : 0)
#define I2C_DELAY_COUNT 1
#define BUTTON_WAKE_MASK (1u << 0)
#define BUTTON_SET_MASK  (1u << 4)

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
void OLED_WriteCmd(uint8_t cmd) {
    I2C_Start(); I2C_WriteByte(0x78); I2C_WriteByte(0x00); I2C_WriteByte(cmd); I2C_Stop();
}
static void OLED_WriteCmds(const uint8_t *cmds, uint8_t count) {
    I2C_Start(); I2C_WriteByte(0x78); I2C_WriteByte(0x00);
    while (count--) I2C_WriteByte(*cmds++);
    I2C_Stop();
}
void OLED_WriteData(uint8_t data) {
    I2C_Start(); I2C_WriteByte(0x78); I2C_WriteByte(0x40); I2C_WriteByte(data); I2C_Stop();
}
void OLED_WriteDataBuffer(const uint8_t *data, uint8_t length) {
    I2C_Start(); I2C_WriteByte(0x78); I2C_WriteByte(0x40);
    while (length--) I2C_WriteByte(*data++);
    I2C_Stop();
}
void OLED_WriteDataFill(uint8_t value, uint8_t length) {
    I2C_Start(); I2C_WriteByte(0x78); I2C_WriteByte(0x40);
    while (length--) I2C_WriteByte(value);
    I2C_Stop();
}
void OLED_Init() {
    static const uint8_t init_cmds[] = {0xAE,0x20,0x00,0xB0,0xC8,0x00,0x10,0x40,0x81,0xCF,0xA1,0xA6,0xA8,0x3F,0xA4,0xD3,0x00,0xD5,0x80,0xD9,0xF1,0xDA,0x12,0xDB,0x40,0x8D,0x14,0xAF};
    OLED_WriteCmds(init_cmds, sizeof(init_cmds));
}
void OLED_ShowChar(uint8_t x, uint8_t page_y, uint8_t char_idx) {
    uint8_t cmds[] = {(uint8_t)(0xB0 + page_y), (uint8_t)(((x & 0xF0) >> 4) | 0x10), (uint8_t)(x & 0x0F)};
    OLED_WriteCmds(cmds, sizeof(cmds));
    I2C_Start(); I2C_WriteByte(0x78); I2C_WriteByte(0x40);
    for(uint8_t i=0; i<6; i++) I2C_WriteByte(Font6x8[char_idx][i]);
    I2C_Stop();
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
    uint8_t row[80];
    uint8_t filled = (uint8_t)(((uint16_t)sec * 80) / 60);
    for (uint8_t i = 0; i < 80; i++) row[i] = i < filled ? 0x3C : 0x24;
    row[79] = 0x7E;                  // 右端封口
    uint8_t cmds[] = {0xB7, (uint8_t)(0x10 | ((34 >> 4) & 0x0F)), (uint8_t)(34 & 0x0F)};
    OLED_WriteCmds(cmds, sizeof(cmds)); OLED_WriteDataBuffer(row, 80);
}
void OLED_DrawHeaderRule(void) {
    static const uint8_t cmds[] = {0xB1,0x10,0x02};
    OLED_WriteCmds(cmds, sizeof(cmds)); OLED_WriteDataFill(0x08, 124);
}
void OLED_ShowChar_Big(uint8_t x, uint8_t page_y, uint8_t char_idx, uint8_t scale) {
    if (scale == 0 || (8 % scale) != 0) return;
    uint8_t bpp = 8 / scale;
    for (uint8_t page = 0; page < scale; page++) {
        uint8_t sb = page * bpp;
        uint8_t cmds[] = {(uint8_t)(0xB0 + page_y + page), (uint8_t)(((x & 0xF0) >> 4) | 0x10), (uint8_t)(x & 0x0F)};
        OLED_WriteCmds(cmds, sizeof(cmds)); I2C_Start(); I2C_WriteByte(0x78); I2C_WriteByte(0x40);
        for (uint8_t col = 0; col < 6; col++) {
            uint8_t cd = Font6x8[char_idx][col];
            uint8_t ob = 0;
            for (uint8_t b = 0; b < bpp; b++) {
                if (cd & (1 << (sb + b))) {
                    for (uint8_t s = 0; s < scale; s++) ob |= (1 << (b * scale + s));
                }
            }
            for (uint8_t s = 0; s < scale; s++) I2C_WriteByte(ob);
        }
        I2C_Stop();
    }
}
void OLED_ShowTime_Big(uint8_t h, uint8_t m) {
    uint8_t sc = 4;
    OLED_ShowChar_Big(  4, 2, (h >> 4) & 0x0F, sc);
    OLED_ShowChar_Big( 28, 2,  h       & 0x0F, sc);
    OLED_ShowChar_Big( 52, 2, 10,              sc);
    OLED_ShowChar_Big( 76, 2, (m >> 4) & 0x0F, sc);
    OLED_ShowChar_Big(100, 2,  m       & 0x0F, sc);
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

    // LSE 使用外部 32.768 kHz 晶体；驱动等级只在振荡器关闭时设置。
    if (!(RCC_CSR & RCC_CSR_LSEON)) {
        RCC_CSR &= ~RCC_CSR_LSEBYP;
        RCC_CSR &= ~RCC_CSR_LSEDRV;
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
static void RTC_ReadCalendar(uint8_t *year,uint8_t *month,uint8_t *day,uint8_t *weekday,uint8_t *hour,uint8_t *minute,uint8_t *second) { uint32_t a,b,d; do{a=RTC_TR;b=RTC_TR;}while(a!=b); d=RTC_DR; *year=BCD_To_Dec((uint8_t)(d>>16)); *month=BCD_To_Dec((uint8_t)((d>>8)&0x1F)); *day=BCD_To_Dec((uint8_t)(d&0x3F)); *weekday=(uint8_t)((d>>13)&7); *hour=BCD_To_Dec((uint8_t)(a>>16)); *minute=BCD_To_Dec((uint8_t)(a>>8)); *second=BCD_To_Dec((uint8_t)a); }
typedef struct {
    uint8_t candidate, pressed, stable, long_sent;
    uint16_t last_phase;
    uint32_t held_ticks;
} ButtonState;

/* RTC runs at 256 subsecond ticks/s (PREDIV_S=255). */
static uint16_t RTC_Phase(uint8_t second) {
    return (uint16_t)(((uint16_t)second * 256u) + (255u - (RTC_SSR & 0xFFu)));
}

/* Non-blocking event: 1=short release, 2=long press at 1.5 s. */
static uint8_t ButtonUpdate(ButtonState *b, uint32_t mask, uint16_t phase) {
    uint8_t raw = ButtonDown(mask);
    if (raw != b->candidate) { b->candidate = raw; b->stable = 0; return 0; }
    if (b->stable < 4) { b->stable++; return 0; }
    if (raw && !b->pressed) {
        b->pressed = 1; b->long_sent = 0; b->held_ticks = 0; b->last_phase = phase;
        return 0;
    }
    if (!raw && b->pressed) {
        b->pressed = 0;
        if (!b->long_sent) return 1;
        return 0;
    }
    if (raw && b->pressed) {
        uint16_t elapsed = (uint16_t)((phase + 15360u - b->last_phase) % 15360u);
        b->held_ticks += elapsed;
        b->last_phase = phase;
        if (!b->long_sent && b->held_ticks >= 384u) { b->long_sent = 1; return 2; }
    }
    return 0;
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
    RCC_IOPENR|=(1<<0); GPIOA_MODER&=~((3u<<0)|(3u<<2)|(3u<<8)); GPIOA_MODER|=(1u<<2); GPIOA_MODER&=~((3u<<18)|(3u<<20)); GPIOA_MODER|=((1u<<18)|(1u<<20)); GPIOA_OTYPER|=(1<<9)|(1<<10); GPIOA_PUPDR&=~((3u<<0)|(3u<<8)|(3u<<18)|(3u<<20)); GPIOA_PUPDR|=(1u<<0)|(1u<<8)|(1u<<18)|(1u<<20);
    for(volatile int i=0;i<2000000;i++);
    OLED_Init();
    for(uint8_t i=0;i<8;i++){
        uint8_t cmds[] = {(uint8_t)(0xB0+i),0x00,0x10};
        OLED_WriteCmds(cmds, sizeof(cmds)); OLED_WriteDataFill(0,128);
    }
    OLED_DrawHeaderRule();
    uint8_t err=RTC_Init_LSE(); if(err){OLED_ShowChar(0,0,10);OLED_ShowChar(8,0,err);while(1){LED_TOGGLE();delay(500000);}}
    uint8_t last_sec=0xFF,last_min=0xFF,last_day=0xFF,setting=0,asleep=0,item=0,last_blink=0xFF;
    ButtonState wake_button, set_button;
    wake_button.candidate=0; wake_button.pressed=0; wake_button.stable=0; wake_button.long_sent=0; wake_button.last_phase=0; wake_button.held_ticks=0;
    set_button.candidate=0; set_button.pressed=0; set_button.stable=0; set_button.long_sent=0; set_button.last_phase=0; set_button.held_ticks=0;
    while(1) {
        uint8_t y,mo,da,wd,h,mi,se;
        RTC_ReadCalendar(&y,&mo,&da,&wd,&h,&mi,&se);
        uint16_t phase=RTC_Phase(se);
        uint8_t wake_evt=ButtonUpdate(&wake_button,BUTTON_WAKE_MASK,phase);
        uint8_t set_evt=ButtonUpdate(&set_button,BUTTON_SET_MASK,phase);

        if (asleep) {
            if (wake_evt==1) { asleep=0; OLED_Wake(); OLED_DrawHeaderRule(); last_sec=0xFF; last_min=0xFF; last_day=0xFF; }
            delay(20000);
            continue;
        }
        if (wake_evt==1) { OLED_Sleep(); asleep=1; delay(20000); continue; }

        if (!setting && set_evt==2) {
            setting=1; item=0; se=0; RTC_WriteCalendar(y,mo,da,wd,h,mi,se);
            last_sec=0xFF; last_min=0xFF; last_day=0xFF; last_blink=0xFF;
        } else if (setting && set_evt) {
            if (set_evt==1) {
                if(item==0)y=(y>=99)?0:y+1;
                else if(item==1)mo=(mo>=12)?1:mo+1;
                else if(item==2){uint8_t mx=DateDays(y,mo);da=(da>=mx)?1:da+1;}
                else if(item==3)wd=(wd>=7)?1:wd+1;
                else if(item==4)h=(h>=23)?0:h+1;
                else if(item==5)mi=(mi>=59)?0:mi+1;
                else se=0;
                RTC_WriteCalendar(y,mo,da,wd,h,mi,se);
                last_sec=0xFF; last_min=0xFF; last_day=0xFF;
            } else if(item>=6) {
                setting=0; item=0; RTC_WriteCalendar(y,mo,da,wd,h,mi,se);
                last_sec=0xFF; last_min=0xFF; last_day=0xFF;
            } else { item++; last_blink=0xFF; }
        }

        if(se!=last_sec) {
            last_sec=se; LED_TOGGLE();
            if(mi!=last_min){last_min=mi;OLED_ShowTime_Big(Dec_To_BCD(h),Dec_To_BCD(mi));}
            OLED_ShowChar_Big(4,6,se/10,2); OLED_ShowChar_Big(16,6,se%10,2); OLED_ShowProgress(se);
            if(da!=last_day){last_day=da;OLED_ShowDate(RTC_DR);}
        }
        if(setting) {
            uint8_t blink=(uint8_t)((phase/128u)&1u);
            if(blink!=last_blink){last_blink=blink;OLED_BlinkSettingField(item,blink,y,mo,da,wd,h,mi,se);}
        }
        delay(20000);
    }
}

