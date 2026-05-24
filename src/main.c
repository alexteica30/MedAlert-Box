#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#define REED_PIN        PD0
#define RED_LED_PIN     PD1

#define C4              PD2
#define C3              PD3
#define C2              PD4
#define C1              PD5
#define R1              PD6
#define R2              PD7

#define R3              PB0
#define R4              PB1
#define BUZZER_PIN      PB2
#define TM_CLK          PB3
#define TM_DIO          PB4
#define ENDSTOP_PIN     PB5

#define MOTOR_IN1       PC0
#define MOTOR_IN2       PC1
#define MOTOR_IN3       PC2
#define MOTOR_IN4       PC3

#define TM_PORT         PORTB
#define TM_DDR          DDRB

#define ENDSTOP_ACTIVE_LOW  1
#define REED_ACTIVE_LOW     1

uint8_t digits[] = {
	0x3f,
	0x30,
	0x5b,
	0x79,
	0x74,
	0x6d,
	0x6F,
	0x38,
	0x7f,
	0x7C
};

#define BAUD 9600
#define MYUBRR ((F_CPU / 16 / BAUD) - 1)

uint16_t selected_alarm_seconds = 5;
uint8_t selected_compartment = 1;

#define KEY_START_CONFIG       'D'
#define KEY_ADD_SECONDS        '#'
#define KEY_SELECT_COMPARTMENT '0'
#define KEY_START_COUNTDOWN    '*'

void red_off_green_on(void);

#define DS3231_ADDR 0x68

uint8_t bcd_to_dec(uint8_t value)
{
	return ((value >> 4) * 10) + (value & 0x0F);
}

uint8_t dec_to_bcd(uint8_t value)
{
	return ((value / 10) << 4) | (value % 10);
}

void i2c_init(void)
{
	TWSR = 0x00;
	TWBR = 72;
	TWCR = (1 << TWEN);
}

void i2c_start(void)
{
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
	while (!(TWCR & (1 << TWINT)));
}

void i2c_stop(void)
{
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

void i2c_write(uint8_t data)
{
	TWDR = data;
	TWCR = (1 << TWINT) | (1 << TWEN);
	while (!(TWCR & (1 << TWINT)));
}

uint8_t i2c_read_ack(void)
{
	TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
	while (!(TWCR & (1 << TWINT)));
	return TWDR;
}

uint8_t i2c_read_nack(void)
{
	TWCR = (1 << TWINT) | (1 << TWEN);
	while (!(TWCR & (1 << TWINT)));
	return TWDR;
}

void rtc_get_time(uint8_t *hour, uint8_t *minute, uint8_t *second)
{
	i2c_start();
	i2c_write((DS3231_ADDR << 1) | 0);
	i2c_write(0x00);

	i2c_start();
	i2c_write((DS3231_ADDR << 1) | 1);

	uint8_t sec_bcd = i2c_read_ack();
	uint8_t min_bcd = i2c_read_ack();
	uint8_t hour_bcd = i2c_read_nack();

	i2c_stop();

	*second = bcd_to_dec(sec_bcd & 0x7F);
	*minute = bcd_to_dec(min_bcd & 0x7F);
	*hour = bcd_to_dec(hour_bcd & 0x3F);
}

void rtc_set_time(uint8_t hour, uint8_t minute, uint8_t second)
{
	i2c_start();
	i2c_write((DS3231_ADDR << 1) | 0);
	i2c_write(0x00);

	i2c_write(dec_to_bcd(second));
	i2c_write(dec_to_bcd(minute));
	i2c_write(dec_to_bcd(hour));

	i2c_stop();
}

void USART0_init(unsigned int ubrr)
{
	UBRR0H = (unsigned char)(ubrr >> 8);
	UBRR0L = (unsigned char)ubrr;

	UCSR0B = (1 << TXEN0);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void USART0_transmit(char data)
{
	while (!(UCSR0A & (1 << UDRE0)));
	UDR0 = data;
}

void USART0_finish_and_disable(void)
{
	while (!(UCSR0A & (1 << TXC0)));

	UCSR0B &= ~(1 << TXEN0);

	DDRD |= (1 << RED_LED_PIN);
	red_off_green_on();
}

void USART0_print(const char *data)
{
	USART0_init(MYUBRR);

	UCSR0A |= (1 << TXC0);

	while (*data) {
		USART0_transmit(*data);
		data++;
	}

	USART0_finish_and_disable();
}

void USART0_print_alarm(uint8_t compartment)
{
	USART0_init(MYUBRR);

	UCSR0A |= (1 << TXC0);

	USART0_transmit('A');
	USART0_transmit('L');
	USART0_transmit('A');
	USART0_transmit('R');
	USART0_transmit('M');
	USART0_transmit(':');
	USART0_transmit('0' + compartment);
	USART0_transmit('\r');
	USART0_transmit('\n');

	USART0_finish_and_disable();
}

void buzzer_on(void)
{
	DDRB |= (1 << BUZZER_PIN);
	PORTB |= (1 << BUZZER_PIN);
}

void buzzer_off(void)
{
	PORTB &= ~(1 << BUZZER_PIN);
	DDRB &= ~(1 << BUZZER_PIN);
}

void buzzer_beep(void)
{
	buzzer_on();
	_delay_ms(120);
	buzzer_off();
}

void red_on_green_off(void)
{
	PORTD |= (1 << RED_LED_PIN);
}

void red_off_green_on(void)
{
	PORTD &= ~(1 << RED_LED_PIN);
}

void tm_delay(void)
{
	_delay_us(5);
}

void clk_low(void)
{
	TM_DDR |= (1 << TM_CLK);
	TM_PORT &= ~(1 << TM_CLK);
}

void clk_high(void)
{
	TM_DDR |= (1 << TM_CLK);
	TM_PORT |= (1 << TM_CLK);
}

void dio_low(void)
{
	TM_DDR |= (1 << TM_DIO);
	TM_PORT &= ~(1 << TM_DIO);
}

void dio_high(void)
{
	TM_DDR |= (1 << TM_DIO);
	TM_PORT |= (1 << TM_DIO);
}

void dio_release(void)
{
	TM_DDR &= ~(1 << TM_DIO);
	TM_PORT |= (1 << TM_DIO);
}

void tm_start(void)
{
	dio_high();
	clk_high();
	tm_delay();
	dio_low();
	tm_delay();
	clk_low();
}

void tm_stop(void)
{
	clk_low();
	tm_delay();
	dio_low();
	tm_delay();
	clk_high();
	tm_delay();
	dio_high();
	tm_delay();
}

void tm_write_byte(uint8_t data)
{
	for (uint8_t i = 0; i < 8; i++) {
		clk_low();

		if (data & 0x01) {
			dio_high();
		} else {
			dio_low();
		}

		tm_delay();
		clk_high();
		tm_delay();

		data >>= 1;
	}

	clk_low();
	dio_release();
	tm_delay();
	clk_high();
	tm_delay();
	clk_low();
	dio_high();
}

void tm_set_brightness(uint8_t brightness)
{
	if (brightness > 7) {
		brightness = 7;
	}

	tm_start();
	tm_write_byte(0x88 | brightness);
	tm_stop();
}

void tm_display_raw(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3)
{
	tm_start();
	tm_write_byte(0x40);
	tm_stop();

	tm_start();
	tm_write_byte(0xC0);
	tm_write_byte(d0);
	tm_write_byte(d1);
	tm_write_byte(d2);
	tm_write_byte(d3);
	tm_stop();

	tm_set_brightness(7);
}

void tm_display_number(uint16_t number)
{
	uint8_t mii = (number / 1000) % 10;
	uint8_t sute = (number / 100) % 10;
	uint8_t zeci = (number / 10) % 10;
	uint8_t unitati = number % 10;


	tm_display_raw(
		digits[unitati],
		digits[zeci],
		digits[sute],
		digits[mii]
	);
}

void tm_clear(void)
{
	tm_display_raw(0x00, 0x00, 0x00, 0x00);
}

void tm_display_compartment(uint8_t compartment)
{
	if (compartment > 9) {
		compartment = 9;
	}


	tm_display_raw(
		digits[compartment],
		0x00,
		0x00,
		0x00
	);
}

void display_current_time(void)
{
	uint8_t hour;
	uint8_t minute;
	uint8_t second;

	rtc_get_time(&hour, &minute, &second);

	if (hour > 23 || minute > 59) {
		tm_display_number(9999);
		return;
	}

	uint8_t h1 = hour / 10;
	uint8_t h2 = hour % 10;
	uint8_t m1 = minute / 10;
	uint8_t m2 = minute % 10;

	tm_display_raw(
		digits[m2],
		digits[m1] | 0x80,
		digits[h2],
		digits[h1]
	);
}

const uint8_t motor_steps[8] = {
	0b0001,
	0b0011,
	0b0010,
	0b0110,
	0b0100,
	0b1100,
	0b1000,
	0b1001
};

uint8_t motor_index = 0;

void motor_write(uint8_t value)
{
	if (value & 0b0001) {
		PORTC |= (1 << MOTOR_IN1);
	} else {
		PORTC &= ~(1 << MOTOR_IN1);
	}

	if (value & 0b0010) {
		PORTC |= (1 << MOTOR_IN2);
	} else {
		PORTC &= ~(1 << MOTOR_IN2);
	}

	if (value & 0b0100) {
		PORTC |= (1 << MOTOR_IN3);
	} else {
		PORTC &= ~(1 << MOTOR_IN3);
	}

	if (value & 0b1000) {
		PORTC |= (1 << MOTOR_IN4);
	} else {
		PORTC &= ~(1 << MOTOR_IN4);
	}
}

void motor_step_forward(void)
{
	motor_write(motor_steps[motor_index]);
	motor_index++;

	if (motor_index >= 8) {
		motor_index = 0;
	}

	_delay_ms(15);
}

void motor_step_backward(void)
{
	motor_write(motor_steps[motor_index]);

	if (motor_index == 0) {
		motor_index = 7;
	} else {
		motor_index--;
	}

	_delay_ms(15);
}

void motor_stop(void)
{
	PORTC &= ~((1 << MOTOR_IN1) | (1 << MOTOR_IN2) |
	           (1 << MOTOR_IN3) | (1 << MOTOR_IN4));
}

uint8_t endstop_pressed(void)
{
#if ENDSTOP_ACTIVE_LOW
	return !(PINB & (1 << ENDSTOP_PIN));
#else
	return (PINB & (1 << ENDSTOP_PIN));
#endif
}

uint8_t reed_active(void)
{
#if REED_ACTIVE_LOW
	return !(PIND & (1 << REED_PIN));
#else
	return (PIND & (1 << REED_PIN));
#endif
}

uint8_t reed_closed_stable(void)
{
	for (uint8_t i = 0; i < 10; i++) {
		if (!reed_active()) {
			return 0;
		}

		_delay_ms(20);
	}

	return 1;
}

void keypad_init(void)
{
	DDRD |= (1 << C4) | (1 << C3) | (1 << C2) | (1 << C1);
	PORTD |= (1 << C4) | (1 << C3) | (1 << C2) | (1 << C1);

	DDRD &= ~((1 << R1) | (1 << R2));
	PORTD |= (1 << R1) | (1 << R2);

	DDRB &= ~((1 << R3) | (1 << R4));
	PORTB |= (1 << R3) | (1 << R4);
}

void set_all_columns_high(void)
{
	PORTD |= (1 << C4) | (1 << C3) | (1 << C2) | (1 << C1);
}

void set_column_low(uint8_t col)
{
	set_all_columns_high();

	if (col == 0) {
		PORTD &= ~(1 << C4);
	} else if (col == 1) {
		PORTD &= ~(1 << C3);
	} else if (col == 2) {
		PORTD &= ~(1 << C2);
	} else if (col == 3) {
		PORTD &= ~(1 << C1);
	}
}

uint8_t read_row(void)
{
	if (!(PIND & (1 << R1))) {
		return 0;
	}

	if (!(PIND & (1 << R2))) {
		return 1;
	}

	if (!(PINB & (1 << R3))) {
		return 2;
	}

	if (!(PINB & (1 << R4))) {
		return 3;
	}

	return 255;
}

char keypad_get_key(void)
{
	char keys[4][4] = {
		{'A', '3', '2', '1'},
		{'B', '6', '5', '4'},
		{'C', '9', '8', '7'},
		{'*', '0', '#', 'D'}
	};

	for (uint8_t col = 0; col < 4; col++) {
		set_column_low(col);
		_delay_us(10);

		uint8_t row = read_row();

		if (row != 255) {
			_delay_ms(25);

			if (read_row() == row) {
				while (read_row() != 255) {
					_delay_ms(10);
				}

				set_all_columns_high();
				return keys[row][col];
			}
		}
	}

	set_all_columns_high();
	return 0;
}

void wait_key_release(void)
{
	while (keypad_get_key() != 0) {
		_delay_ms(50);
	}
}

uint8_t key_to_compartment(char key)
{
	if (key == '1') {
		return 1;
	}

	if (key == '2') {
		return 2;
	}

	if (key == '3') {
		return 3;
	}

	if (key == 'A') {
		return 4;
	}

	if (key == '4') {
		return 5;
	}

	if (key == '5') {
		return 6;
	}

	if (key == '6') {
		return 7;
	}

	if (key == 'B') {
		return 8;
	}

	return 0;
}

void GPIO_init(void)
{
	DDRD &= ~(1 << REED_PIN);
	PORTD |= (1 << REED_PIN);

	DDRD |= (1 << RED_LED_PIN);
	red_off_green_on();

	DDRB |= (1 << BUZZER_PIN);
	buzzer_off();

	DDRB &= ~(1 << ENDSTOP_PIN);
	PORTB |= (1 << ENDSTOP_PIN);

	DDRC |= (1 << MOTOR_IN1) | (1 << MOTOR_IN2) |
	        (1 << MOTOR_IN3) | (1 << MOTOR_IN4);
	motor_stop();

	TM_DDR |= (1 << TM_CLK) | (1 << TM_DIO);
	TM_PORT |= (1 << TM_CLK) | (1 << TM_DIO);

	keypad_init();
}

void configure_alarm(void)
{
	char key;
	uint8_t setting_started = 0;
	uint8_t selecting_compartment = 0;
	uint8_t compartment_chosen = 0;
	uint8_t compartment_value;

	selected_alarm_seconds = 0;
	selected_compartment = 1;

	while (1) {
		key = keypad_get_key();


		if (setting_started == 0) {
			if (key == 0) {
				display_current_time();
				_delay_ms(100);
				continue;
			}

			wait_key_release();

			if (key == KEY_START_CONFIG) {
				setting_started = 1;
				selecting_compartment = 0;
				compartment_chosen = 0;
				selected_alarm_seconds = 0;
				selected_compartment = 1;

				tm_display_number(0);
				_delay_ms(300);
			}

			continue;
		}

		if (key == 0) {
			_delay_ms(100);
			continue;
		}

		wait_key_release();


		if (selecting_compartment == 0) {
			if (key == KEY_ADD_SECONDS) {
				selected_alarm_seconds += 5;

				if (selected_alarm_seconds > 9999) {
					selected_alarm_seconds = 9999;
				}

				tm_display_number(selected_alarm_seconds);
				_delay_ms(300);
			} else if (key == KEY_SELECT_COMPARTMENT) {
				if (selected_alarm_seconds == 0) {
					selected_alarm_seconds = 5;
				}

				selecting_compartment = 1;
				compartment_chosen = 0;


				tm_display_number(0);
				_delay_ms(300);
			}
		} else {

			compartment_value = key_to_compartment(key);

			if (compartment_value >= 1 && compartment_value <= 8) {
				selected_compartment = compartment_value;
				compartment_chosen = 1;

				tm_display_number(selected_compartment);
				_delay_ms(300);
			} else if (key == KEY_START_COUNTDOWN && compartment_chosen == 1) {
				return;
			}
		}
	}
}

void alarm_sequence(void)
{
	red_on_green_off();

	for (uint8_t i = 0; i < 3; i++) {
		buzzer_beep();
		_delay_ms(150);
	}

	red_on_green_off();

	uint16_t steps = 0;

	while (!endstop_pressed() && steps < 1500) {
		motor_step_backward();
		steps++;
	}

	motor_stop();
	_delay_ms(300);

	if (endstop_pressed()) {
		red_on_green_off();

		for (uint16_t i = 0; i < steps; i++) {
			motor_step_forward();
		}

		motor_stop();
		red_off_green_on();
	} else {
		motor_stop();
		red_on_green_off();
	}

	red_off_green_on();


	USART0_print_alarm(selected_compartment);

	while (reed_active()) {
		buzzer_on();
		_delay_ms(500);

		buzzer_off();
		_delay_ms(50);
	}

	while (!reed_closed_stable()) {
		buzzer_on();
		_delay_ms(500);

		buzzer_off();
		_delay_ms(50);
	}

	buzzer_off();

	_delay_ms(1000);

	USART0_print("DONE\r\n");

	red_off_green_on();
	_delay_ms(100);
}

int main(void)
{
	GPIO_init();
	i2c_init();

	while (1) {
		buzzer_off();
		red_off_green_on();

		configure_alarm();

		red_on_green_off();

		for (uint16_t i = selected_alarm_seconds; i > 0; i--) {
			tm_display_number(i);
			_delay_ms(1000);
		}

		tm_clear();
		_delay_ms(200);

		alarm_sequence();

		buzzer_off();
		set_all_columns_high();
		_delay_ms(500);
	}

	return 0;
}
