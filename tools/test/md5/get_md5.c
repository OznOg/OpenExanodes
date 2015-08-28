/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include    <fcntl.h>
#include    <stdio.h>
#include <stdlib.h>
#include    <unistd.h>
#include    "md5.h"
#include    <string.h>
#include    <sys/stat.h>

/* FIXME Should get rid of atoi() but buffers allocated on the stack depend
         on 'taillebloc', and I don't want to modify this now... */

/*
 * read the file caculate the checksum and compare with the one write on it
 */
int main(int argc, char *argv[])
{
    int file;
    char * filename = argv[1];
    int taillebloc = atoi(argv[2]);
    int tailleblocFin;
    int status = 0;
    int di,i,j;
    char buffer[taillebloc];
    char buffer2[taillebloc];
    int buf1size,buf2size;
    char hex_output[16*2 + 1];
    char hex_end[16*2+1];
    struct stat     sts;

    /* Open the file*/
    if((file = open(filename, O_RDONLY)) < 0) {
        printf("Error Opening File: %s\n",filename);
        return(2);
    }
    fstat (file, & sts);

    memset (buffer,'\0', sizeof (buffer));
    memset (buffer2,'\0', sizeof (buffer2));

    for ( j = 0 ; j < ((sts.st_size - 32) / taillebloc); j++){
        md5_state_t state;
        md5_byte_t digest[16];

        /* Reading */
        buf1size = read(file,buffer,taillebloc);
        if (buf1size <= 0) {
            printf("ERROR reading\n");
        }
        /* Check sum */
        md5_init(&state);
        md5_append(&state, (const md5_byte_t *)buffer, buf1size);
        md5_finish(&state, digest);
        for (di = 0; di < 16; ++di)
            sprintf(hex_output + di * 2, "%02x", digest[di]);
    }

    tailleblocFin = sts.st_size - (taillebloc*j);

    /* Manage the end of the file */
    if(tailleblocFin < taillebloc)
    {

        buf1size = read(file,buffer,tailleblocFin);
        if (buf1size <= 0) {
            printf("ERROR reading\n");
        }

        /* All the check sum is in buffer2 */
        buf1size -=32;
        for( i = 0; i < 32; i++)
        {
            hex_end[i] = buffer[buf1size + i];
        }

    }
    else
    {
        int partsize;
        buf1size = read(file,buffer,taillebloc);
        buf2size = read(file,buffer2,taillebloc);
        /* get the part of the checksum witch is in buffer1 */
	partsize = 32 - buf2size;
        for( i = 0; i < partsize; i++)
        {
            hex_end[i] = buffer[buf1size - partsize + i];
        }
        buf1size = buf1size - partsize;
        /* get the part of the checksum witch is in buffer2 */
        for ( ; i < 32 ; i++)
        {
            hex_end[i] = buffer2[i - partsize];
        }

    }
    hex_end[32] = '\0';
    if (buf1size){
         /* Check sum */
        md5_state_t state;
        md5_byte_t digest[16];
        md5_init(&state);
        md5_append(&state, (const md5_byte_t *)buffer, buf1size);
        md5_finish(&state, digest);
        for (di = 0; di < 16; ++di)
            sprintf(hex_output + di * 2, "%02x", digest[di]);
    }

    /* Result */
    if (strcmp(hex_output,hex_end) == 0)
    {
        status = 1;
        puts("md5 self-test completed successfully.");
    }
    else
    {
        status = 3;
        puts("md5 self-test failed.");
//      printf("hex_output: %s\n",hex_output);
//      printf("hex_end:    %s\n",hex_end);
    }

    close(file);
    return status;
}
