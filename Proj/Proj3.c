#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#define F_CPU 16000000UL // CPU 시간
#define DELAY_TIME 20 // DELAY 시간
#define WAIT_TIME 5000 // 문을 닫는데 기다리는 시간
#define DISTANCE_PER_FLOOR 1000 // 실제 이동해야 하는 거리
#define MAX_HEIGHT 10 // 엘리베이터의 높이
#define QUEUE_SIZE 20

unsigned char obj_floor_state; // 0b00000000으로 값을 정한다.
unsigned int queue[QUEUE_SIZE]; //
unsigned int q_size;
unsigned char is_open, is_move, is_up, has_open;
unsigned int duty = 0; // duty 값
unsigned int current_floor, diff_floor, object_floor, distance; // 목표 층과 현재 층
const unsigned char seven_seg_digits_decode_gfedcba[75]= {
	/*  0     1     2     3     4     5     6     7     8     9     :     ;     */
	0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F, 0x00, 0x00,
	/*  <     =     >     ?     @     A     B     C     D     E     F     G     */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71, 0x3D,
	/*  H     I     J     K     L     M     N     O     P     Q     R     S     */
	0x76, 0x30, 0x1E, 0x75, 0x38, 0x55, 0x54, 0x5C, 0x73, 0x67, 0x50, 0x6D,
	/*  T     U     V     W     X     Y     Z     [     \     ]     ^     _     */
	0x78, 0x3E, 0x1C, 0x1D, 0x64, 0x6E, 0x5B, 0x00, 0x00, 0x00, 0x00, 0x00,
	/*  `     a     b     c     d     e     f     g     h     i     j     k     */
	0x00, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71, 0x3D, 0x76, 0x30, 0x1E, 0x75,
	/*  l     m     n     o     p     q     r     s     t     u     v     w     */
	0x38, 0x55, 0x54, 0x5C, 0x73, 0x67, 0x50, 0x6D, 0x78, 0x3E, 0x1C, 0x1D,
	/*  x     y     z     */
	0x64, 0x6E, 0x5B
};
unsigned char fnd_sel[4] = {0x01, 0x02, 0x04, 0x08};

// 레지스터 초기 세팅
// 1. LED setting
void init_LED()
{
	DDRA = 0xFF;
	for(int i=0;i<8;++i)PORTA = (1 << i);
}

// 2. Button setting
void init_button()
{
	DDRE = 0x00;
	EICRB = 0x0A; // 0b 0000 1010;
	EIMSK = 0x30; // 0b 0011 0000;
}

// 3. 7-Seg setting
void init_7seg()
{
	DDRC = 0xFF;
	DDRG = 0x0F;
	for(int i=0;i<4;++i)
	{
		PORTC = fnd_sel[i];
		for(int j=i * 1;j<j * 8;++j)PORTG = seven_seg_digits_decode_gfedcba[j];
	}
}

// 4. USART(bluetooth setting)
void init_USART()
{
	UBRR0H = 0;
	UBRR0L = 0;
	UCSR0B = 0x18; // 0b 0001 1000
	UCSR0C = 0x06; 
}

// 5. ADC setting
void init_ADC()
{
	ADMUX = 0x00;
	ADCSRA = 0x87;
}

// 6. PWM setting => Fast PWM
void init_PWM()
{
	TCCR2 = (1 << WGM01) | (1 << WGM00) | (2 << COM00);
	TCCR2 |= (2 << CS00);
	OCR2 = 100;
}

// 7. init_MOTOR()
void init_MOTOR()
{
	DDRB |= (3 << DDB6);
	PORTB = 0x80;
	_delay_ms(100);
	PORTB = 0x40;
	_delay_ms(100);
}

void init()
{
	is_open=0, is_move=0, is_up=0, has_open=1;
	q_size = 0;
	queue[0] = 8; queue[1] = 7, queue[2] = 6;
	init_7seg();
	init_ADC();
	init_button();
	init_LED();
	init_MOTOR();
	init_PWM();
	init_USART();
}

// 필요 함수
unsigned short read_adc()
{
	unsigned char adc_low, adc_high;
	unsigned short value;
	ADCSRA |= 0x40;      //ADC start conversion, ADSC = '1'
	while(~(ADCSRA&0x10));//ADC 변환 완료 검사
	adc_low = ADCL;      // 변환된 Low값 읽어오기
	adc_high = ADCH;   // 변환된 High값 읽어오기
	value = (adc_high << 8) | adc_low;
	// value는 High 및 Low 연결 16비트값
	return value;
}

void set_segment(unsigned int idx, char c)
{
	if(idx < 4 && idx >=0)
	{
		PORTC = fnd_sel[idx];
		PORTG = seven_seg_digits_decode_gfedcba[c];
	}
	return;
}

unsigned char gettingnumber()
{
	unsigned char c;
	while(!(UCSR0A & 0x80));
	c = UDR0;
	return c;
}

void putchar0(short n)
{
	while(!(UCSR0A & 0x20));
	UDR0 = n; // 1 char 전달
}
// 인터럽트
ISR(INT4_vect)
{
	if(!is_move)
	{
		cli();
		is_open = 1;
		while(~PINE & 0x10); // 버튼 누르기 기다림
		_delay_ms(20);
		set_segment(3, 'O');
		set_segment(2, 'P');
		set_segment(1, 'E');
		set_segment(0, 'N');
		sei();
		EIFR = (1 << 4);
	}
}
ISR(INT5_vect)
{
	if(!is_move)
	{
		sei();
		is_open = 0;
		while(~PINE & 0x20); // 버튼 누르기 기다림
		_delay_ms(20);
		set_segment(3, 'C');
		set_segment(2, 'L');
		set_segment(1, 'O');
		set_segment(0, 'S');
		EIFR = (1 << 5);
	}
}
ISR(USART0_RX_vect)
{
	set_segment(3, 'I');
	set_segment(2, 'N');
	set_segment(1, 'P');
	set_segment(0, '#');
	unsigned int value = 0;
	cli();
	switch(gettingnumber())
	{
		case 0:
		value = 0;
		break;
		case 1:
		value = 1;
		break;
		case 2:
		value = 2;
		break;
		case 3:
		value = 3;
		break;
		case 4:
		value = 4;
		break;
		case 5:
		value = 5;
		break;
		case 6:
		value = 6;
		break;
		case 7:
		value = 7;
		break;
		case 8:
		value = 8;
		break;
		case 9:
		value = 9; // -48해야 int->char
		break;
		default:
		break;
	}
	obj_floor_state ^= value & 0xFF;
	if(q_size != MAX_HEIGHT)
	{
		queue[q_size++] = value;
	}
}

int main()
{
	// 초기화
	init();
	sei(); // 전역 인터럽트 허용
	while(1)
	{
		// 표현
		PORTA = obj_floor_state & 0xFF;
		if(is_move)
		{
			diff_floor = obj_floor_state - current_floor;
			if((diff_floor < 40 || diff_floor > -40))
			{
				// 도착 
				current_floor = object_floor;
				OCR2 = 0; // 모터 멈추기
				is_move = 0;
				set_segment(3, '=');
				set_segment(2, '>');
				set_segment(1, '>');
				set_segment(0, (unsigned char)((current_floor/1000)-48));
			}
			else
			{
				// 위로 이동
				if(diff_floor > 0)
				{
					if(diff_floor > distance /2)OCR2 += 20;
					else OCR2 -= 20;
					if(OCR2 < 20)OCR2 = 20;
					if(OCR2 > 200)OCR2 = 200;
					current_floor += OCR2;
				}
				else // diff_floor < 0, 아래로 이동
				{
					if(diff_floor < distance /2)OCR2 += 20;
					else OCR2 -= 20;
					if(OCR2 < 20)OCR2 = 20;
					if(OCR2 > 200)OCR2 = 200;
					current_floor -= OCR2;
				}
			}
		}
		else // 정지 상태
		{
			if(!has_open)
			{
				has_open = 1;
				is_open = 1;
			}
			else
			{
				if(!is_open)
				{
					if(q_size > 0 && (obj_floor_state & 0xFF))
					{
						unsigned int idx = queue[q_size];
						for(int i=0;i<q_size-1;++i)queue[i] = queue[i+1];
						if(obj_floor_state & (1 << idx) && (idx != current_floor/1000))
							object_floor = idx * DISTANCE_PER_FLOOR;
						is_move = 1;
					}
					else
					{
						q_size = 0;
						obj_floor_state = 0x00;
					}
				}
				else
				{
					is_open = 0;
				}
			}
		}
	}
}