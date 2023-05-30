### CONTRIBUTING GUIDE
- Please use the coding style/format represented in the rest of the code:<br>
    - Use 4-space tabbing and put the pointer asterisks in the type (`void* ptr` instead of `void *ptr`)
        ```c
        int main(int argc, char** argv) {
            // variations of 'if' statements 
            if (cond1) {
                do_something();
            } else {
                something_else();
            }
            if (cond2) a_function(&var, sizeof(var));
            if (cond3) {
                some_func(argc, argv[0], var, var2, var3, &out);
                #if DBGLVL(3)
                printf("argv0: {%s}, out: [%d]\n", argv[0], out);
                #endif
            }
            // nested 'for'
            for (int i = 0; i < 256; ++i) {
                for (int j = 0; j < 16; ++j) {
                    func(i, j);
                }
            }
            // variations of 'switch' statements
            int val = another_func();
            switch (val) {
                default:;
                    val = 4;
                    break;
                case 2:;
                    val = 7;
                    break;
                case 5:;
                    val = 10;
                    break;
            }
            switch (val) {
                default:; {
                    struct mystruct state;
                    teststart(&state);
                    test(&state, val, 1);
                    testend(&state);
                } break;
                case 0:; {
                    printf("val -> mangle: [%d]\n", (val = testmangle(rand(), 0)));
                    dotest(testflip(val));
                } break;
                case 3:; {
                    dotest(test1(42));
                } break;
                case 6:; {
                    dotest(test2(rand(), 10));
                    dotest(test2(rand(), 14));
                    dotest(test2(rand(), 32));
                } break;
            return 0;
        }
        ``` 
    - Try not to split lines until around a 140-150 character width
    - Write in a way compatible with C99
    - For headers (.h), use the following layout:
        ```c
        #ifndef [FOLDER]_[BASE FILENAME]_H
        #define [FOLDER]_[BASE FILENAME]_H
        
        [Local includes]
        
        [System includes]
        
        [Macros, typedefs, structs, and enums (seperate groups with newlines)]
        
        [Function declarations]
        
        [Extern variable declarations]
        
        #endif
        ```
    - For code (.c), the following layout is recommended:
        ```c
        #include <main/main.h>
        #include "[BASE FILENAME].h"
        [More local includes]
        
        [System includes]
        
        [Code]
        ```
- Do not contribute mallicious code
- Try not to pull in more external dependencies unless they can be statically linked or they are widely available
- Try not to use/add large or bloated dependencies
- Do not use any other languages besides C (no C++, no Rust, etc) for anything under `src/*/`
