#include <io.h>
#include <delay.h>
#include <stdio.h>
#include <ff.h>
#include <display.h>

#asm
    .equ __lcd_port=0x02
    .equ __lcd_EN=1
    .equ __lcd_RS=0
    .equ __lcd_D4=2
    .equ __lcd_D5=3
    .equ __lcd_D6=4
    .equ __lcd_D7=5
#endasm

//Código base que reproduce A001.WAV que es un WAV, Mono, 8-bit, y frec de muestreo de 22050HZ
    
char bufferL[256];
char bufferH[256]; 
char NombreArchivo[6][12]  = {"0:A001.wav","0:A002.wav","0:A003.wav","0:A004.wav","0:A005.wav","0:A006.wav"};
unsigned int i=0;
bit LeerBufferH,LeerBufferL,pausa;
char s = 0, stereo;
unsigned long muestras;

interrupt [TIM1_COMPA] void timer1_compa_isr(void)
{
disk_timerproc();
/* MMC/SD/SD HC card access low level timing function */
}
        
//Interrupción que se ejecuta cada T=1/Fmuestreo_Wav
interrupt [TIM2_COMPA] void timer2_compa_isr(void)         
{
    if (stereo == 0)
    {
        if (i<256){
          OCR0A=bufferL[i];
          OCR0B=bufferL[i++];
        }
        else      
        {
          OCR0A=bufferH[i-256];
          OCR0B=bufferH[i-256];
          i++;
        }   
        if (i==256)
           LeerBufferL=1;
        if (i==512)
        {
           LeerBufferH=1;
           i=0;
        }
    } 
    else 
    {
        if (i<256){
          OCR0A=bufferL[i++];
          OCR0B=bufferL[i++];
          }
        else      
        {
          OCR0A=bufferH[i-256];
          i++;
          OCR0B=bufferH[i-256];
          i++;
        }   
        if (i==256)
           LeerBufferL=1;
        if (i==512)
        {
           LeerBufferH=1;
           i=0;
        }
    }   
}

void DisplaySet(char buffer[])
{
    char i=0,j=0;
    char artist[16],title[16];
    MoveCursor(0,0);
    StringLCD("                "); 
    MoveCursor(0,1);
    StringLCD("                ");
    while(buffer[i]!='M')
        i++;
    i+=5;
    while(buffer[i]!=NULL)
    {
        title[j]=buffer[i];
        j++;
        i++;
    } 
    title[j]='\0';      
    while(buffer[i]!='T')
        i++; 
    i+=5;
    j=0;
    while(buffer[i]!=NULL)     
    {               
        artist[j]=buffer[i];  
        j++;
        i++;
    }
    artist[j]='\0';  
    MoveCursor(0,0);
    StringLCDVar(title);  
    MoveCursor(0,1);
    StringLCDVar(artist);
    //StringLCDVar(bufferH);
}

void main()
{
    unsigned int  br;
       
    /* FAT function result */
    FRESULT res;
                  
   
    /* will hold the information for logical drive 0: */
    FATFS drive;
    FIL archivo; // file objects 
                                  
    CLKPR=0x80;         
    CLKPR=0x01;         //Cambiar a 8MHz la frecuencia de operación del micro 
       
    // Código para hacer una interrupción periódica cada 10ms
    // Timer/Counter 1 initialization
    // Clock source: System Clock
    // Clock value: 1000.000 kHz
    // Mode: CTC top=OCR1A
    // Compare A Match Interrupt: On
    TCCR1B=0x0A;     //CK/8 10ms con oscilador de 8MHz
    OCR1AH=0x27;
    OCR1AL=0x10;
    TIMSK1=0x02; 
                                                    
    //PWM para conversión Digital Analógica WAV->Sonido
    // Timer/Counter 0 initialization
    // Clock source: System Clock
    // Clock value: 8000.000 kHz
    // Mode: Fast PWM top=0xFF
    // OC0A output: Non-Inverted PWM
    TCCR0A=0xA3;        // 0x83 (salida en 0C0A, mono)         
                        // 0xA3 (salida en 0C0A y 0C0B para estereo)
    DDRB.7=1;  //Salida bocina (OC0A)
    
    DDRG.5=1; // Salida bocina (OC0B)
                                  
    // Timer/Counter 2 initialization
    // Clock source: System Clock
    // Clock value: 1000.000 kHz
    // Mode: CTC top=OCR2A
    ASSR=0x00;
    TCCR2A=0x02;
    TCCR2B=0x02;
    OCR2A=0xFF;     // Cambia de acuerdo a la frecuencia de muestreo

    // Timer/Counter 2 Interrupt(s) initialization
    TIMSK2=0x02;
    
    DDRD.7=1;
    #asm("sei")   
    disk_initialize(0);  /* Inicia el puerto SPI para la SD */
    delay_ms(500);
    PORTA.6=1;
    PORTA.7=1;
    PORTC.7=1;
    
    SetupLCD();                
    /* mount logical drive 0: */
    if ((res=f_mount(0,&drive))==FR_OK){  
        while(1)
        { 
           
        //Lectura de Archivo/ 
        res = f_open(&archivo, NombreArchivo[s], FA_OPEN_EXISTING | FA_READ);
        if(s < 6)
        	s++;	
        else
        	s = 0;
        EraseLCD();
        if (res==FR_OK){ 
            PORTD.7=1;
            f_read(&archivo, bufferL, 44,&br); //leer encabezado
            if (bufferL[25] == '>'){
                OCR2A=0x3D;    
            } else if (bufferL[25] == 'V'){
                OCR2A=0x2C;
            } else if (bufferL[25] == ']'){
                OCR2A=0x28;
            }
          
            if (bufferL[22] == 1)
                stereo = 0;
            else
                stereo = 1;
            i = 0;
            
            muestras = (long)bufferL[43]*16777216+(long)bufferL[42]*65536+(long)bufferL[41]*256+bufferL[40];
            f_lseek(&archivo,muestras+44);
            f_read(&archivo, bufferH, 100,&br); //BufferH esta la info de la cancion
                        
            DisplaySet(bufferH);
          
            f_lseek(&archivo,44);
            f_read(&archivo, bufferL, 256,&br); //leer los primeros 512 bytes del WAV
            f_read(&archivo, bufferH, 256,&br);    
            LeerBufferL=0;     
            LeerBufferH=0;
            TCCR0B=0x01;    //Prende sonido
            do{  
                if (PINA.6 == 0){                    // NEXT
                    delay_ms(30);         //rebote al presionar
                    while (PINA.6 == 0);  //esperar a soltar boton 
                    delay_ms(10);
                    EraseLCD();
                    break;
                    }
                    if (PINA.7 == 0){            // REWIND
                    delay_ms(30);         //rebote al presionar
                    while (PINA.7 == 0);  //esperar a soltar boton 
                    delay_ms(10);
                    s--;
                    break;
                    } 
                    if (PINC.7 == 0){            // PAUSE
                    delay_ms(30);         //rebote al presionar
                    while (PINC.7 == 0);  //esperar a soltar boton 
                    delay_ms(10);
                    pausa=~pausa;  
                    do{
                        TCCR0B=0x00;
                       if (PINC.7 == 0){
                        delay_ms(30);         //rebote al presionar
                        while (PINC.7 == 0);  //esperar a soltar boton 
                        delay_ms(10);
                        pausa=~pausa;
                        TCCR0B=0x01;
                        } 
                    }
                    while (pausa == 1);
                    }
                      
                 while((LeerBufferH==0)&&(LeerBufferL==0));
                 if (LeerBufferL)
                 {                       
                     f_read(&archivo, bufferL, 256,&br); //leer encabezado
                     LeerBufferL=0; 
                 }
                 else
                 { 
                     f_read(&archivo, bufferH, 256,&br); //leer encabezado
                     LeerBufferH=0;
                 }            
                             
            }while(br==256);
            TCCR0B=0x00;   //Apaga sonido
            f_close(&archivo);    
        }              
        }
    }
    f_mount(0, 0); //Cerrar drive de SD
    while(1);
}