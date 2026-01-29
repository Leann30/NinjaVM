#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "bigint/src/bigint.h"

#define MAXITEMS 10000

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

#define NEW 32
#define GETF 33
#define PUTF 34
#define NEWA 35
#define GETFA 36
#define PUTFA 37
#define GETSZ 38
#define PUSHN 39
#define REFEQ 40
#define REFNE 41

#define VERSION 8

//Sonderfälle negative Immediate-Werte
#define OPCODE(x) (x >> 24)  //8 Bit nach rechts schieben (obere Bits sind Opcode)
#define IMMEDIATE(x) ((x) & 0x00FFFFFF) //unterste 24 Bit, weil sonst Vorzeichen in Opcode
#define SIGN_EXTEND(i) ((i) & 0x00800000 ? (i) | 0xFF000000 : (i)) 
/*übergebe 24 Bit immediate
prüfe mit bitweiser Verundung ob bit 23 gesetzt ist, dann minus*/


int stackSize = 10000;
int heapSize = 10000;

//Objekt im Heap
typedef struct {
    bool isCmpObject;
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

StackSlot *stack;

#define GET_REFS_PTR(objRef) ((ObjRef *) (objRef)->data)//Zugriff auf Compound 
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

/*-----------------------------------------
                BIG INT
----------------------------------------*/
void fatalError(char *msg){
    printf("fatal error: %s/n", msg);
    exit(1);
}

void * newPrimObject(int dataSize) {
    int objSize = sizeof(Object) + dataSize;
    if(objSize > (heapSize * 1024)){
        fatalError("heap overflow");
    }
    ObjRef newPrimObj = malloc(objSize);

    if (newPrimObj == NULL) {
        fatalError("newPrimObj darf nicht null sein!");
    }
    newPrimObj->size = dataSize;
    return newPrimObj;
}

void * getPrimObjectDataPointer(void * obj){
    ObjRef oo = ((ObjRef) (obj));
    return oo->data;
}

//STACK OPERATIONEN//
void push(int x) {
    bigFromInt(x); 

    if (sp >= stackSize) {
        fatalError("Stack overflow");
    }
    stack[sp].isObjRef = true;
    stack[sp].u.objRef = bip.res;
    sp++;
}

void pushc(int x){
    push(x);
}

void push_number(int x) {
    if (sp >= stackSize){
        fatalError("Stack overflow");
    }
    stack[sp].isObjRef = false;
    stack[sp].u.number = x;
    sp++;
}

int pop(void) {
    sp--; 
    if (sp < 0) { 
        printf("error: stack underflow\n");
        exit(0);
    }
    
    if(stack[sp].isObjRef){
        printf("expected: number, found: ObjRef\n");
        exit(0);
    }
    return stack[sp].u.number;
}

void push_obj(ObjRef objRef){

    if (sp >= stackSize) {
        fatalError("Stack overflow");
    }
    //Auf Stack legen
    stack[sp].isObjRef = true;
    stack[sp].u.objRef = objRef;
    sp++;
}

ObjRef pop_obj(void) {
    sp--;
    if (sp < 0) {
        fatalError("Stack underflow\n");
        exit(0);
    }
    if (!stack[sp].isObjRef) {
        fatalError("Expected ObjRef, but found number\n");
    exit(0);
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
void asf(int n) {
    push_number(fp); 
    fp = sp;
    for (int i = 0; i < n; i++) {
        stack[sp].isObjRef = true;
        stack[sp].u.objRef = NULL;
        sp++;
    }
}
//entfernen des aktuellen stackframes, rückkehr zum vorheringen
void rsf(void){ 
    sp = fp;
    fp = pop();
}

/*-----------------------------
    VERBUNDOBJEKTTYPEN
-------------------------------*/
//ObjRef newPrimitiveObject(int numBytes);
ObjRef newCompoundObject(int numObjRefs){
    int objSize = sizeof(Object) + (numObjRefs * sizeof(ObjRef));
    if(objSize > (heapSize*1024)){
        fatalError("Heap overflow");
    }
    ObjRef cmpObj = malloc(objSize);
    cmpObj->size = numObjRefs;
    cmpObj->isCmpObject = true;
 
    for (int i = 0; i < numObjRefs; i++) {
        GET_REFS_PTR(cmpObj)[i] = NULL;
    }
    return cmpObj;
}

//Records 
void new(int number_elements){
    newCompoundObject(number_elements);
}

ObjRef getf(int n){
    ObjRef objRec = pop_obj(); //pointer zum object 
    if(objRec == NULL){
        fatalError("Objekt ist Null");
    } if(!(objRec->isCmpObject)){
        fatalError("Objekt ist kein Record!");
    }  if(n < 0 || n >= objRec->size){
        fatalError("Index out of bounds!");
    }
    return GET_REFS_PTR(objRec)[n]; //REferenz auf Objekt
}

void putf(int n){
    ObjRef field = pop_obj();
    ObjRef objRec = pop_obj();
    if (objRec == NULL){
        fatalError("Record ist null!");
    } if (!objRec->isCmpObject){
        fatalError("Kein Compound Objekt!");
    } if (n < 0 || n >= objRec->size) {
        fatalError("Index out of bounds!");
    }
    GET_REFS_PTR(objRec)[n] = field; 
}

//Arrays
void newa(void) {
    ObjRef sizeObj = pop_obj(); //Anzahl der Objekte oben auf Stack
    bip.op1 = sizeObj;
    int n = bigToInt();
    ObjRef objArr = newCompoundObject(n);
    push_obj(objArr);
}

void getfa(void){
    ObjRef indexObj = pop_obj();
    ObjRef objArr = pop_obj();
    bip.op1 = indexObj;
    int index = bigToInt();
    if (objArr == NULL){
        fatalError("Record ist null!");
    } if (!objArr->isCmpObject){
        fatalError("Kein Compound Objekt!");
    } if (index < 0 || index >= objArr->size){
        fatalError("Index out of bounds!");
    } 
    push_obj(GET_REFS_PTR(objArr)[index]); 
}

void putfa(int index){
    ObjRef field = pop_obj();
    ObjRef objArr = pop_obj();

    if (objArr == NULL){
        fatalError("Record ist null!");
    } if (!objArr->isCmpObject){
        fatalError("Kein Compound Objekt!");
    } if (index < 0 || index >= objArr->size){
        fatalError("Index out of bounds!");
    } 
    GET_REFS_PTR(objArr)[index] = field;   
}

void getsz(void){
    ObjRef obj = pop_obj();

    if (obj == NULL) {
        fatalError("Object darf nicht null sein!");
    }

    int objSize = obj->size;
    bigFromInt(objSize);
    push_obj(bip.res);
}

void pushn(void){
    push_obj(NULL);
}

void refeq(void){
    ObjRef objRef2 = pop_obj();
    ObjRef objRef1 = pop_obj();
    if (objRef1 == objRef2) {
        bigFromInt(1); //true
    } else {
        bigFromInt(0); //false
    }
    push_obj(bip.res);
}

void refne(void){
    ObjRef objRef2 = pop_obj();
    ObjRef objRef1 = pop_obj();
    if (objRef1 != objRef2) {
        bigFromInt(1);
    } else {
        bigFromInt(0); 
    }
    push_obj(bip.res);
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
   bip.op2 = pop_obj();
   bip.op1 = pop_obj();
   bigAdd();
   push_obj(bip.res);
}

void sub(void){
   bip.op2 = pop_obj();
   bip.op1 = pop_obj();
   bigSub();
   push_obj(bip.res);
}

void mul(void){
   bip.op2 = pop_obj();
   bip.op1 = pop_obj();
   bigMul();
   push_obj(bip.res);
}

void division(void){
   bip.op2 = pop_obj();
   bip.op1 = pop_obj();
   bigDiv();
   push_obj(bip.res);
}

void mod(void){
   bip.op2 = pop_obj();
   bip.op1 = pop_obj();
   bigDiv();
   push_obj(bip.rem);
}

void rdint(void){
    bigRead(stdin);
    push_obj(bip.res);
}

void wrint(void){
    ObjRef obj = pop_obj();
    bip.op1 = obj;
    bigPrint(stdout);
}

void rdchr(void){
    printf("welches Zeichen soll eingelesen werden?\n");
    char c = getchar();
    bigFromInt(c);
    push_obj(bip.res);
}

void wrchr(void){
    ObjRef obj = pop_obj();
    bip.op1 = obj;
    int c = bigToInt();
    printf("%c", c);
    //printf("\n");
}

void eq(void){
    bip.op2 = pop_obj();
    bip.op1 = pop_obj();
    int cmpRes = bigCmp();
    int res = 0;
    if(cmpRes == 0){
        res = 1;
    } 
    bigFromInt(res);
    push_obj(bip.res);
}

void ne(void){
    bip.op2 = pop_obj();
    bip.op1 = pop_obj();
    int cmpRes = bigCmp();
    int res = 0;
    if(cmpRes != 0){
        res = 1;
    } 
    bigFromInt(res);
    push_obj(bip.res);
}

void lt(void){
    bip.op2 = pop_obj();
    bip.op1 = pop_obj();
    int cmpRes = bigCmp();
    int res = 0;
    if(cmpRes < 0){ //dann ist op1 kleiner
        res = 1;
    } 
    bigFromInt(res);
    push_obj(bip.res);
}

void le(void){
    bip.op2 = pop_obj();
    bip.op1 = pop_obj();
    int cmpRes = bigCmp();
    int res = 0;
    if(cmpRes <= 0){ 
        res = 1;
    } 
    bigFromInt(res);
    push_obj(bip.res);
}

void gt(void){
    bip.op2 = pop_obj();
    bip.op1 = pop_obj();
    int cmpRes = bigCmp();
    int res = 0;
    if(cmpRes > 0){ //dann ist op1 größer
        res = 1;
    } 
    bigFromInt(res);
    push_obj(bip.res);
}

void ge(void){
    bip.op2 = pop_obj();
    bip.op1 = pop_obj();
    int cmpRes = bigCmp();
    int res = 0;
    if(cmpRes >= 0){
        res = 1;
    } 
    bigFromInt(res);
    push_obj(bip.res);
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
    ObjRef obj = pop_obj();
    bip.op1 = obj;
    int value = bigToInt();
    if(value == 0){
        jmp(target);
    }
}

//springe wenn value true
void brt(int target){
    ObjRef obj = pop_obj();
    bip.op1 = obj;
    int value = bigToInt();
    if(value != 0){
        jmp(target);
    }
}

//speicher Rücksprungadresse auf stack
void call(int n){
  push_number(pc);
  jmp(n);  
}
//kehre zur Rücksprungadresse zurück
void ret(void){
  int ra = pop();
  pc = ra;
}
//lösche n einträge vom stack
void drop(int n){
    sp -= n;
}

void pushr(void){
    push_obj(rv);
}

void popr(void){
    rv = pop_obj();
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
            break;
        case 32: 
            new(immediate);
            break;
        case 33:
            getf(immediate);
            break;
        case 34:
            putf(immediate);
            break;
        case 35:
            newa();
            break;
        case 36:
            getfa();
            break;
        case 37:
            putfa(immediate);
            break;
        case 38:
            getsz();
            break;
        case 39:
            pushn();
            break;
        case 40:
            refeq();
            break;
        case 41:
            refne();
    }
}

void startPr(int length, int* program_memory){
    pc = 0;
    int opcode;
    //--vor Start Programm printen--
    //print_program(length, program_memory);

    //--ausführen--
    do {
        unsigned int instruction = program_memory[pc];
        opcode = OPCODE(instruction);
        pc++;   
        execute(instruction);
        
    } while(opcode != 0);
}

int main(int argc, char *argv[]) {
   
    FILE *fp = NULL;
    if (argc < 2) {
        return 1;
    }

    printf("Ninja Virtual Machine started\n");
    if(argv[1] == NULL){
        printf("unknown command line argument %s, try 'njvm --help'\n", argv[1]);
        exit(0);
    } else {
        for (int i = 1; i < argc; i++) {
    
            if (strcmp(argv[i], "--version") == 0) {
                printf("Ninja Virtual Machine version 4.0\n");
            } 
            /*else if (strcmp(argv[1], "--debug") == 0) {
                FILE * fp = fopen(argv[2], "rb");
                if (fp == NULL) {
                    perror("ERROR - fopen");
                    exit(1);
                } else {
                    printf("DEBUG: file %d loaded (code size = %d, data size = %d)", fp);
                } 
            } */
            else if (strcmp(argv[i], "--help") == 0) {
                printf("--version   show version and exit\n");
                printf("--help      show this help and exit\n");
            } else if(strcmp(argv[i], "--stack") == 0) {
                if(i+1 < 0){
                    exit(1);
                }
                if(i+1 < argc){
                    stackSize = atoi(argv[++i]);
                }
            } else if(strcmp(argv[i], "--heap") == 0) {
                if((i+1) < 0 /*|| (i+1) > 34000*/){
                    exit(1);
                }
                if(i+1 < argc){
                    heapSize = atoi(argv[++i]);
                }
            } else {
                //--FEHLER BEIM ÖFFNEN ABFANGEN--
                fp = fopen(argv[i], "rb");
                if (fp == NULL) {
                    perror("ERROR - fopen");
                    exit(0);
                } else {
                    fseek(fp, 0, SEEK_SET);
                    char c[4];
                    fread(c, 1, 4, fp);
                    if(c[0] != 'N' && c[1] != 'J' && c[2] != 'B' && c[3] != 'F'){
                        fatalError("Kein Ninja Binary File.\n");
                        fclose(fp);
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
                    size_t sda_size = varNumber * sizeof(ObjRef); 
                    sda = malloc(sda_size); 
                    stack = malloc(sizeof(StackSlot) * stackSize);

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
                    free(stack);
                }
            }
        }
    }
    printf("Ninja Virtual Machine stopped\n");
    return 0;
}

