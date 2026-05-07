# WebAssembler support for C

There is support for C, you can write C code and convert into WebAssembler instructions upon executing the c2wa.c

> [!WARNING]
> There isnt full support for C yet. <br>
> Use with causion in production

## Syntax Diffrence
*Be aware that there can and will be changes and improvements*
<br>
You cannot begin with a function unless it is a reusable function, for instance
```C
#include <stdio.h>

int main() { // Unwanted instance; Will loop itself.

}
```
Because when the transpiler does its thing, it returns back to the begining since the return happened. <br>
Causing a continuis loop. <br>

Fix for this is to just not use a function
```C
// Correct
#include <stdio.h>

printf("Hello World");
```
