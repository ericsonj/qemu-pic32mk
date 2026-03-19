/*
 * hello.c — Hello World for PIC32MK QEMU
 *
 * Uses UART1 in polled TX mode.
 * U1MODE base: 0xBF828000  (DS60001519E §21, Table 21-2)
 */

/* UART1 registers (KSEG1 virtual addresses) */
#define U1MODE   (*(volatile unsigned int *)0xBF828000u)
#define U1STA    (*(volatile unsigned int *)0xBF828010u)
#define U1TXREG  (*(volatile unsigned int *)0xBF828020u)
#define U1BRG    (*(volatile unsigned int *)0xBF828040u)

/* U1MODE bits */
#define UARTEN   (1u << 15)     /* UART enable */

/* U1STA bits */
#define UTXEN    (1u << 10)     /* TX enable */
#define UTXBF    (1u <<  9)     /* TX buffer full — wait when set */

static void uart1_init(void)
{
    U1BRG  = 64;                /* baud rate divider (ignored by QEMU stub) */
    U1STA  = UTXEN;             /* enable TX */
    U1MODE = UARTEN;            /* enable UART */
}

static void uart1_putc(char c)
{
    while (U1STA & UTXBF)       /* wait until TX buffer not full */
        ;
    U1TXREG = (unsigned int)c;
}

static void uart1_puts(const char *s)
{
    while (*s)
        uart1_putc(*s++);
}

int main(void)
{
    uart1_init();
    uart1_puts("Hello, PIC32MK!\r\n");
    for (;;)
        ;
    return 0;
}
