### CONTRIBUTING GUIDE
- Please fork the `dev` branch and not the `master` branch
    - The `master` branch is reserved for working releases and is only updated when the version is incremented
- Please use the coding style/format represented in the rest of the code and 4 spaces for tabbing<br>
    Example:
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
        if (cond4) {f1(); f2();}
        if (cond5) f3();
        else f4();
        if (cond6) {f5();}
        else {f6();}
        // nested 'for'
        for (int i = 0; i < 256; ++i) {
            for (int j = 0; j < 16; ++j) {
                func(i, j);
            }
        }
        return 0;
    }
    ``` 
- Don't contribute mallicious code
- Try not to pull in more external dependencies unless they can be statically linked
    - The less the amount of library binaries that need to be included, the better
- Try not to use/add large or bloated dependencies
    - Most of the time it's not really needed, and the functionality can probably be implemented locally with a bit of work
    - It will almost certainly hurt performance
    - It will increase project download size
- Do not use any other languages besides C (no C++, no Rust, etc) for anything under `src/*/`
