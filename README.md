# Text Muncher

C program that reads lines from a file, numbers them and appends their lengths to the end, then prints them out. It does these four steps in four separate threads that are coordinated using mutexes and spinlocks.

### Usage

Warning: you must include the `-pthread` argument in gcc for this program to compile.

```sh
curl https://raw.githubusercontent.com/apc518/parallel-muncher/master/muncher.c > muncher.c
gcc -pthread muncher.c -o munch
./munch some-text-file.txt 4
```

Calling the program will have the format `./munch [input_text_file] [number_of_threads]`

### Known Issues
- Last character of input file gets ignored in output

This was written for a homework assignment in CS 475: Operating Systems, at the University of Puget Sound.