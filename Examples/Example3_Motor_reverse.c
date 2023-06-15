#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

static unsigned char pattern[2]
={0x80,0x40};

ISR(INT4_vect)
{
   _delay_ms(20);
   while(~PINE & 0x10);
   _delay_ms(20);
   while(1)
   {
      PORTB = pattern[0];
      sei();
      EIMSK = (1<<INT5);
   }
}

ISR(INT5_vect)
{
   _delay_ms(20);
   while(~PINE & 0x10);
   _delay_ms(20);
   while(1)
   {
      PORTB = pattern[1];
      sei();
      EIMSK = (1<<INT4);
   }
}

int main()
{
   DDRB |= (1<<DDB7);
   DDRE = 0x00;
   EICRB = 0x0A; // 0b 1100 0000
   EIMSK = (1<<INT4)|(1<<INT5);
   sei();
   TCCR2 = 0x68; // 0b 0110 1000 
   TCCR2 |= (2<<CS00); // 분주비 8로 설정
   OCR2 = 128;
   while(1)
   {
      PORTB = 0;
   }
}