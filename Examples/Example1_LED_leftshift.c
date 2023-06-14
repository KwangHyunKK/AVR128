#include <avr/io.h>
#include <util/delay.h>

#define DEBOUNCING_DELAY 20 // 디바운싱 지연 시간
void msec_delay(int n); // 시간 지연 함수

unsigned char pattern[8] = {0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F};
	
	/// <Summary>
	/// 폴링 방식을 활용한 스위치 켜는 코드
	/// </Summary>
int main()
{
	int i = 0;
	
	DDRA = 0xFF; // 포트 A를 출력으로 설정
	DDRD = 0x00;
	PORTA = pattern[i]; // 처음 패턴으로 LED 켜기
	while(1)
	{
		
		while(!(~PIND & 0x01)); // 스위치 누름을 기다림
		msec_delay(DEBOUNCING_DELAY); // 시간 지연
		
		if(++i == 8) i=0; // 마지막 패턴에서 인덱스 리셋
		PORTA = pattern[i]; // i-번째 패턴으로 점등
		
		while(~PIND& 0x01); // 스위치 떨어짐을 기다림
		msec_delay(DEBOUNCING_DELAY); // 시간 지연
	}
}

void msec_delay(int n)
{
	for(;n>0;--n)_delay_ms(1); // 1msec * n만큼 지연
}