/*
  re-calculate NMEA checksum.
  This is useful, when you manipulate the data before you pass it
  on to another software which accepts only data with correct checksum.

  gcc nmea_cksum.c -o nmea_cksum
  
  Eleganterer Ansatz:
  crc immer mitlaufen lassen, und wenn ein * kommt, dann in eine
  shadow-Variable kopieren, wenn $ kommt, dann crc init
  
*/
#include <stdio.h>



main(int argc, char** argv)
{
   int cc;
   int cksum_orig=0;
   int cksum='$';
   int state=0;

   while(state<6) {
     cc=getchar();
     switch (cc) {
       case '$':  state=1;   
                  break;
       case '*':  state=2;   
                  break;
       case '\r': state=0; 
                  break;
       case '\n': state=0; 
                  break;
       case  EOF: state=6; 
                  break;
     }
     
     switch (state) {
       case 0:  putchar( cc );
                cksum='$';
                break;
       case 1:  putchar( cc );
                cksum ^= cc;
                break;
       case 2:  putchar( cc );
                state++;
                break;
       case 3:  state++;
                //chsum_orig=atoi..
                break;
       case 4:  state++;
                //chsum_orig+=atoi..
                printf( "%02X", cksum);
                break;
       case 5:  printf( " %c unexpected ", cc);
                break;
       case 6:  putchar( cc );
                break;
     }
     //printf("\n  %c %d %02x  ", cc, state, cksum);
   }                
}
