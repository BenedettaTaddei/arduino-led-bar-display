// byte dove ogni bit è accessibile singolarmente  
typedef struct {
    uint8_t b0:1;
    uint8_t b1:1;
    uint8_t b2:1;
    uint8_t b3:1;
    uint8_t b4:1;
    uint8_t b5:1;
    uint8_t b6:1;
    uint8_t b7:1;
} bf_t;

#define POBbf (*(volatile bf_t*)&PORTB)
#define PIBbf (*(volatile bf_t*)&PINB)
#define PDBbf (*(volatile bf_t*)&DDRB)
#define PODbf (*(volatile bf_t*)&PORTD)
#define PIDbf (*(volatile bf_t*)&PIND)
#define PDDbf (*(volatile bf_t*)&DDRD)
#define FLAGbf (*(volatile bf_t*)&GPIOR0)

// Variabili globali
volatile uint8_t K = 0x10;   // fattore di scala 1.0 in formato 4.4
volatile uint8_t V0, V1;     // valori potenziometri a 4 bit [0;15]
volatile uint8_t H0, H1;     // altezze barre [0;10]

// Contatore TICK
#define TAUTICK 100
volatile uint8_t cntTick = TAUTICK;

// ISR Timer genera TICK periodico
ISR(TIMER0_COMPB_vect) {
    if(cntTick) if(--cntTick == 0) {
        cntTick = TAUTICK;
        FLAGbf.b0 = 1;  // TICK = 1
    }
}

// ISR Encoder
ISR(INT0_vect) {
    uint8_t DT = PIDbf.b3;
    if(DT == 1) {                    // CLK sceso per primo → orario
        if(K < 0x20) K += 0x01;     // incrementa, satura a 2.0
    } else {                         // DT sceso per primo → antiorario
        if(K > 0x04) K -= 0x01;     // decrementa, satura a 0.25
    }
}

void inviaBit(uint8_t data) {
    for(int8_t i = 7; i >= 0; i--) {
        POBbf.b3 = (data >> i) & 1;
        POBbf.b5 = 1;
        POBbf.b5 = 0;
    }
}

uint8_t calcolaAltezza(uint8_t valore, uint8_t K) {
    uint16_t h = (valore * K * 10) / (15 * 0x10);
    if(h > 10) h = 10;
    return (uint8_t)h;
}

void aggiornaBarreLED(uint8_t h0, uint8_t h1) {
    uint16_t mask0 = (1 << h0) - 1;
    uint16_t mask1 = (1 << h1) - 1;
    uint8_t byte3 = (mask1 >> 6) & 0x0F;
    uint8_t byte2 = ((mask0 >> 8) & 0x03) | ((mask1 & 0x3F) << 2);
    uint8_t byte1 = mask0 & 0xFF;

    POBbf.b2 = 0;
    inviaBit(byte3);
    inviaBit(byte2);
    inviaBit(byte1);
    POBbf.b2 = 1;
}

void setup() {
    PDBbf.b3 = 1;  // DS  = D11
    PDBbf.b5 = 1;  // SCK = D13
    PDBbf.b2 = 1;  // RCK = D10

    PDDbf.b2 = 0;  // D2 CLK encoder input
    PDDbf.b3 = 0;  // D3 DT encoder input
    PODbf.b2 = 1;  // pull-up CLK
    PODbf.b3 = 1;  // pull-up DT

    EICRA = _BV(ISC01);   // INT0 su fronte di discesa
    EIMSK = _BV(INT0);

    ADMUX  = _BV(REFS0);
    ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);

    OCR0B  = 0x80;
    TIMSK0 = _BV(OCIE0B);

    FLAGbf.b0 = 0;  // TICK = 0
    K = 0x10;       // K = 1.0 in formato 4.4

    sei();
}

void loop() {
    if(FLAGbf.b0) {
        FLAGbf.b0 = 0;

        // Lettura A0
        ADMUX = _BV(REFS0) | 0x00;
        ADCSRA |= _BV(ADSC);
        while(ADCSRA & _BV(ADSC));
        V0 = ADC >> 6;   // scala da [0-1023] a [0-15]

        // Lettura A1
        ADMUX = _BV(REFS0) | 0x01;
        ADCSRA |= _BV(ADSC);
        while(ADCSRA & _BV(ADSC));
        V1 = ADC >> 6;   // scala da [0-1023] a [0-15]

        // Calcolo altezze
        H0 = calcolaAltezza(V0, K);
        H1 = calcolaAltezza(V1, K);

        // Aggiorna barre
        aggiornaBarreLED(H0, H1);
    }
}
