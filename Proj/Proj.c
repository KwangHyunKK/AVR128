#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#define DELAY_TIME 20
#define WAIT_TIME 5000
#define DISTANCE_PER_FLOOR 1000;
#define QUEUE_SIZE 20;
static unsigned int queue[QUEUE_SIZE];
static unsigned int left, right; // 큐의 좌/우측에 넣을 값들
static unsigned int velocity, acceleration=10; // 속도 / 가속도
static unsigned int current_floor, diff_floor, object_floor; // 목표 층과 현재 층
static unsigned char is_open, is_move, is_up, has_open; // 움직일 수 있는지, 문을 열 수 있는지 check
static unsigned long times1, times2; // 시간 check
static unsigned int duty = 0;

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
unsigned char fnd[4];
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
    DDRC = 0xFF;
    DDRG = 0x0F;
}

void set_7seg_num(unsigned int idx, unsigned int digit)
{
    PORTC = fnd_sel[idx];
    PORTG = seven_seg_digits_decode_gfedcba[digit];
}

// idx : 위치  c : 문자열
void set_7seg_char(unsigned int idx ,char c)
{
    PORTC = fnd_sel[idx];
    PORTG = seven_seg_digits_decode_gfedcba[c-48];
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
    TIMSK = 0x40; // TOV 인터럽트에서 사용 : CLOCK
    TCCR2 = (1 << WGM01) | (1 << WGM00) | (2 << COM00); // Fast PWM 모드, 비교 일치 떄 동작모드
    TCRR2 |= (3 << CS00); // 분주비 8 설정 (2) // 3 => 64로 설정
    OCR2 = duty;
}

// 인터럽트
ISR(INT4_vect)
{
    cli(); // 열리는 도중에는 인터럽트 불가능
    // 열림 버튼
    // 7-seg에 open이라고 적는다.
    is_open = 1;
    set_7seg_char(3, 'O');
    set_7seg_char(2, 'P');
    set_7seg_char(1, 'E');
    set_7seg_char(0, 'N');
    while(~PINE & 0x10); // 버튼 누르기 기다림
    msec_delay(DELAY_TIME);
    EIFR = (1 << 4);
}

ISR(INT5_vect)
{
    // 닫힘 버튼 : 5초 후에 닫는다
    // 7-seg에 close라고 적는다
    sei(); // 열림 버튼이 열리면 다시 열어야 하니까 반응 가능
    if(times2 < 5000)break;
    else{
        is_open = 0;
        set_7seg_char(3, 'C');
        set_7seg_char(2, 'L');
        set_7seg_char(1, 'O');
        set_7seg_char(0, 'S');
    }
    while(~PINE & 0x20); 
    msec_delay(DELAY_TIME);
    EIFR = (1 << 5);
}

// 가장 많이 사용될 함수 
ISR(TIMER2_COMP_vect)
{
    // 움직이는 상태
    if(is_move)
    {  
        cli();
        // 층 변화에 따른 변화 check
        if(is_up) // current_floor < object_floor // 정회전
        {
            // 1층 이동
            if(object_floor - current_floor > diff_floor/2)
            {
                current_floor += velocity;
                velocity += 10;
            }
            else if(object_floor - current_floor== 0)
            {
                current_floor = object_floor;
                velocity = 0;
                is_move = 0;
            }
            else 
            {
                current_floor += velocity;
                if(velocity <= 10)velocity = 10;
                velocity -= 10;
                if(object_floor-current_floor < 100)
                {
                    int floor = object_floor/DISTANCE_PER_FLOOR;
                    set_7seg_char(3, '=');
                    set_7seg_char(2, '>');
                    set_7seg_num(1, floor/10);
                    set_7seg_char(0, floor%10);
                }
            }

            OCR2 = velocity;
        }else // object_floor > current_floor // 역회전 
        {
            // 1층 이동
            if(current_floor - object_floor > diff_floor/2)
            {
                current_floor -= velocity;
                velocity += 10;
            }
            else if(current_floor - object_floor == 0)
            {
                current_floor = object_floor;
                velocity = 0;
                is_move = 0;
            }
            else
            {
                current_floor -= velocity;
                if(velocity <= 10)velocity = 10;
                velocity -= 10;
                if(current_floor-object_floor < 100)
                {
                    int floor = object_floor/DISTANCE_PER_FLOOR;
                    set_7seg_char(3, '=');
                    set_7seg_char(2, '>');
                    set_7seg_num(1, floor/10);
                    set_7seg_char(0, floor%10);
                }
            }
            OCR2 = 255 - velocity;
        }
    }
    else // 멈춰있는 상태
    {
        // is_open이 아니고 1초가 지나면 is_move 
        sei();

        if(!has_open)
        {
            is_open = 1;
            has_open = 0;
            set_7seg_char(3, 'O');
            set_7seg_char(2, 'P');
            set_7seg_char(1, 'E');
            set_7seg_char(0, 'N');
        }
        ++times1;

        if(!is_open)
        {
            // 다음 행선지를 정한다.
            if(left != right)
            {
                object_floor = queue[left] * DISTANCE_PER_FLOOR;
                left = (left + 1) % QUEUE_SIZE;
            }else{
                // 움직일 필요가 없다.
                // 배열이 비어있다.
            }
            if(times2 < 1000)++times2;
            else{
                is_move = 1;
                times2 = 0;
            }
        }
    }
    EIFR = 0x40; // 오버플로 플래그 지움 
}

// 블루투스 입력
ISR(USART0_RX_vect)
{
    cli();
    switch(gettingnumber())
    {
        case 0:
        break;
        case 1:
        break;
        case 2:
        break;
        case 3:
        break;
        case 4:
        break;
        case 5:
        break;
        case 6:
        break;
        case 7:
        break;
        case 8:
        break;
        case 9:
        break;
        default:
        break;
    }
}

int main()
{
    sei(); // 전역 인터럽트 허용
    while(1)
    {
        // 조도 센서를 통한 LED 제어
        OCR2 = duty;
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