/*
  re-calculate NMEA checksum.
  This is useful, when you manipulate the data before you pass it
  on to another software which accepts only data with correct checksum.

  gcc nmea_cksum.c -o nmea_cksum
  
  Eleganterer Ansatz:
  crc immer mitlaufen lassen, und wenn ein * kommt, dann in eine
  shadow-Variable kopieren, wenn $ kommt, dann crc init
  
  This is not a proper parser and may not detect all corrupted sentences
*/

#include <stdio.h>



main(int argc, char** argv)
{
   int cc;
   int cksum_orig=0;
   int cksum='$';
   int state=0;
   char buf[3];
   
   int option_r=0;    // replace wrong checksum
   int option_m=1;    // mark checksum error

   while(state<6) {
     cc=getchar();
     switch (cc) {
       case '$':  state=1;   
                  break;
       case '*':  if(state==1)
                    state=2;   
                  break;
       case '\r': if(state==5)
                    state=0; 
                  break;
       case '\n': if(state==5)
                    state=0; 
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
                buf[0]=cc;
                break;
       case 4:  state++;
                buf[1]=cc;
                buf[2]=0;
                sscanf(buf,"%2X",&cksum_orig);
                if(option_r) {
                  printf( "%02X", cksum);
                } else{
                  printf( "%02X", cksum_orig);
                }
                if(option_m &&(cksum_orig!=cksum))
                  printf( " # wrong:%02X # right:%02X # ",cksum_orig, cksum);
                break;
       case 5:  printf( "%c # unexpected # ", cc);
                state=0;
                break;
       case 6:  putchar( cc );
                break;
     }
     //printf("\n  %c %d %02x  ", cc, state, cksum);
   }                
}
