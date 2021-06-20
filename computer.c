#include <stdbool.h> 
#include <stdio.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BYTE_VALUE 255
#define MEMORY_SIZE 255

typedef __uint8_t byte;

enum operation {load,store,data,jumpr,jump,jumpc,clf};
enum opcode {add,shl,shr,not,and,or,xor,cmp};
enum flags {c,a,e,z};

struct Memory  {
    byte mar;
    byte ram[MEMORY_SIZE];
} mem;

struct ALU {
  bool flags[4]; 
} alu;

struct CPU {
  byte regs[4];
  byte iar;
  byte ir;
  byte tmp;
  byte acc;
} cpu;

byte bus;


int currentStep = 1; 

void call_alu(byte opcode, byte bus1){
  byte op1 = bus;
  byte op2 = bus1 ? bus1 : cpu.tmp;
  byte cf = alu.flags[c] ? 1 : 0;

  if (opcode == add) {
      cpu.acc = op1 + op2 + cf;
      alu.flags[c] = ((op1 + op2 + cf) > MAX_BYTE_VALUE) ? true : false; 
  }else if (opcode == shl){
      cpu.acc = (op1 << 1) | cf;
      alu.flags[c] = (op1 & 128) ? true : false; 
  }else if (opcode == shr){
      cpu.acc = (op1 >> 1) | (cf ? 128 : 0); 
      alu.flags[c] = (op1 & 1) ? true : false; 
  }else if (opcode == not){
      cpu.acc = ~op1;
  }else if (opcode == and){
      cpu.acc = op1 & op2;
  }else if (opcode == or){
      cpu.acc = op1 | op2;
  }else if (opcode == xor){
      cpu.acc = op1 ^ op2;
  }else if (opcode == cmp){
      alu.flags[a] = (op1 > op2);
      alu.flags[e] = (op1 == op2);
  }
  alu.flags[z] = (cpu.acc == 0);
}

void step1(){
  bus = cpu.iar;
  call_alu(0,1);
  mem.mar = bus;
}

void step2(){
  bus = mem.ram[bus]; 
  cpu.ir = bus; 
}

void step3(){
  bus = cpu.acc;
  cpu.iar = bus;
}

void step4(){
  byte op = cpu.ir >> 4; 
  byte opr = (cpu.ir & 15);
  byte opr1 = (opr & 12) >> 2;
  byte opr2 = (opr & 3);

  if (op == load || op == store || op == jumpr) {               
    bus = cpu.regs[opr1]; 
    mem.mar = bus; 
  }else if (op == data || jumpc){        
    bus = cpu.iar; 
    call_alu(0,1); 
    mem.mar = bus; 
  }else if (op == jump){      
    bus = cpu.iar; 
    mem.mar = bus; 
  }else if (op == clf){ 
    for (int i = 0; i < 4; i++){ alu.flags[i] = false; }
  }else if (op > 7){
    bus = cpu.regs[opr2];
    cpu.tmp = bus;
  }
}

void step5(){
  byte op = cpu.ir >> 4; 
  byte opr = (cpu.ir & 15);
  byte opr1 = (opr & 12) >> 2;
  byte opr2 = (opr & 3);

  if (op == load) {         
    bus = mem.ram[mem.mar]; 
    cpu.regs[opr2] = bus; 
  }else if (op == store){  
    bus = cpu.regs[opr2]; 
    mem.ram[mem.mar] = bus; 
  }else if (op == data){  
    bus = mem.ram[mem.mar]; 
    cpu.regs[opr1] = bus; 
  }else if (op == jumpr){
    bus = cpu.regs[opr1]; 
    cpu.iar = bus;     
  }else if (op == jump){
    bus = mem.ram[mem.mar];
    cpu.iar = bus;
  }else if (op == jumpc){ 
    bus = cpu.acc; 
    cpu.iar = bus; 
  }else if (op > 7){
    bus = cpu.regs[opr1];
    byte opcode = op & 7; 
    call_alu(opcode,0);
    if (opcode != 7){
      //execute move of the acc if not a cmp (not very clean)
      bus = cpu.acc;
      cpu.regs[opr2] = bus; //set: put result into operand2
    }
  }
}

void step6(){
  byte op = cpu.ir >> 4; 
  byte opr = (cpu.ir & 15);
  byte opr1 = (opr & 12) >> 2;
  byte opr2 = (opr & 3);

  if (op == data){  
    bus = cpu.acc; 
    cpu.iar = bus; 
  }else if (op == jumpc){ 
    byte res = 0;
    res |=   alu.flags[z] | 
            (alu.flags[e] ? 2 : 0) | 
            (alu.flags[a] ? 4 : 0) | 
            (alu.flags[c] ? 8 : 0);
    
    if (res & opr){ 
      bus = mem.ram[mem.mar];
      cpu.iar = bus;
    }
  }else if (op > 7){
    byte opcode = op & 7; 
    if (opcode != 7){
      //execute move of the acc if not a cmp (not very clean)
      bus = cpu.acc;
      cpu.regs[opr2] = bus; //set: put result into operand2
    }

  }
}

void cycle(){
    if (currentStep == 1) { step1(); }
    else if (currentStep == 2){ step2(); }
    else if (currentStep == 3){ step3(); }
    else if (currentStep == 4){ step4(); }
    else if (currentStep == 5){ step5(); }
    else if (currentStep == 6){ step6(); }
    currentStep = (currentStep == 6) ? 1 : currentStep + 1;
}

void start(){
  for (int i = 0; i < 100; i++){ 
    cycle(); 
  }
}

void load_file(char * filename){
  FILE *fileptr;
  long filelen;

  fileptr = fopen(filename, "rb");  
  fseek(fileptr, 0, SEEK_END);       
  filelen = ftell(fileptr);         
  rewind(fileptr);                 

  fread(mem.ram, filelen, 1, fileptr);
  fclose(fileptr); 
}

void print_ram(){
  char src[4];  //null terminator
  sprintf(src,"%03d", 7);
  printf("%s",src); 
}


void memWindow(int x,int y,int width,int height){
  WINDOW *winmem = newwin(height,width,y,x);
  box(winmem,0,0);
  int cellSize = 4;
  int cellsPerLine = width/cellSize;
  for (int i = 0; i < MEMORY_SIZE; i++){
    char memCell[4];
    sprintf(memCell,"%03d", mem.ram[i]);
    mvwprintw(winmem,(i/cellsPerLine)+1,(i%cellsPerLine)*cellSize+1,"%s",memCell);
  }
  wrefresh(winmem);
}

void cpuWindow(int x,int y,int width,int height){
  WINDOW *wincpu= newwin(height,width,y,x);
  box(wincpu,0,0);
  char r0[8],r1[8],r2[8],r3[8],ir[8],iar[9],tmp[9],acc[9];
  sprintf(r0,"R0: %03d",cpu.regs[0]);
  mvwprintw(wincpu,1,1,"%s",r0);
  sprintf(r1,"R1: %03d",cpu.regs[1]);
  mvwprintw(wincpu,2,1,"%s",r1);
  sprintf(r2,"R2: %03d",cpu.regs[2]);
  mvwprintw(wincpu,3,1,"%s",r2);
  sprintf(r3,"R3: %03d",cpu.regs[3]);
  mvwprintw(wincpu,4,1,"%s",r3);
  sprintf(ir,"IR: %03d",cpu.ir);
  mvwprintw(wincpu,6,1,"%s",ir);
  sprintf(iar,"IAR: %03d",cpu.iar);
  mvwprintw(wincpu,7,1,"%s",iar);
  sprintf(tmp,"TMP: %03d",cpu.tmp);
  mvwprintw(wincpu,8,1,"%s",tmp);
  sprintf(acc,"ACC: %03d",cpu.acc);
  mvwprintw(wincpu,9,1,"%s",acc);
  wrefresh(wincpu);
}

void busWindow(int x,int y,int width,int height){
  WINDOW *winbus= newwin(height,width,y,x);
  box(winbus,0,0);
  char buss[10];
  sprintf(buss,"BUS: %03d",bus);
  mvwprintw(winbus,1,1,"%s",buss);
  wrefresh(winbus);
}

void aluWindow(int x,int y,int width,int height){
  WINDOW *winalu= newwin(height,width,y,x);
  box(winalu,0,0);
  char fc[7],fa[7],fe[7],fz[7];
  sprintf(fc,"C: %03d",alu.flags[0]);
  mvwprintw(winalu,1,1,"%s",fc);
  sprintf(fa,"A: %03d",alu.flags[1]);
  mvwprintw(winalu,2,1,"%s",fa);
  sprintf(fe,"E: %03d",alu.flags[2]);
  mvwprintw(winalu,3,1,"%s",fe);
  sprintf(fz,"Z: %03d",alu.flags[3]);
  mvwprintw(winalu,4,1,"%s",fz);
  wrefresh(winalu);
}

void display(){
  initscr();
  clear();
  refresh();
  memWindow(20,20,62,20);
  busWindow(84,20,20,3);
  cpuWindow(84,23,20,17);
  aluWindow(106,20,20,20);
  int input = getch();
  endwin();
}

int main(int argc, char *argv[]){
  if (argv[1] == NULL){
    printf("please enter a binary file\n");
    exit(1);
  }
  load_file(argv[1]); 
  display();
}
