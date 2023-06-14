#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

void putchar0(short n); // 1 char 송신(Transmit)
void init_adc();
unsigned short read_adc();
void msec_delay(int n);
unsigned char gettingnumber();

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

int main()
{
    DDRA = 0xFF;
    PORTA = 0x80;
    init_adc();
    unsigned short value;
    unsigned short ADCtoSD;
    UCSR0A = 0;
    UBRR0H = 0;
    UBRR0L = 8;
    UCSR0B = 0x18;
    UCSR0C = 0x06;

    sei();

    while(1)
    {
        value = read_adc();
        // AD 변환 시작 및 결과 읽어오기(함수로 처리)
        ADCtoSD = value/256;
        putchar0(ADCtoSD);
        _delay_ms(DELAY); // 시간 지연
    }
}

ISR(USART0_RX_vect)
{
    cli();
    switch(gettingnumber())
    {
        case 0:
        PORTA = 0x01;
        break;
        case 1:
        PORTA = 0x02;
    }
}

void init_adc()
{
    ADMUX = 0x00;
    ADCSRA = 0x87;
}

unsigned short read_adc()
{
    unsigned char adc_low, adc_high;
    unsigned short value;
    ADCSRA |= 0x40; // ADC start conversion, ADSC = '1'
    while((ADCSA&0x10)!=0x10); // ADC 변환 완료 검사
    adc_low = ADCL; // 변환된 low 값
    adc_high = ADCH; // 변환된 high 값
    value = (adc_high << 8) | adc_low;
    // value는 hight 값 및 Low 연결 16bit
    return value;
}