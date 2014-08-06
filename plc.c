#define f_osc  1000000UL  // fosc = 1 MHz

#include <avr/io.h> // ATmega8
#include <avr/interrupt.h>
#include <avr/signal.h>
#include "misc.h"

// piny dla diod LED (PD5 - PD7)
#define   LED1  5
#define   LED2  6
#define   LED3  7

// flagi
char zero_cross_detected = 0;
char carriage_detected   = 0;

//
//
// FUNKCJE INICJALIZACYJNE
//
//

// inicjalizacja PWM na wyjsciu PB1 - pin 15 (p. 87-88, 95-99)
//
void init_pwm()
{
	/**

A frequency (with 50% duty cycle) waveform output in fast PWM mode can be achieved
by setting OC1A to toggle its logical level on each Compare Match (COM1A1:0 = 1).
This applies only if OCR1A is used to define the TOP value (WGM13:0 = 15). The waveform
generated will have a maximum frequency of fOC1A = fclk_I/O/2 when OCR1A is set to
zero (0x0000). This feature is similar to the OC1A toggle in CTC mode, except the double
buffer feature of the Output Compare unit is enabled in the fast PWM mode.

	**/

	// WGM13:0 = 15
	TCCR1A |= (1 << WGM11) | (1 << WGM10);
	TCCR1B |= (1 << WGM13) | (1 << WGM12);

	// COM1A1:0 = 1
	TCCR1A |= (1 << COM1A0);

	// ustawienie prescalera (fosc = 1 MHz)
	//TCCR1B |= (1 << CS10); 

	// ustawienie PB1 jako wyjście bez podciągania
	DDRB  |=  0x02;
	PORTB &= ~0x02;

	// ustawienie okresu PWM
	OCR1AL = 0x03; // pełny okres fali -> 8 okresów zegara -> f PWM = fosc / 8 = 125 kHz
	OCR1AH = 0x00;
}

// inicjalizacja przetwornika ADC na ADC0 (PC0)
//
void init_adc()
{
	// napiecie ref 2.56 V wewnetrzne + wyrownanie wynikow (8 bitow w ADCH)
	ADMUX |= (1<<REFS1) | (1<<REFS0) |  (1<<ADLAR);

	// kanał ADC0 - PC0
	// MUX = 0

	// dopuszczenie przerwania po zakończeniu konwersji
	//ADCSRA |= (1<<ADEN) | (1<<ADIE);
	ADCSRA |= (1<<ADIE);

	// konwersja ciągła (Free Run)
	ADCSRA |= (1<<ADFR);

	// start konwersji
	ADCSRA |= (1<<ADSC);
}

// inicjalizacja przerwań na INT0 (p. 64-66)
//
void init_zcd()
{
	// INT0 - detekcja przejść przez zero fazy zasilania (Zero Crossing Detection)
	MCUCR |= (1 << ISC00);

	// zgoda na przerwanie od INT0
	GICR |= (1 << INT0);
}

// inicjalizacja przerwania zegarowego od przepełnienia Timera0
//
void init_timer()
{
	TIMSK |= (1 << TOIE0);	// Timer Overflow
}


//
//
// OBSŁUGA PRZERWAŃ
//
//



// obsluga przerwan z ADC
//
SIGNAL(SIG_ADC)
{
	//if (ADCH > 10)	// 10 x 4 = 40 (40/1024 * 2,56 => 100 mV)
	if (ADCH > 4)		// 10 x 4 = 40 (40/1024 * 2,56 => 40 mV)
	{
		carriage_detected = 1;
	}
}

// detekcja przejścia przez zero - ZCD (początek szczeliny czasowej)
//
SIGNAL(SIG_INTERRUPT0)
{
	// zablokuj detekcję zera
	GICR &= ~(1 << INT0);

	zero_cross_detected = 1;

	// sygnalizujemy detekcję zera na diodzie zasilania
	cbi(PORTD, LED1);	

	// początek szczeliny czasowej - start Timera0
	//   odliczamy 1 ms = 1000 us (1000 / 8 cykli = 125)
	TCNT0  = 255 - 125;
	TCCR0 |= (1 << CS01);	// preskaler CK/8

	// póki co nic nie odebrano
	cbi(PORTD, LED3);	

	// uruchamiamy PWM
	TCCR1B |= (1 << CS10);
	sbi(PORTD, LED2);

	// uruchomienie przetwornika ADC + start konwersji
	sbi(ADCSRA, ADEN);
	sbi(ADCSRA, ADSC);
}


// przepełnienie timera (koniec szczeliny czasowej)
//
SIGNAL(SIG_OVERFLOW0)
{
	// zatrzymujemy timer
	TCCR0 &= ~(0x07);

	// zapalenie diody zasilania
	sbi(PORTD, LED1);

	// zatrzymujemy PWM
	TCCR1B &= ~(0x07);
	cbi(PORTD, LED2);

	// zatrzymanie ADC
	cbi(ADCSRA, ADEN);

	// sprawdzenie stanu flagi odbioru
	if (carriage_detected)
	{
		sbi(PORTD, LED3);
		carriage_detected = 0;
	}

	// zapalamy LED1 dla sygnalizacji zasilania
	sbi(PORTD, LED1);

	zero_cross_detected = 0;

	// zgoda na ponowną detekcję zera
	GICR |= (1 << INT0);
}


//
//
// INICJALIZACJA PRACY MODEMU I PRZEJŚCIE W OBSŁUGĘ ZGŁOSZEŃ OD PRZERWAŃ
//
//

int main()
{
	// globalna zgoda na przerwania
	sei();

	// ustawienie portów jako wyjscia dla diod diagnostycznych
	//
	// LED1 G - zasilanie modemu (przygasa w trakcie 1ms szczeliny czasowej)
	// LED2 Y - zapala się w momencie wysyłania danych
	// LED3 R - zapala się w momencie odbioru danych
	//
	DDRD |= (1 << LED1) + (1 << LED2) + (1 << LED3);

	// modem włączony
	sbi(PORTD, LED1);

	// test diod diagnostycznych
	sbi(PORTD, LED2);sbi(PORTD, LED3);wait_ms(10);
	cbi(PORTD, LED2);cbi(PORTD, LED3);wait_ms(10);
	sbi(PORTD, LED2);sbi(PORTD, LED3);wait_ms(10);
	cbi(PORTD, LED2);cbi(PORTD, LED3);

	// inicjalizacje
	//
	init_pwm();		// PWM (generacja nośnej)
	init_adc();		// przetwornik A/D (detekcja nośnej)
	init_zcd();		// przerwania na INT0 od zbocza (ZCD)
	init_timer();	// przerwania od timera (szczelina czasowa 1ms)

	// praca ciągła -> czekanie na przerwania
	while(1);
	
	return 0;
}
