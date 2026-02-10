#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int eval(char *);

int get_file_size(FILE* file) {
    int size;
    fseek(file, 0, SEEK_END); // Go to the end of the file
    size = ftell(file);       // Get the current position (which is the size)
    fseek(file, 0, SEEK_SET); // Go back to the beginning
    return size;
}

int main(int argc, char **argv) {
    if(argc != 2) {
      printf("ERROR: must call the target with exactly one file as a command line argument.\n");
      return 1;
    }
    
    // Open the file in binary read mode ("rb")
    FILE * file_ptr = fopen(argv[1], "rb");
    if (file_ptr == NULL) {
        perror("Error opening file");
        exit(-1);
    }

    // Determine the file size and allocate memory
    int file_size = get_file_size(file_ptr);
    if (file_size == -1) {
        perror("Error getting file size");
        fclose(file_ptr);
        exit(-1);
    }

    // Allocate memory
    char * buffer = (char *)malloc(file_size);
    if (buffer == NULL) {
        perror("Error allocating memory");
        fclose(file_ptr);
        exit(-1);
    }

    // Read the entire file contents into the buffer
    size_t bytes_read = fread(buffer, 1, file_size, file_ptr);
    if (bytes_read != file_size) {
        fprintf(stderr, "Error reading file, expected %d bytes, read %zu\n", file_size, bytes_read);
        free(buffer);
        fclose(file_ptr);
        exit(-1);
    }

    // Close the file
    fclose(file_ptr);

    return eval(buffer);
}

int eval(char * input) {
  // 62 possible characters
  if(input[0] == 'q')
    if(input[1] == 'a')
      if(input[2] == 'M') {
 /*       if(input[3] == '2')
          if(input[4] == 'x')
            if(input[5] == 'W')
              if(input[6] == 'Z')
                if(input[7] == 'E')
                  if(input[8] == 'd')
                    if(input[9] == 'r')
                      if(input[10] == '8')
                        if(input[11] == 'b')
                          if(input[12] == '1')
                            if(input[13] == '4')
                              if(input[14] == 'f')
                                if(input[15] == 'n')
                                  if(input[16] == 'f')
                                    if(input[17] == 'F')
                                      if(input[18] == 'I')
                                        if(input[19] == 'H')
                                          if(input[20] == 'I')
                                            if(input[21] == '5')
                                              if(input[22] == 'A')
                                                if(input[23] == 'a')
                                                  if(input[24] == '8')
                                                    if(input[25] == 'z')
                                                      if(input[26] == 'W')
                                                        if(input[27] == 'e')
                                                          if(input[28] == 'I')
                                                            if(input[29] == 'M')*/
                                                                __builtin_trap();//*((int *)0) = 15;
}
  
  return 0;
}
