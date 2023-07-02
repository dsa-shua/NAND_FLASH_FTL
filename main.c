#include "memory.h"
int main(void){
    init();
    
    printNAND();
    write(4,1);
    // printNAND();
    write(4,2);
    // printNAND();
    write(5,5);
    write(6,6);
    write(7,7);
    write(4,4);

    write(1,1);
    write(1,10);
    write(8,8);
    write(8,80);
    write(4,40);

    dealloc();
    return 0;
}