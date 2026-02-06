#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int eval(char *);

int main(int argc, char **argv) {
    if(argc != 2) {
      printf("ERROR: must call the target with exactly one string command line argument.\n");
      return 1;
    }
    
    return eval(argv[1]);
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
