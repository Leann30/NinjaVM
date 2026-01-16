#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define MAXITEMS 100 

#define HALT 0
#define PUSHC 1
#define ADD 2
#define SUB 3
#define MUL 4
#define DIV 5
#define MOD 6
#define RDINT 7
#define WRINT 8
#define RDCHR 9
#define WRCHR 10 
#define PUSHG 11
#define POPG 12
#define ASF 13
#define RSF 14
#define PUSHL 15
#define POPL 16
#define EQ 17
#define NE 18
#define LT 19
#define LE 20
#define GT 21
#define GE 22
#define JMP 23
#define BRF 24
#define BRT 25
#define CALL 26
#define RET 27
#define DROP 28
#define PUSHR 29
#define POPR 30
#define DUP 31

#define VERSION 5

//Sonderfälle negative Immediate-Werte
#define OPCODE(x) (x >> 24)  //8 Bit nach rechts schieben (obere Bits sind Opcode)
#define IMMEDIATE(x) ((x) & 0x00FFFFFF) //unterste 24 Bit, weil sonst Vorzeichen in Opcode
#define SIGN_EXTEND(i) ((i) & 0x00800000 ? (i) | 0xFF000000 : (i)) 
/*übergebe 24 Bit immediate
prüfe mit bitweiser Verundung ob bit 23 gesetzt ist, dann minus*/
#define STACK_SIZE 100


//Objekt im Heap
typedef struct {
    unsigned int size; // # byte of payload
    unsigned char data[]; // payload data, size as needed!
} Object;  /* Objekt im Stack*/

//typedef int Object;
typedef Object *ObjRef;

//STACK//
int sp=0;

//Slot auf Stack
typedef struct {
    bool isObjRef;
    union {
        ObjRef objRef; //Rechenobjekt im heaü
        int number; //Verwaltungsdaten(fp, ra)
    } u;
} StackSlot;

StackSlot stack[STACK_SIZE];
/*-------------------------------------
isObjRef == true: stack[sp].u.ObjRef
isObjRef == false: stack[sp].u.number
-------------------------------------*/


//framepointer
int fp=0;

//PROGRAMMSPEICHER//
int pc=0;
int *program_memory;

//--GLOBALE VARIABLEN--
int gc = 0;

// SDA

ObjRef *sda; 

// RVR - Register für Rückgabewerte
ObjRef rv; // single object

//INFOS AUS BIN DATEI
int instructionNumber = 0;
int varNumber = 0;

//SPEZIALREGISTER
int sr = 0;

//STACK OPERATIONEN//
void push(int x) {
    stack[sp].isObjRef = false;
    stack[sp].u.number = x;
    sp++;
}

void pushc(int x){
    push(x);
}

int pop(void) {
    sp--; 
    if (sp < 0) { 
        printf("error: stack underflow\n");
        exit(1);
    }
    
    if(stack[sp].isObjRef){
        printf("expected: number, found: ObjRef\n");
        exit(1);
    }
    return stack[sp].u.number;
}

void push_obj(int x){

    ObjRef intObject;
    //Größe berechnen
    unsigned int objSize = sizeof(*intObject) + (sizeof(int));

    if ((intObject = malloc(objSize)) == NULL) {
        perror("malloc");
        exit(0);
    }
    //Objekt befüllen
    intObject->size=sizeof(int);
    *(int *)intObject->data = x;
    //Auf Stack legen
    stack[sp].isObjRef = true;
    stack[sp].u.objRef = intObject;
    sp++;
}

ObjRef pop_obj(void) {
    sp--;
    if (sp < 0) {
        fprintf(stderr, "Error: Stack underflow\n");
        exit(1);
    }
    if (!stack[sp].isObjRef) {
        fprintf(stderr, "Error: Expected ObjRef at stack[%d], but found number\n", sp);
        exit(1);
    }
    return stack[sp].u.objRef;
}
void pushg(int addr){

    //--ADRESSE AUßERHALB/ KEINE VARIABLE IM SPEICHER--
    if(addr < 0 || addr >= varNumber){
        printf("error: array out of bounds");
        exit(0);
    }
    ObjRef ref = sda[addr];
    //n-tes Element aus sda auf stack
    stack[sp].isObjRef = true;
    stack[sp].u.objRef = ref;
    sp++;
}
//value als n-tes element in sda
void popg(int addr){ 
    ObjRef value = pop_obj();
    if(addr < 0 || addr >= varNumber) {
        printf("error: index out of bounds\n");
        exit(0);
    }
    sda[addr] = value;
}

void pushl(int n){
    int varPos = fp+n;
    
    //wenn innerhalb des frames
    if(varPos < 0 || varPos >= sp){
        printf("error: stack out of bounds\n");
        exit(0);
    }
    if(!stack[varPos].isObjRef){
        printf("expected: ObjRef");
    }
    //Referenz kopieren
    stack[sp] = stack[varPos];
    sp++;
}

void popl(int n){
    int varPos = fp+n;
    ObjRef value = pop_obj();
    stack[varPos].isObjRef = true;
    stack[varPos].u.objRef = value;
}

//--VERWALTUNG DER FRAMES--
//speicher für lokale variablen
void asf(int n){
    pushc(fp);
    fp = sp;
    sp = sp + n;
}
//entfernen des aktuellen stackframes, rückkehr zum vorheringen
void rsf(void){ 
    sp = fp;
    fp = pop();
}

void print_program(int length, int* program_memory){

    for(int i = 0; i < length; i++){

        unsigned int instruction = program_memory[i];
        int opcode = OPCODE(instruction);
        int immediate = SIGN_EXTEND(IMMEDIATE(instruction));
        printf("%03d:\t", i);

        switch(opcode){
        case 0:
            printf("halt");
            break;
        case 1:
            printf("pushc\t%d", immediate);
            break;
        case 2: 
            printf("add");
            break;
        case 3:
            printf("sub");
            break;
        case 4:
            printf("mul");
            break;
        case 5:
            printf("div");
            break;
        case 6:
            printf("mod");
            break;
        case 7:
            printf("rdint");
            break;
        case 8: 
            printf("wrint");
            break;
        case 9:
            printf("rdchr");
            break;
        case 10:
            printf("wrchr");
            break;
        case 11:
            printf("pushg\t%d", immediate);
            break;
        case 12:
            printf("popg\t%d", immediate);
            break;
        case 13:
            printf("asf\t%d", immediate);
            break;
        case 14:
            printf("rsf");
            break;
        case 15:
            printf("pushl\t%d", immediate);
            break;
        case 16:
            printf("popl\t%d", immediate);
            break;
        case 17:
            printf("eq");
            break;
        case 18:
            printf("ne");
            break;
        case 19:
            printf("lt");
            break;
        case 20:
            printf("le");
            break;
        case 21:
            printf("gt");
            break;
        case 22:
            printf("ge");
            break;
        case 23:
            printf("jmp\t%d", immediate);
            break;
        case 24:
            printf("brf\t%d", immediate);
            break;
        case 25:
            printf("brt\t%d", immediate);
            break;
        case 26:
            printf("call\t%d", immediate);
            break;
        case 27:
            printf("ret");
            break;
        case 28:
            printf("drop\t%d", immediate);
            break;
        case 29:
            printf("pushr");
            break;
        case 30:
            printf("popr");
            break;
        case 31:
            printf("dup"); 
    }
    printf("\n");
    }
}

//INSTRUKTIONEN//

void halt(void){
    printf("Ninja Virtual Machine stopped\n");
    exit(0);
}

void add(void){
   ObjRef obj2 = pop_obj();
   ObjRef obj1 = pop_obj();
   int n2 = *(int*)(obj2->data);
   int n1 = *(int*)(obj1->data);
   int res = n1 + n2;
   push_obj(res);
}

void sub(void){
   ObjRef obj2 = pop_obj();
   ObjRef obj1 = pop_obj();
   int n2 = *(int*)(obj2->data);
   int n1 = *(int*)(obj1->data);
   int res = n1 - n2;
   push_obj(res);
}

void mul(void){
   ObjRef obj2 = pop_obj();
   ObjRef obj1 = pop_obj();
   int n2 = *(int*)(obj2->data);
   int n1 = *(int*)(obj1->data);
   int res = n1 * n2;
   push_obj(res);
}

void division(void){
   ObjRef obj2 = pop_obj();
   ObjRef obj1 = pop_obj();
   int n2 = *(int*)(obj2->data);
   int n1 = *(int*)(obj1->data);
    if(n2 != 0){
        int res = n1 / n2;
        push_obj(res);
    } else {
        printf("Division durch 0 nicht möglich.");
        exit(0);
    }
}

void mod(void){
   ObjRef obj2 = pop_obj();
   ObjRef obj1 = pop_obj();
   int n2 = *(int*)(obj2->data);
   int n1 = *(int*)(obj1->data);
 
   if(n2 != 0){
    int res = n1 % n2;
    push_obj(res);
   }
   else{
    printf("error: modulo by zero");
   }
}

void rdint(void){
    int n;
    printf("Gib eine Ganzzahl ein\n");
    if ((scanf("%d", &n)) != 1) {
        printf("Fehlerhafte Eingabe!\n");
        exit(0);
    }
    push_obj(n);
}

void wrint(void){
    ObjRef obj = pop_obj();
    int n = *(int*)(obj->data);
    printf("%d", n);
}

void rdchr(void){
    printf("welches Zeichen soll eingelesen werden?\n");
    char c = getchar();
    push_obj(c);
}

void wrchr(void){
    ObjRef obj = pop_obj();
    int a = *(int*)(obj->data);
    printf("%c", (char)a);
}

void eq(void){
    ObjRef obj2 = pop_obj();
    ObjRef obj1 = pop_obj();
    int n2 = *(int*)(obj2->data);
    int n1 = *(int*)(obj1->data);
    int res = 0;
    if(n2 == n1){
        res = 1;
    } 
    push_obj(res);
}

void ne(void){
    ObjRef obj2 = pop_obj();
    ObjRef obj1 = pop_obj();
    int n2 = *(int*)(obj2->data);
    int n1 = *(int*)(obj1->data);
    int res = 0;
    if(n2 != n1){
        res = 1;
    } 
    push_obj(res);
}

void lt(void){
    ObjRef obj2 = pop_obj();
    ObjRef obj1 = pop_obj();
    int n2 = *(int*)(obj2->data);
    int n1 = *(int*)(obj1->data);
    int res = 0;
    if(n1 < n2){
        res = 1;
    }
    push_obj(res);
}

void le(void){
    ObjRef obj2 = pop_obj();
    ObjRef obj1 = pop_obj();
    int n2 = *(int*)(obj2->data);
    int n1 = *(int*)(obj1->data);
    int res = 0;
    if(n1 <= n2){
        res = 1;
    } 
    push_obj(res);
}

void gt(void){
    ObjRef obj2 = pop_obj();
    ObjRef obj1 = pop_obj();
    int n2 = *(int*)(obj2->data);
    int n1 = *(int*)(obj1->data);
    int res = 0;
    if(n1 > n2){
        res = 1;
    }
    push_obj(res);
}

void ge(void){
    ObjRef obj2 = pop_obj();
    ObjRef obj1 = pop_obj();
    int n2 = *(int*)(obj2->data);
    int n1 = *(int*)(obj1->data);
    int res = 0;
    if(n1 >= n2){
        res = 1;
    } 
    push_obj(res);
}

void jmp(int target){
    if(target < 0 || target > instructionNumber){
        printf("error: impossible jump\n");
        exit(0);
    }
    pc = target;
}

//springe wenn value false
void brf(int target){
    int value = pop();
    if(value == 0){
        jmp(target);
    }
}
//springe wenn value true
void brt(int target){
    int value = pop();
    if(value != 0){
        jmp(target);
    }
}
//speicher Rücksprungadresse auf stack
void call(int n){
  pushc(pc);
  jmp(n);  
}
//kehre zur Rücksprungadresse zurück
void ret(void){
  int ra = pop();
  pc = ra;
}
//lösche n einträge vom stack
void drop(int n){
    while(n>0){
        pop();
        n--;
    }
}

void pushr(void){
    pushc(sr);
}

void popr(void){
    sr = pop();
}

void dup(void){
    if(sp <= 0){
    printf("error: stack out of bounds\n");
    exit(0);
    }
    stack[sp] = stack[sp-1];
    sp++;
}

//AUSGEWÄHLTES PROGRAMM LADEN & AUSFÜHREN//
/*
1. Programm in Speicher laden
2. Programm ausprinten
3. Programm ausführen
*/

void execute(unsigned int instr){
    //Instruktion dekodieren
    //Operation ausführen
    
    unsigned int opcode = OPCODE(instr);
    int immediate = SIGN_EXTEND(IMMEDIATE(instr));
    switch(opcode){
        case 0:
            halt();
            break;
        case 1:
            pushc(immediate);
            break;
        case 2: 
            add();
            break;
        case 3:
            sub();
            break;
        case 4:
            mul();
            break;
        case 5:
            division();
            break;
        case 6:
            mod();
            break;
        case 7:
            rdint();
            break;
        case 8: 
            wrint();
            break;
        case 9:
            rdchr();
            break;
        case 10:
            wrchr();
            break;
        case 11:
            pushg(immediate);
            break;
        case 12:
            popg(immediate);
            break;
        case 13:
            asf(immediate);
            break;
        case 14:
            rsf();
            break;
        case 15:
            pushl(immediate);
            break;
        case 16:
            popl(immediate);
            break;
        case 17:
            eq();
            break;
        case 18:
            ne();
            break;
        case 19:
            lt();
            break;
        case 20:
            le();
            break;
        case 21:
            gt();
            break;
        case 22:
            ge();
            break;
        case 23:
            jmp(immediate);
            break;
        case 24:
            brf(immediate);
            break;
        case 25:
            brt(immediate);
            break;
        case 26:
            call(immediate);
            break;
        case 27:
            ret();
            break;
        case 28:
            drop(immediate);
            break;
        case 29:
            pushr();
            break;
        case 30:
            popr();
            break;
        case 31:
            dup();
    }
}

void startPr(int length, int* program_memory){
    pc = 0;
    unsigned int instruction = program_memory[pc];

    //--vor Start Programm printen--
    //print_program(length, program_memory);

    //--ausführen--
    int opcode = OPCODE(instruction);
    while(opcode != 0) {
        instruction = program_memory[pc];
        pc++;
        opcode = OPCODE(instruction);
        execute(instruction);
    }
}

int main(int argc, char *argv[]) {
    
    if (argc < 2) {
        return 1;
    }

    printf("Ninja Virtual Machine started\n");
    
    if(argv[1] == NULL){
        printf("unknown command line argument %s, try 'njvm --help'\n", argv[1]);
        exit(0);
    } 
    else if (strcmp(argv[1], "--version") == 0) {
        printf("Ninja Virtual Machine version 4.0\n");
        
    } /*else if (strcmp(argv[1], "--debug") == 0) {

        FILE * fp = fopen(argv[2], "rb");
    
        if (fp == NULL) {
            perror("ERROR - fopen");
            exit(1);
        } else {
            printf("DEBUG: file %d loaded (code size = %d, data size = %d)", fp);
    } */
     else if (strcmp(argv[1], "--help") == 0) {

        //printf("--prog1     select program 1 to execute\n");
        //printf("--prog2     select program 2 to execute\n");
        //printf("--prog3     select program 3 to execute\n");
        //printf("--debug     start virtual machine in debug mode\n");
        printf("--version   show version and exit\n");
        printf("--help      show this help and exit\n");

    } else {
        FILE * fp=NULL;

        //--FEHLER BEIM ÖFFNEN ABFANGEN--
        fp = fopen(argv[1], "rb");
        if (fp == NULL) {
            perror("ERROR - fopen");
            exit(0);
        } else {

            fseek(fp, 0, SEEK_SET);
            char c[4];
            fread(c, 1, 4, fp);
            if(c[0] != 'N' && c[1] != 'J' && c[2] != 'B' && c[3] != 'F'){
                exit(0);
            }
            int version = 0;
            
            fread(&version, 1, 4, fp);
            if(version != VERSION){
                exit(0);
            } 
            fread(&instructionNumber, 1, 4, fp);
            fread(&varNumber, 1, 4, fp);

            gc = varNumber;

            //--SPEICHER RESERVIEREN--
            int *program_memory = malloc(instructionNumber * sizeof(unsigned int));
            size_t sda_size = varNumber * sizeof(ObjRef); // 8 Byte
            sda = malloc(sda_size); // array - sda_size is known!;
            //stack = malloc(sizeof(StackSlot) * MAXITEMS);
            //stack_cap = (instructionNumber * sizeof(unsigned int))+varNumber;
            //--LESEN & IN PM LADEN--
            fread(program_memory, sizeof(unsigned int), instructionNumber, fp);

            // Starten
            startPr(instructionNumber, program_memory);
        

        //FEHLER BEIM SCHLIESSEN
        if (fclose(fp) != 0) {
            perror("ERROR - fclose");
            exit(0);
        }
        //--SPEICHER FREIGEBEN--
        free(program_memory);
        free(sda);
        //free(stack);
    }
}

printf("Ninja Virtual Machine stopped\n");
return 0;
}
