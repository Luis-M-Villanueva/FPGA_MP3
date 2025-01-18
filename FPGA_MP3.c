#include <system.h> 
#include <stdio.h> 
#include "sys/alt_stdio.h" 
#include "altera_avalon_pio_regs.h" 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <stdbool.h> 
#include "Altera_UP_SD_Card_Avalon_Interface.h" 
#include <unistd.h> 
#include "altera_avalon_pio_regs.h" 
#include "sys/alt_irq.h" 
#include "alt_types.h" 
#include "altera_avalon_timer_regs.h" 
#include <time.h> 
#include "altera_up_avalon_audio_and_video_config.h" 
#include "altera_up_avalon_audio.h" 
 
 
#define MAX_BUFFER 5000000 
15 
 
 
#define MAX_SUBDIRECTORIES 20 
#define MAX_FILE 30 
 
volatile char *LCD_display_ptr = (char *) 0xFF203050; // 16x2 character display 
char *files[MAX_FILE]; 
int findex = 0; 
 
int k = 128; 
int f_size; 
int display = 0; 
unsigned int soundBuff[MAX_BUFFER]; 
volatile int edge_capture; 
char *pathfile; 
 
int toggle = 0; 
int play =1; 
int skip =0; 
 
void write_audio(alt_up_audio_dev *audio_dev); 
void save_files (char* path); 
void clear_lcd_screen(); 
void write_string_to_lcd(char *text, int x, int y); 
void sd_load(char *path); 
unsigned int getSize(char *path); 
void save_buffer(char* path); 
void key1_isr(); 
void key2_isr(); 
void get_files (); 
 
int main() { 
 
 printf("+-- Loaded System Successfully! --+ \n"); 
 write_string_to_lcd("WELCOME TO ",0,0); 
 write_string_to_lcd("NIOSIIPOD ...",0,1); 
 int pindex=0; 
 
 alt_up_audio_dev *audio_config; 
 alt_up_audio_dev *audio_dev; 
    audio_config = alt_up_av_config_open_dev("/dev/AV_Config"); 
 
    alt_up_av_config_reset(audio_config); 
    while(!alt_up_av_config_read_ready(audio_config)); 
    { 
    } 
    audio_dev = alt_up_audio_open_dev ("/dev/Audio_Subsystem_Audio"); 
    if (audio_dev == NULL) 
     printf("Error: could not open audio device\n"); 
    else 
     printf("Opened audio device\n"); 
    alt_up_audio_reset_audio_core(audio_dev); 
 
 
    get_files(); 
    clear_lcd_screen(); 
16 
 write_string_to_lcd("Initializing",0,0); 
 write_string_to_lcd("Wait ...",0,1); 
 save_buffer(files[pindex]); 
 clear_lcd_screen(); 
    write_string_to_lcd("SW1 to Play",0,0); 
    write_string_to_lcd("Key 3 to SKIP",0,1); 
 
    while(1) 
    { 
     if(IORD_ALTERA_AVALON_PIO_DATA(SLIDER_SWITCHES_BASE)& 0b1) 
     { 
 
      if(play) 
      { 
       printf("write\n"); 
       clear_lcd_screen(); 
       write_string_to_lcd("PLAYING: ",0,0); 
       write_string_to_lcd(files[pindex],0,1); 
       write_audio(audio_dev); 
       printf("%s\n",files[pindex]); 
       play = 0; 
      } 
     } 
     else if((IORD_ALTERA_AVALON_PIO_DATA(SLIDER_SWITCHES_BASE)==0)) 
     { 
      if(!play) 
      { 
       play = 1; 
 
       printf("turned off\n"); 
       alt_up_audio_reset_audio_core(audio_dev); 
      } 
     } 
 
     if(IORD_ALTERA_AVALON_PIO_DATA(KEYS_BASE)& 0b1000) 
     { 
 
           if(skip) 
           { 
            if(pindex >2) 
            { 
             pindex = 0; 
            } 
            else { 
            pindex++; 
 
            printf("skipping\n"); 
            } 
            skip = 0; 
            clear_lcd_screen(); 
            write_string_to_lcd("Initializing",0,0); 
            write_string_to_lcd("Wait ...",0,1); 
               save_buffer(files[pindex]); 
               clear_lcd_screen(); 
               write_string_to_lcd("SW1 to Play",0,0); 
               write_string_to_lcd("Key 3 to SKIP",0,1); 
17 
           } 
 
     } 
     else if(IORD_ALTERA_AVALON_PIO_DATA(KEYS_BASE) == 0) 
     { 
      if(!skip) 
       { 
        skip = 1; 
       } 
     } 
 
 
 
 
    } 
 
    printf("end:%u\n",k); 
    free(soundBuff); 
    return 0; 
} 
 
void get_files () 
{ 
    alt_up_sd_card_dev * sd_card; 
    sd_card = alt_up_sd_card_open_dev("/dev/SD_Card"); 
 
    if (sd_card!=NULL){ 
     if (alt_up_sd_card_is_Present()){ 
      printf("An SD Card was found!\n"); 
     } 
     else { 
      printf("No SD Card Found. \n Exiting the program."); 
     } 
     if (alt_up_sd_card_is_FAT16()){ 
      printf("FAT-16 partiton found!\n"); 
     } 
     else{ 
      printf("No FAT-16 partition found - Exiting!\n"); 
     } 
     printf("The SD Card contains the following files:\n"); 
     //Call find_files on the root directory 
     save_files ("."); 
     } 
 
} 
void save_files (char* path){ 
 char filepath [90]; 
 char filename [15]; 
 char fullpath [104]; 
 char* folders [MAX_SUBDIRECTORIES]; 
 int num_dirs = 0; 
 short int file; 
 short int attributes; 
 bool foundAll; 
 
 //copy the path name to local memory 
18 
 strcpy (filepath, path); 
 
 foundAll = (alt_up_sd_card_find_first(filepath,filename) == 0 ? false : true); 
 
 //output the current directory 
 
 //loop through the directory tree 
 while (!foundAll){ 
  strcpy (fullpath,filepath); 
  //remove the '.' character from the filepath (foo/bar/. -> foo/bar/) 
  fullpath [strlen(filepath)-1] = '\0'; 
  strcat (fullpath,filename); 
 
  file = alt_up_sd_card_fopen (fullpath, false); 
  attributes = alt_up_sd_card_get_attributes (file); 
  if (file != -1) 
   alt_up_sd_card_fclose(file); 
 
  //print the file name, unless it's a directory or mount point 
  if ( (attributes != -1) && !(attributes & 0x0018)){ 
   files[findex] = malloc((strlen(fullpath) + 1) * sizeof(char)); 
   strcpy(files[findex],fullpath); 
   findex++; 
  } 
 
  //if a directory is found, allocate space and save its name for later 
  if ((attributes != -1) && (attributes & 0x0010)){ 
   folders [num_dirs] = malloc (15*sizeof(char)); 
   strcpy(folders[num_dirs],filename); 
   num_dirs++; 
  } 
 
  foundAll = (alt_up_sd_card_find_next(filename) == 0 ? false : true); 
 } 
 
 //second loop to open any directories found and call find_files() on them 
 int i; 
 for (i=0; i<num_dirs; i++){ 
 
  strcpy (fullpath,filepath); 
  fullpath [strlen(filepath)-1] = '\0'; 
  strcat (fullpath,folders[i]); 
  strcat (fullpath, "/."); 
  save_files(fullpath); 
  free(folders[i]); 
 } 
 
 return; 
} 
 
void clear_lcd_screen() { 
    // Send clear screen command 
    *(LCD_display_ptr) = 0x01;  // Command for clear screen 
 
    // Wait for the clear operation to complete (optional, you may need to check your LCD's specifications) 
    // You can use a delay function or a busy-wait loop here 
19 
} 
 
void write_string_to_lcd(char *text, int x, int y) { 
    char instruction; 
 
    instruction = x; 
    if (y != 0) 
        instruction |= 0x40; // set bit 6 for bottom row 
    instruction |= 0x80;     // need to set bit 7 to set the cursor location 
 
    *(LCD_display_ptr) = instruction; // write to the LCD instruction register 
 
    while (*text) { 
        *(LCD_display_ptr + 1) = *text; // write to the LCD Data register 
        ++text; 
    } 
} 
 
void sd_load(char *path) 
{ 
 alt_up_sd_card_dev * sd_card; 
 sd_card = alt_up_sd_card_open_dev("/dev/SD_Card"); 
 
 printf("%s\n",path); 
 
 
  if (sd_card!=NULL){ 
  if (alt_up_sd_card_is_Present()){ 
   printf("An SD Card was found!\n"); 
  } 
  else { 
   printf("No SD Card Found. \n Exiting the program."); 
   perror("error"); 
  } 
 
   if (alt_up_sd_card_is_FAT16()){ 
   printf("FAT-16 partiton found!\n"); 
  } 
  else{ 
   printf("No FAT-16 partition found - Exiting!\n"); 
   perror("error"); 
  } 
 
 
 
 } 
  save_buffer(path); 
  printf("Successful SD_LOAD\n"); 
} 
 
unsigned int getSize(char *path) 
{ 
 short int temp_size[8]; 
 short int dataRead; 
 dataRead = alt_up_sd_card_fopen(path, false); 
 
20 
 if(dataRead == -1){ 
  perror("dataRead/ERROR:"); 
 } 
 for(int i = 0; i < 8; i++) 
 { 
  temp_size[i] = alt_up_sd_card_read(dataRead); 
 } 
 
 int size = (temp_size[7] << 24) | (temp_size[6] << 16) | (temp_size[5] << 8)| temp_size[4]; 
 alt_up_sd_card_fclose(dataRead); 
 printf("Successful getSize\n"); 
 return size - 35; 
 
} 
 
void save_buffer(char* path) 
{ 
 int file_size = getSize(path); 
 
 short int file = alt_up_sd_card_fopen(path,false); 
 
 if(file==-1){ 
  printf("no file\n"); 
 } 
 
 printf("file_size %u \n", file_size); 
 f_size = file_size; 
 
 short int temp; 
 for(int i = 0; i<44;i++) 
 { 
  temp = alt_up_sd_card_read(file); 
 } 
 
 for(int i = 0; i<f_size;i++) 
 { 
     soundBuff[i / 4] = (unsigned int)alt_up_sd_card_read(file); 
     soundBuff[i / 4] |= (unsigned int)alt_up_sd_card_read(file) << 8; 
     soundBuff[i / 4] |= (unsigned int)alt_up_sd_card_read(file) << 16; 
     soundBuff[i / 4] |= (unsigned int)alt_up_sd_card_read(file) << 24; 
 } 
 
 
 alt_up_sd_card_fclose(file); 
 printf("Successful SAVE_BUFFER\n"); 
} 
 
void key1_isr() 
{ 
 printf("Key1 Success\n"); 
 strcpy(pathfile, "1KHZ.WAV"); 
 printf("%s\n",pathfile); 
 display = 1; 
 save_buffer(pathfile); 
 
} 
21 
 
void key2_isr() 
{ 
 printf("Key2 Success\n"); 
 strcpy(pathfile,"BURST.WAV"); 
 printf("%s\n",pathfile); 
 display = 1; 
 save_buffer(pathfile); 
} 
 
void write_audio(alt_up_audio_dev *audio_dev) 
{ 
 int index = 0; 
 unsigned long temp_r[128]; 
 int fifospace = alt_up_audio_write_fifo_space(audio_dev, ALT_UP_AUDIO_RIGHT); 
 
 while(index < f_size) 
   { 
    if(fifospace == 128){ 
     for(int i = 0; i< 128; i++) 
      { 
       temp_r[i] = soundBuff[index] << 9; 
       index++; 
      } 
    alt_up_audio_write_fifo (audio_dev, &(temp_r[index]),128, 
ALT_UP_AUDIO_RIGHT); 
    alt_up_audio_write_fifo (audio_dev, &(temp_r[index]),128, 
ALT_UP_AUDIO_LEFT); 
 
    } 
   } 
   printf("%u \n",index); 
   index = 0; 
}