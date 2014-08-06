// operacje bitowe
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~(1<<bit)) // clear bit
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= (1<<bit))	// set bit
#define tbi(sfr, bit) (_SFR_BYTE(sfr) ^= (1<<bit))	// toogle bit

#define delay_ms 10

void inline nop(unsigned long ticks)
{
    while (ticks--) asm("NOP");
}

void wait_ms(unsigned int ms)
{
    unsigned long nops = f_osc / 10000 * ms;

    nop(nops);
}

void wait_us(unsigned int us)
{
    unsigned long nops = f_osc / 100000000L * us;

    nop(nops);
}
