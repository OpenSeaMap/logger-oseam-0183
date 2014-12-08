/* binlog2hexlog.c
 * 
 * Purpose:
 *     Tool to convert OpenSeaMapLogger files containing binary data.
 *     This happens, if the Seatalk flag was not configured properly
 * 
 * Make:
       gcc -o binlog2hexlog binlog2hexlog.c
 *
 * Usage examples:
       echo ABC0D | ./binlog2hexlog
       echo '00:00:34.597;I;$POSMACC,16644,-200,2024*46' | ./binlog2hexlog
       echo '$*46' | ./binlog2hexlog
       cat DATA0001.DAT | ./binlog2hexlog | more 
       cat DATA0001.DAT | ./binlog2hexlog | grep -v "#"  
 *
 *
 * Background:
 *   - http://wkla.no-ip.biz/ArduinoWiki/doku.php?id=arduino:oseam
     - git repositories
       db6am   https://github.com/Github6am/logger-oseam-0183.git
       origin  https://github.com/willie68/OpenSeaMapLogger
       oseam   https://github.com/OpenSeaMap/logger-oseam-0183.git
 *
 *
 *   - typical data to be parsed:
 *     00:00:34.597;I;$POSMACC,16644,-200,2024*46
 *     00:00:34.596;I;$POSMVCC,5143,4943*5E
 *     00:00:34.592;A;<binary data>
 *     00:00:34.596;B;$GPVTG,,T,247.3,M,0.0,N
 *     00:00:35.406;B;$GPRMC,123158,A,4309.9431,N,01348.5853,E,0.0,250.4,250814,3,E*6A
 *
 *   - simple state machine implementation:
 *            00:00:34.596;I;$POSMVCC,5143,4943*5E
 *     state: 1  2  3  4   5  6                 7  8
 *                         10 11  ...
 *
 *  Author: 
 *     Andreas Merz, 2014
 *
 *  Licence:
 *     GPL
 */

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>

unsigned int a2nibble(unsigned char nibble)
{
  nibble=nibble | 0x20;  // make lowercase
  if (nibble > '9') 
    return( nibble - 'a'+10);
  return(nibble-'0');
}

// calculate checksum over a string buffer
void update_cksum(unsigned int *csum, unsigned char *str)
{ 
  while(*str){
    *csum ^= *(str++);
  }
} 


main(int argc, char *argv[]) 
{
   int opt;
   unsigned int c1,c0='\n';   // previous and actual character
   unsigned int state=0;
   unsigned int col=0;
   unsigned int csum=0;
   unsigned int optdebug=0; 
   unsigned int opta=0;       // handle channel A as ascii
   unsigned char *sep0=",";   // separate binary message header from body
   unsigned char *sep1="";    // default: point to empty string
   unsigned char buf[16];     // buffer for checksum calculation, no boundary check!
   
   // parse command line arguments
   while ((opt = getopt(argc, argv, "ac:d:s:")) != -1) {
       switch (opt) {
       case 's':
           sep1 = optarg;
           sep0 = ""; 
           break;
       case 'd':
           optdebug = atoi(optarg);
           break;
       case 'c':
           printf("code 0x%02X %s %c\n", *optarg, optarg, atoi(optarg));  // undocumented character conversion
           return;
           break;
       case 'a':
           opta = 1;
           break;
       default:
           fprintf(stderr, "\ncheck and convert OpenSeaMapLogger files containing binary data\n");
           fprintf(stderr, "Usage: cat DATA0001.DAT | %s [-a] [-d debuglevel] [-s separator]\n", argv[0]);
           exit(EXIT_FAILURE);
       }
   }
   
   // now process stdinput
   while(!feof(stdin)) {
     c1=c0;
     c0=getchar();
     switch(state) {

       case 0:                          // sync state
         if(isdigit(c0) && ((c1=='\n') || (c1=='\r'))) {
           state++;
         }
         else if( (c0=='$' || c0=='!') && ((c1=='\n') || (c1=='\r')) ) {
           state=7;
         }
         break;
          

       case 1:                          // timestamp H state
         if(isdigit(c1) && c0==':') {
           state++;
         }
         else if( isdigit(c0) || ((c1=='\n') || (c1=='\r'))) {
           // stay
         }
         else {
           state=0;
         }
         break;


       case 2:                          // timestamp M state
         if(isdigit(c1) && c0==':') {
           state++;
         }
         else if( isdigit(c0)) {
           // stay
         }
         else {
           state=0;
         }
         break;


       case 3:                          // timestamp S state
         if(isdigit(c1) && c0=='.') {
           state++;
         }
         else if( isdigit(c0)) {
           // stay
         }
         else {
           state=0;
         }
         break;

       case 4:                          // timestamp ms state
         if(isdigit(c1) && c0==';') {
           state++;
         }
         else if( isdigit(c0)) {
           // stay
         }
         else {
           state=0;
         }
         break;


       case 5:                          // channel
         if ( (c0 =='A') || (c0 =='B') || (c0 =='I')) {
           //stay
         }
         else if((c0 ==';') && ((c1=='B') || (c1=='I') || ((c1=='A') && opta) )) {
           state++;
         }
         else if((c0 ==';') &&  (c1=='A')) {
           state=10;
           sprintf(buf,";$POSMSK%s", sep0);
           csum=0x1F;
           update_cksum( &csum, buf);
           printf("%s", buf);
         }
         else {
           state=0;
         }
         break;
         

       case 6:                          // NMEA start
         if((c0=='$') || (c0=='!') ) {
           csum = 0;
           state++;
         }
         else {
           state=0;
           csum=0;
         }
         break;


       case 7:                          // NMEA or AIS messge body
         if(c0=='*') {
           state++;
         }
         else if((c0=='\n') || (c0=='\r')) {
           printf("##");  // mark unexpected line ending
           state=1;       
         }
         else if( c0 < 0x20 ) {    // likely corrupted
           printf("###");          
           state=0;
         }
         else {
           // stay
           csum^=c0;
         }
         break;


       case 8:                          // NMEA checksum
         if((c0=='\n') || (c0=='\r')) {
           if(csum) 
             printf(" # checksum error, residual: %02X", csum>>8);
           state=1;
         }
         else {
           // stay
           csum ^= a2nibble(c0)<<4;
           csum <<= 4;
         }
         break;

       case 10:                          // binary data 
         if((isdigit(c0) || (c0 =='$')) && ((c1=='\n') ||(c1=='\r')) ) {
           //printf("*XX\n");
           printf("*%02X\n", csum);
           state=1;
         }
         else {
           // stay
           sprintf(buf,"%s%02X",sep1,c0);
           update_cksum( &csum, buf);
           printf("%s", buf);
         }
         break;


       default:
         printf(" # %02x %02x ",c1, c0);
     
     }
     
     if( (state > 0) && (state < 10) && c0>=0x0A)
       printf("%c",c0);

     
     if(optdebug>0) {
       if(c0>=0x20)
         printf("  state: %2d   %c  %c      %02x\n", state, c1, c0, (csum&0xFF));
       else
         printf("  state: %2d  %02x %02x\n", state, c1, c0);
     }
   }
   
   printf("\n");

}
