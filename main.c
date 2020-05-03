
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // execl
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <errno.h>
#include <string.h>

//  https://eli.thegreenplace.net/2011/01/23/how-debuggers-work-part-1
//  https://stackoverflow.com/questions/23131773/how-can-i-insert-int3-with-ptrace-in-ubuntu-x64


// Definirajte si strukturo, v katero boste shranjevali bajte inštrukcij  katere boste spreminjali v int3 (0xcc)
// Predlagam da uporabite enosmerno povezan seznam.
long bytes[10];

int main() {
    pid_t pid;

    pid = fork();

    /*
     *  SIN
     */
    if (pid == 0) {
        printf("Jaz sem sin\n");

        //Proces, ki ga zaženemo v sinu želimo slediti

        // Omogočimo sledenje procesu (zadnji trije argumenti so odveč)
        if (ptrace(PTRACE_TRACEME, 0, 0, 0) < 0) {  //ptrace(...);
            perror("ptrace:");
            return -1;
        }

        // Sliko trenutnega procesa zamenjamo s novo:
        execl("demo", "demo",(char*)NULL);      //execl(...);

        // Also, all subsequent calls to exec() by this process will cause a SIGTRAP to be sent to it,
        // giving the parent a chance to gain control before the new program begins execution
    }
        /*
         *  OČE
         */
    else {
        printf("Jaz sem oče\n");

        int current_bp = 0;
        int num_of_bps = 0;
        int action = 0;
        int status; // za meni
        struct user_regs_struct regs; // za manipulacijo registrov

        // Počakamo, da se process v sinu ustavi, da ga lahko manipulirano
        wait(&status);     //wait(...);

        /*
         *  Manipulacija
         */
        while(WIFSTOPPED(status)) { //dokler se program ne zaključi
            printf("Vnesi akcijo (-1 to esc)\n");
            scanf("%d", &action);

            /*
             *  Nadaljuj do bp
             */
            if (action == 1) {
                // Nadaljujemo izvajanje do ustavitve
                ptrace(PTRACE_CONT, pid, 0, 0);//CONT

                // Počakamo na ustavitev
                wait(&status);

                // Procesiramo wait status
                if (WIFEXITED(status)) {
                    printf("Otrok se je prenehal izvajati\n");
                    return 0;
                }
                else if (WIFSTOPPED(status)) {

                    // Preberemo registre
                    ptrace(PTRACE_GETREGS, pid, 0, &regs);
                    long data = ptrace(PTRACE_PEEKTEXT, pid, regs.rip - 1, 0);
                    printf("Izvajanje otroka je bilo prekinjeno (%d)\nBreakpoint najden na:\n 0x%08x: 0x%08x\n", WSTOPSIG(status), regs.rip - 1, data);


                    // Ponastavimo instrukcijo in instruction pointer
                    regs.rip-=1;
                    ptrace(PTRACE_POKETEXT, pid,  regs.rip, bytes[current_bp]);
                    ptrace(PTRACE_SETREGS, pid, 0, &regs);

                    // Pogledamo pravilnost ponastavljanja
                    ptrace(PTRACE_GETREGS, pid, 0, &regs);
                    long newData = ptrace(PTRACE_PEEKTEXT, pid, regs.rip, 0);
                    printf("Instrukcija ponastavljena na:\n 0x%08x: 0x%08x\n", regs.rip, newData);

                    // povečamo števec trenutnega breakpointa
                    current_bp++;
                }
                else {
                    perror("wait");
                    return -1;
                }
            }

            /*
            *  Siglestep
           */
            else if (action == 4) {

                // Nadaljujemo izvajanje do naslednje instrukcije
                ptrace(PTRACE_SINGLESTEP,pid, 0, 0);//CONT

                // Počakamo na ustavitev
                wait(&status);

                // Procesiramo wait status
                if (WIFEXITED(status)) {
                    printf("Otrok se je prenehal izvajati\n");
                    return 0;
                }
                else if (WIFSTOPPED(status)) {
                    // Preberemo registre
                    ptrace(PTRACE_GETREGS, pid, 0, &regs);
                    printf("(%d):\t 0x%08x", WSTOPSIG(status), regs.rip);
                }
                else {
                    perror("wait");
                    return -1;
                }

            }
                /*
                 *  Nastavi bp
                 */
            else if (action == 2) {
                // TODO: use -  0x4004fd(e8), 0x400509(e8), 0x400515(e8)             0x4004e7 je zacetek
                long addr;
                while(1) {

                    // Preberi naslov
                    printf("Naslov (-1 to esc): ");
                    scanf(" %x",&addr);
                    if (addr == 0xffffffff) break;

                    // Preberi inštrukcijo na naslovu in jo shrani
                    long data = ptrace(PTRACE_PEEKTEXT, pid, addr, 0);
                    bytes[num_of_bps++] = data; //(unsigned char *)(data & 0xff);

                    // Zapiši 0xcc v prvi bajt inštrukcije na naslovu
                    ptrace(PTRACE_POKETEXT, pid, addr, ((data & 0xFFFFFF00) | 0xCC));

                    // Izpis na stout
                    printf("Spremembe:\n");
                    printf("0x%08x: 0x%08x\n", addr, data);
                    data = ptrace(PTRACE_PEEKTEXT, pid, addr, 0);
                    printf("0x%08x: 0x%08x\n", addr, data);
                }
            }

            /*
             *  Izpiši registre
             */
            else if (action == 3) {
                //izpiši vrednosti registrov na trenuntnem bp-ju
                // preberemo še enkrat
                ptrace(PTRACE_GETREGS, pid, 0, &regs);
                // izpišemo
                printf("Trenutna vsebina GPR-jev:\n");
                printf("IP: 0x%08x\n", regs.rip);
                printf("AX: 0x%08x\n", regs.rax);
                printf("CX: 0x%08x\n", regs.rcx);
                printf("RX: 0x%08x\n", regs.rdx);
                printf("BX: 0x%08x\n", regs.rbx);
                printf("SP: 0x%08x\n", regs.rsp);
                printf("BP: 0x%08x\n", regs.rbp);
                printf("SI: 0x%08x\n", regs.rbp);
                printf("DI: 0x%08x\n", regs.rdi);
            }

            /*
            *  Exit
            */
            else if (action == -1) {
                return 0;
            }

            //wait(&status);
        }
    }




    return 0;
}