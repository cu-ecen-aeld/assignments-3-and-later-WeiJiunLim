/* File        : writer.c
 * Author      : Wei Jiun Lim
 * Descriptiom : For AELD Assignment 2 */

#include <stdio.h>
#include <syslog.h>

int main(int argc, char *argv[]) {

    FILE *fp;
    char* writefile;
    char* writestr;

    /* Open syslog */
    openlog(argv[0], LOG_CONS | LOG_PID, LOG_USER);

    /* Check for correct number of arguments:
     * 0 = program name
     * 1 = writefile
     * 2 = writestr */
    if (argc != 3) {
        // printf("Usage: %s <argument1> <argument2>\n", argv[0]);
        syslog (LOG_ERR, "Usage: %s <argument1> <argument2>", argv[0]);
        return 1;
    }

    /* Access the provided arguments */
    writefile = argv[1];
    writestr = argv[2];

    // printf("writefile: %s\n", writefile);
    // printf("writestr: %s\n", writestr);

    /* Open file, or create new file if it does not exist */
    fp = fopen(writefile, "w");

    if (fp == NULL) {
        // printf("Error opening file %s!\n", writefile);
        syslog (LOG_ERR, "Error opening file %s!", writefile);
        return 1; 
    }

    /* Write content to file */
    if (fprintf(fp, "%s", writestr) < 0) {
        // printf("Error writing file %s!\n", writefile);
        syslog(LOG_ERR, "Error writing file %s!", writefile);
        return 1;        
    }

    /* Close the file */
    if (fclose(fp) < 0) {
        // printf("Error closing file %s!\n", writefile);
        syslog(LOG_ERR, "Error closing file %s!", writefile);
        return 1;        
    }

    /* Write successful */
    syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);

    return 0; 
}
