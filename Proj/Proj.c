#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#define DELAY_TIME 20
static unsigned int queue[20];
static unsigned int left, right;
// 1. LED setting
void LED setting()
{
    PORTA = 0xFF; // A 포트 출력
}

// 2. Button setting
void init_button()
{
    PORTE = 0x00; // 스위치 입력
    EICRB = 0x0A; // 0b 0000 1010
    EIMSK = 0x30; // 0b 0011 0000
}

// 3. 7-seg setting
void init_7seg()
{
    
}

// 4. bluetooth setting
void putchar0(short n); // 1 char 송신
unsigned char gettingnumber(); // 1 char 수신

void init_bluetooth()
{
    UBRR0H = 0;
    UBRR0L = 0;
    UCSR0B = 0x18;
    UCSR0C = 0x06;
}

// 5. ADC setting
void init_ADC()
{
    ADMUX = 0x00;
    ADCSRA = 0x87;
}

unsigned short read_adc();

// 6. PWM setting => Fast PWM 사용
void init_PWM()
{
    TCCR2 = (1 << WGM01) | (1 << WGM00) | (2 << COM00); // Fast PWM 모드, 비교 일치 떄 동작모드
    TCRR2 |= (2 << CS00); // 분주비 8 설정
    OCR2 = duty;
}

// 인터럽트
ISR(INT4_vect)
{
    while(~PINE & 0x10); // 버튼 누르기 기다림
    msec_delay(DELAY_TIME);
    EIFR = (1 << 4);
}

ISR(INT5_vect)
{
    while(~PINE & 0x20); 
    msec_delay(DELAY_TIME);
    EIFR = (1 << 5);
}

// 블루투스 입력
ISR(USART0_RX_vect)
{
    cli();
    switch(gettingnumber())
    {

    }
}

int main()
{
    sei(); // 전역 인터럽트 허용
    while(1)
    {
        
    }
    return 0;
}

// 함수 정의 

void msec_delay(int msec)
{
    for(;msec>0;--msec)
    {
        for(int i=0;i<1600;++i)
        {
            asm("nop"::);
            asm("nop"::);
        }
    }
}

unsigned char gettingnumber(void)
{
    while(!(UCSR0A & 0x80));
    return UDR0;
}

void putchar0(short n)
{
    while(!(UCSR0A & 0x20));
    UDR0 = n; // 1 char 전달
}