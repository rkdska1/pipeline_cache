#include <stdio.h>
#include <stdlib.h>
#include <WinSock2.h>

#pragma comment(lib, "ws2_32.lib")

#define MAX_LENGTH 32

#define R_type 0x0
//R type
#define add 0x20
#define addu 0x21
#define and 0x24
#define jr 0x08
#define nor 0x27
#define or 0x25
#define slt 0x2A
#define sltu 0x2B
#define sll 0x00
#define srl 0x02
#define sub 0x22
#define subu 0x23
#define div 0x1A
#define divu 0x1B
#define mfhi 0x10
#define mflo 0x12
//#define mfc0 
#define mult 0x18
#define multu 0x19
#define sra 0x03

//I type
#define addi 0x08
#define addiu 0x09
#define andi 0x0C
#define beq 0x04
#define bne 0x05
#define lbu 0x24
#define lhu 0x25
#define ll 0x30
#define lui 0x0F
#define lw 0x23
#define ori 0x0D
#define slti 0x0A
#define sltiu 0x0B
#define sb 0x28
#define sc 0x38
#define sh 0x29
#define sw 0x2B
#define lwc1 0x31
#define ldc1 0x33
#define swc1 0x39
#define sdc1 0x3D
//J type
#define J 0x02
#define jal 0x03
#define inuse -25000
#define isfree 25001

int fullview=2;

typedef struct {
	unsigned pc;
} pc_state_s, *pc_state;

typedef struct {
	unsigned pc;
	unsigned instr;
	unsigned NOP;
} if_id_s, *if_id;

typedef struct {
	unsigned pc;
	unsigned instr;
	int rs_v;
	int rt_v;
	unsigned unc_jump;
	unsigned jump_branch;
	unsigned NOP;
} id_ex_s, *id_ex;

typedef struct {
	unsigned pc;
	unsigned branch_target;
	unsigned jump_branch;
	int ALU_out;
	int rt_v;
	unsigned wb_dest;
	unsigned M_write;
	unsigned M_read;
	unsigned byte_count;
	unsigned NOP;
	int rd_back;
	int rt_back;
	unsigned b;
} ex_mem_s, *ex_mem;

typedef struct {
	unsigned pc;
	unsigned M_read;
	unsigned wb_dest;
	int ALU_out;
	unsigned read_data;
	unsigned NOP;
} mem_wb_s, *mem_wb;

typedef struct {
	unsigned tag;
	unsigned validity;
	unsigned dirty;
	unsigned sec;
	unsigned linesite[16];
}	cache_str;

int buffer[0xFFFFFF]= {0, };
int registers[MAX_LENGTH] = {0, };
int reg_inuse[MAX_LENGTH] = {0, };
char *Registers[MAX_LENGTH] =
{"zero", "at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7", "s0","s1","s2", "s3", "s4", "s5", "s6", "s7", "t8", "t9", "k0", "k1", "gp", "sp", "s8", "ra"};
int oldest[16]= {0, };


pc_state pc_now;
if_id id_in;
if_id if_in;
if_id if_out;
id_ex ex_in;
id_ex id_out;
ex_mem mem_in;
ex_mem ex_out;
mem_wb wb_in;
mem_wb mem_out;
cache_str cache_a[4][16];




unsigned find_op(unsigned code) { return ((code)>>26&0x3f);}
unsigned find_rs(unsigned code) {return ((code)>>21&0x1f);}
unsigned find_rt(unsigned code) {return ((code)>>16&0x1f);}
unsigned find_rd(unsigned code) {return ((code)>>11&0x1f);}
unsigned find_sa(unsigned code) {return ((code)>>6&0x1f);}
unsigned find_imm(unsigned code) {return ((code)&0xffff);}			//if type is short must be checked--- if typecasted to (unsigned short), turns to not signed extended
int find_s_imm(unsigned imm) {return ((short) imm);}
unsigned find_zeroimm(unsigned imm) {return ((imm)&0xffff) ;}
unsigned find_func(unsigned code) {return (code & 0x3f);}

void IF(int cycle);
void ID();
void EXE();
void MEM();
void WB();
void initialize();
unsigned run_pipe();
unsigned MemRead(unsigned address);
unsigned MemWrite(unsigned address, int writingData);
void cachefill(int way, int index, int address);
int replace(int index, int address);


int main(void){
	FILE *pfile; char input[256];	 int testing=1; //In case I need to return register value//
	unsigned int result=0; int cycle=0;
	int i;
	for(i=0; i <MAX_LENGTH; i++){
		reg_inuse[i]= isfree;		
		registers[i]=0;
	} i=1;
	registers[29] = 0x100000;
	registers[MAX_LENGTH-1] = 0xffffffff;
	while(i==1){
		printf("Enter the name of the file\n");
		scanf("%s", &input);
		pfile = fopen(input, "rb");
		i=0;
		if (pfile == NULL){
			printf("ERROR! Cannot open the file!! Re-enter\n");
			i=1;
		}
	}
	if(buffer ==NULL) { printf("Memory Error"); exit(2); }
	fread(buffer, 4, 0xFFFFFF, pfile);//sizeof(long*
	for(i=0; i<0xFFFFFF; i++){
	buffer[i] = ntohl(buffer[i]);
	}
	if( ferror (pfile)) { printf("READING ERROR"); exit(3);}
	printf("\n00000000 <main>:");
	initialize();
	while(!(fullview==1 || fullview==0)){
		printf("View in full detail? yes = 1  or  no = 0\n");
		scanf("%d", &fullview);
		if(fullview==0)
			printf("Showing only changed state\n\n");
		else if(fullview==1)
			printf("Showing all stages and registers\n\n");
		else
			printf("Re-enter the value!!!\n");
	}
	cycle = run_pipe();
	printf("\nTOTAL CYCLE IS : %d", cycle);
	printf("\n\nRegisters[v0] is %d\n\n", registers[2]);
	fclose(pfile);
	system("pause");
	if(testing)
		return 0;
	else
		return registers[2];
}

void IF(int cycle)	{
	unsigned pc = pc_now->pc; 
	if(fullview)	printf("\n[o] INSTRUCTION IS FETCHED\n");
	if(if_in->NOP){
		;
		if(fullview)		printf("\nPC::::: %d DOES NOT CHANGE", pc);
	}
	else if(ex_out->jump_branch){
		pc = ex_out->branch_target;
		ex_out->jump_branch=0;
		id_in->NOP=0;
		if(fullview)		printf("\nJUMP in EXE detected::::: JUMPING");
//		printf("\nPC::::: %x    ->     %x\n", pc_now->pc, pc);
		pc_now->pc= pc+4;
	}
	else if(id_out->jump_branch){
		pc = id_out->unc_jump;
		id_out->jump_branch = 0;
		id_in->NOP=0;
		if(fullview)		printf("\nJUMP in ID detected::::: JUMPING");
//		printf("\nPC::::: %x    ->     %x\n", pc_now->pc, pc);
		pc_now->pc= pc+4;
	}
	else{
		pc_now->pc= pc+4;
//	printf("\nPC::::: %x    ->     %x\n", pc, pc_now->pc);
	}
//	if(fullview)	printf("instruction is : %08x", buffer[pc/4]);
	if(pc==0xffffffff)
		if_out->NOP=1;
	if_out->instr = MemRead(pc);
										//Do we need to use mem_read here? I think not...we have to access to memory anyway, but why go through unnecessary steps? ju
	
	if(fullview)	printf("\ninstruction is : %08x", if_out->instr);
	if_out->pc = pc;
	if_out->NOP = if_in->NOP;
if(fullview)	printf("\n\n---------------------------------\n");
}		//checked

void ID()	{	
	unsigned write_data=0;
	unsigned instr= if_out->instr;
	unsigned op = find_op(instr);
	int a=0;
	unsigned char flag=0;
	id_in->pc = if_out->pc;
	id_in->NOP = if_out->NOP;
	a= find_rs(instr);
	id_out->rs_v = registers[find_rs(instr)];
	id_out->rt_v = registers[find_rt(instr)];
	a= find_rs(instr);
	if(instr==0x0)
		id_in->NOP=1;
if(fullview){
	if(id_in->NOP)
		printf("\n         ID IS NOT EXCUTED");
	else
		printf("\n[o] ID IS EXCUTING");
}
	if(reg_inuse[find_rs(instr)]!=isfree && reg_inuse[find_rs(instr)]!= inuse){		
		a= reg_inuse[find_rs(instr)];
		id_out->rs_v= reg_inuse[find_rs(instr)];			
		id_in->NOP=0;	if_out->NOP=0;  if_in->NOP=0;
		flag=1;
	}
	if(reg_inuse[find_rt(instr)]!=isfree && reg_inuse[find_rt(instr)]!= inuse){
		id_out->rt_v=reg_inuse[find_rt(instr)];	
		id_in->NOP=0;	if_out->NOP=0; if_in->NOP=0;
		flag=1;
	}
	if(op==R_type){
		if(find_func(instr) == jr){
			if(reg_inuse[find_rs(instr)]==inuse){
				if_out->NOP=1; id_in->NOP=1;  if_in->NOP = 0;
				id_out->jump_branch = 1;
if(fullview)				printf("\nJR is detected in ID stage::REG is in use; halt");
			}
			else{
				id_out->unc_jump= id_out->rs_v;
				id_out->jump_branch= 1;
				id_in->NOP = 1; if_in->NOP = 0;
if(fullview)				printf("\nJR is detected in ID stage::REG NOT IN use; JUMP in next stage");
			}
		}
		else if(find_func(instr) == sll || find_func(instr) == srl){
			if(reg_inuse[find_rt(instr)]==inuse){
				if_out->NOP=1; id_in->NOP=1; if_in->NOP=1;
if(fullview)				printf("\nSLL /// SRL is detected in ID stage::REG is in use; halt");
			}
		}
		else{
			if(reg_inuse[find_rs(instr)]==inuse || reg_inuse[find_rt(instr)]==inuse){
				if_out->NOP=1; id_in->NOP=1; if_in->NOP = 1;
			}
		}
	}
	else if(op == J){
		id_out->unc_jump = ((((id_in->pc)+4)<26)|((instr) & 0x03ffffff))<<2;            
		id_out->jump_branch=1;
		id_in->NOP = 1; if_in->NOP = 0;
	}
	else if (op == jal){
		id_out->unc_jump = ((((id_in->pc)+4)<26)|((instr) & 0x03ffffff))<<2; 
		id_out->jump_branch =1;
		if_in->NOP = 0;
		reg_inuse[find_rd(instr)]=inuse;
	}
	else if(op == beq || op == bne){
		if((reg_inuse[find_rs(instr)]==inuse) || (reg_inuse[find_rt(instr)]==inuse)){
			if_in->NOP = 1;
		}}
	else{
		if((reg_inuse[find_rs(instr)]==inuse) && flag==0){
			if_out->NOP=1; id_in->NOP=1; if_in->NOP = 1;
		}
	}
			// =--=====-=-==========-===================-=
	if(!(id_in->NOP)){
	if(op == R_type && op != jr){
		reg_inuse[find_rd(instr)]=inuse;
if(fullview)		printf("\nREG %s IN USE!!!", Registers[find_rd(instr)]);
	}
	else if((op != bne) && (op != beq) && (op != sb) && (op != sc) && (op != sh) && (op != sw) && (op != J) && (op != jal)){
		reg_inuse[find_rt(instr)]=inuse;
if(fullview)				printf("\nREG %s IN USE!!!", Registers[find_rt(instr)]);
	}
	}	

	id_out->pc = id_in->pc;
	id_out->instr = instr;
	id_out->NOP= id_in->NOP;
}


void EXE() {
	unsigned jump_branch = 0;
	unsigned instr = id_out->instr;
	unsigned func = find_func(instr);
	unsigned opcode = find_op(instr);
	unsigned M_read = 0;
	unsigned M_write = 0;
	unsigned branch_if_zero = 0;
	unsigned branch_if_nonzero = 0;
	unsigned sa = find_sa(instr);
	int rt = id_out->rt_v;
	int rs = id_out->rs_v;
	unsigned rd = find_rd(instr);
	unsigned wb_dest=0;
	unsigned imm = find_imm(instr);
	int s_imm = find_s_imm(imm);
	int zero_imm = find_zeroimm(imm);
	ex_in->instr=id_out->instr;
	ex_out->b=0;
	ex_in->pc= id_out->pc;
	ex_out->ALU_out = 0;
	ex_out->byte_count=0xffffffff;
	ex_in->NOP = id_out->NOP;
if(fullview){
	if(ex_in->NOP)
		printf("\n         EXE IS NOT EXCUTED");
	else
		printf("\n[o] EXE IS EXCUTING\n   ");
}
	if(!(ex_in->NOP)){
	switch (opcode) {
	case(R_type):
		switch(func) {
		case sll:
			ex_out->ALU_out = rt << sa;
			wb_dest = rd;
if(fullview)			printf("sll to R[%s] being executed", Registers[rd]);
			break;
		case srl:
			ex_out->ALU_out = rt >> sa;
			wb_dest = rd;
if(fullview)			printf("srl R[%s] being executed", Registers[rd]);
			break;
		case jr:
			break;
		case add:
			ex_out->ALU_out =(unsigned)(rs + rt);
			wb_dest = rd;
if(fullview)			printf("add R[%s]=R[%s] + R[%s] being executed", Registers[rd], Registers[find_rs(instr)], Registers[find_rt(instr)]);
		case addu:
			ex_out->ALU_out = rs + rt;
			wb_dest = rd;
if(fullview)			printf("add R[%s]=R[%s] + R[%s] being executed", Registers[rd], Registers[find_rs(instr)], Registers[find_rt(instr)]);
			break;
		case and:
			ex_out->ALU_out = rs & rt;
			wb_dest = rd;
if(fullview)			printf("and R[%s]=R[%s] & R[%s] being executed", Registers[rd], Registers[find_rs(instr)], Registers[find_rt(instr)]);
			break;
		case nor:
			ex_out->ALU_out = ~(rs | rt);
			wb_dest = rd;
if(fullview)			printf("nor R[%s]=R[%s] R[%s] being executed", Registers[rd], Registers[find_rs(instr)], Registers[find_rt(instr)]);
			break;
		case or:
			ex_out->ALU_out = rs | rt ;
			wb_dest = rd;
if(fullview)			printf("o R[%s]=R[%s] | R[%s]r being executed", Registers[rd], Registers[find_rs(instr)], Registers[find_rt(instr)]);
			break;
		case slt:
		case sltu:
			ex_out->ALU_out = (rs < rt) ? 1:0;
			wb_dest = rd;
if(fullview)			printf("sltu being executed", Registers[rd], Registers[find_rs(instr)], Registers[find_rt(instr)]);
			break;		
		case sub:
		case subu:
			ex_out->ALU_out = rs - rt ;
			wb_dest = rd;
if(fullview)			printf("subu R[%s]=R[%s]-R[%s] being executed", Registers[rd], Registers[find_rs(instr)], Registers[find_rt(instr)]);
			break;
		default:
				printf("WRONG FUNCTION TYPE!!!!");
			instr;
			break;
		}

		reg_inuse[rd]=ex_out->ALU_out;
if(fullview)		printf("\n		Reg %s is now freed", Registers[rd]);
		break;
	//I- TYPE
	case J:
		break;
	case jal:
		wb_dest=31;
		ex_out->ALU_out = ex_in->pc +8;
		reg_inuse[31]=ex_out->ALU_out;
		break;
	case beq:
		if(rs == rt){
			ex_out->branch_target = (ex_in->pc) + 4 + ((short)(s_imm) << 2);
			ex_out->jump_branch=1;
			ex_in->NOP = 1;	if_in->NOP=0; 
			id_out->NOP = 1; id_in->NOP=1;
if(fullview)			printf("\n		BEQ R[%s]==R[%s] IS BEING EXECUTED:::: JUMP READY", Registers[find_rs(instr)], Registers[find_rt(instr)]);
		}
		else{
if(fullview)			printf("\n      BEQ FAILED-> NO JUMP");
			ex_in->NOP=1; if_in->NOP=0; id_out->NOP=1;
		}
		break;
	case bne:
		if(rs != rt){
			ex_out->branch_target = (ex_in->pc) + 4 + ((short)(s_imm) << 2);
			ex_out->jump_branch=1;
			ex_in->NOP = 1; if_in->NOP=0;
			id_out->NOP = 1; id_in->NOP=1;
if(fullview)			printf("\n		BNE IS BEING EXECUTED:::: JUMP READY Reg[%d] != Reg[%d]", find_rs(instr), find_rt(instr));
		}
		else{
if(fullview)			printf("\n      BNE FAILED-> NO JUMP");
			ex_in->NOP=1; if_in->NOP=0; id_out->NOP=1;
		}
		break;
	case addi:
		ex_out->ALU_out = rs +s_imm;
		wb_dest = find_rt(instr);
		reg_inuse[find_rt(instr)]=ex_out->ALU_out;
if(fullview){
	printf("addiu R[%s]=R[%s] being executed", Registers[wb_dest], Registers[find_rs(instr)]);
	printf("\n		Reg %s is now freed", Registers[find_rt(instr)]);
}
	break;
	case addiu:
		ex_out->ALU_out = (unsigned)(rs +s_imm);
		wb_dest = find_rt(instr);
		reg_inuse[find_rt(instr)]=ex_out->ALU_out;
if(fullview){
		printf("addiu R[%s]=R[%s] being executed", Registers[wb_dest], Registers[find_rs(instr)]);
		printf("\n		Reg %s is now freed", Registers[find_rt(instr)]);
}
		break;
	case slti:
		ex_out->ALU_out = (rs < s_imm)? 1:0;
		wb_dest = find_rt(instr);
		reg_inuse[find_rt(instr)]=ex_out->ALU_out;
if(fullview){
		printf("sltiu R[%s] < s_imm being executed", Registers[find_rs(instr)]);
		printf("\n		Reg %s is now freed", Registers[find_rt(instr)]);
}
		break;
	case sltiu:
		ex_out->ALU_out = ((unsigned)rs < (unsigned)s_imm)? 1:0;
		wb_dest = find_rt(instr);
		reg_inuse[find_rt(instr)]=ex_out->ALU_out;
if(fullview){
		printf("sltiu R[%s] < s_imm being executed", Registers[find_rs(instr)]);
		printf("\n		Reg %s is now freed", Registers[find_rt(instr)]);
}
		break;
	case andi:
		ex_out->ALU_out = rs & zero_imm;
		wb_dest = find_rt(instr);
		reg_inuse[find_rt(instr)]=ex_out->ALU_out;
if(fullview){
		printf("andi R[%s] & zero_imm being executed", Registers[find_rs(instr)]);
		printf("\n		Reg %s is now freed", Registers[find_rt(instr)]);
}
		break;
	case lui:
		ex_out->ALU_out = imm<<16;
		wb_dest = find_rt(instr);
		reg_inuse[find_rt(instr)]=ex_out->ALU_out;
if(fullview){
		printf("lui being executed");
		printf("\n		Reg %s is now freed", Registers[find_rt(instr)]);
}
		break;
	case ori:
		ex_out->ALU_out = rs | zero_imm;
		wb_dest = find_rt(instr);
		reg_inuse[find_rt(instr)]=ex_out->ALU_out;
if(fullview){
		printf("sltiu R[%s] | zero_imm being executed", Registers[find_rs(instr)]);
		printf("\n		Reg %s is now freed", Registers[find_rt(instr)]);
}
		break;
	case lw:
		ex_out->ALU_out = rs + s_imm;
		M_read = 1;
		wb_dest = find_rt(instr);
		reg_inuse[wb_dest]=inuse;
if(fullview){
		printf("lw being executed");
		printf("\n		Reg %s is now freed", Registers[find_rt(instr)]);
}
		break;
	case lbu:
		ex_out->ALU_out = rs +s_imm; 
		ex_out->byte_count = 0xff;
		wb_dest = find_rt(instr);
		M_read =1;
		reg_inuse[wb_dest]=inuse;
		ex_out->b=1;
if(fullview){
		printf("lbu R[%s] + s_imm being executed", Registers[find_rs(instr)]);
		printf("\n		Reg %s is now freed", Registers[find_rt(instr)]);
}
		break;
	case lhu:
		ex_out->ALU_out = rs + s_imm;
		ex_out->byte_count = 0xffff;
		wb_dest = find_rt(instr);
		M_read = 1;
		reg_inuse[wb_dest]=inuse;
		ex_out->b=1;
if(fullview){
		printf("lhu R[%s] + s_imm being executed", Registers[find_rs(instr)]);
		printf("\n		Reg %s is now freed", Registers[find_rt(instr)]);
}
		break;
	case sb:
		ex_out->ALU_out =rs+s_imm; 
		M_write = 1;
		ex_out->rt_v = rt & 0xff;	
if(fullview)		printf("sb R[%s] + s_imm being executed", Registers[find_rs(instr)]);
		break;												//no idea
	case sh:
		ex_out->ALU_out = rs+s_imm;
		M_write = 1;
		ex_out->rt_v = rt & 0xffff;
if(fullview)		printf("sh R[%s] < s_imm being executed", Registers[find_rs(instr)]);
		break;
	case sw:
		ex_out->ALU_out = rs + s_imm;
		M_write = 1;
		ex_out->rt_v = rt;
if(fullview)		printf("sw being executed");
		break;
	case ll:
		ex_out->ALU_out = rs+ s_imm;
		M_read = 1;
		wb_dest= find_rt(instr);
		reg_inuse[wb_dest]=inuse;
if(fullview){
		printf("ll being executed");
		printf("\n		Reg %s is now freed", Registers[find_rt(instr)]);
}
		break;
	case sc:									
		ex_out->ALU_out = rs+ s_imm;
		M_write = 1;
		ex_out->rt_v = rt;
		rt= (1)? 1:0;
if(fullview)		printf("sc being executed");
		break;
	default:
		printf("WRONG DATA!!!!! EXITING");
		break;
	}
	}
	ex_out->M_write = M_write;
	ex_out->pc= ex_in->pc;
	ex_out->M_read = M_read;
	ex_out->wb_dest = wb_dest;
	ex_out->NOP = ex_in->NOP;
} 



void MEM()
{
	int address=0; int sdata=0;	int data = 0;
	mem_in->b = ex_out->b;
	mem_in->pc= ex_out->pc;
	mem_in->NOP = ex_out->NOP;
	mem_in->rt_v= ex_out->rt_v;
	mem_in->ALU_out = ex_out->ALU_out;
	mem_in->M_read = ex_out->M_read;
	mem_in->M_write = ex_out->M_write;
	mem_in->wb_dest= ex_out->wb_dest;
	sdata = mem_in->rt_v;
	address = mem_in->ALU_out;
	if(fullview){
		if(mem_in->NOP)
			printf("\n         MEM IS NOT EXCUTED");
		else
			printf("\n[o] MEM IS EXCUTING");
	}
	if (mem_in->M_read){
												///////////////
		data = MemRead(address);

		if(ex_out->b==1)
			data= (ex_out->byte_count) & (mem_in->ALU_out);
	if(fullview)		printf("\n		Reading value of %x from MEM[%x] to %d", buffer[address/4], address, data);
		reg_inuse[mem_in->wb_dest]=data;
		mem_in->NOP=0;
	}
	if (mem_in->M_write){
		MemWrite(address, sdata);
	if(fullview)		printf("\nWriting value of %x to MEM[%x]",sdata, address/4);
		mem_in->NOP= 1;
	}
	mem_out->M_read = mem_in->M_read;
	mem_out->wb_dest = mem_in->wb_dest;
	mem_out->ALU_out = mem_in->ALU_out;
	mem_out->read_data = data;
	mem_out->NOP = mem_in->NOP;
	mem_out->pc = mem_in->pc;
}

void WB()
{
	wb_in->NOP = mem_out->NOP;
	wb_in->read_data = mem_out->read_data;
	wb_in->M_read = mem_out->M_read;
	wb_in->wb_dest= mem_out->wb_dest;
	wb_in->ALU_out = mem_out->ALU_out;
	if(!(wb_in->NOP)){
if(fullview)		printf("\n[o] WB IS EXCUTING");
		if(wb_in->M_read){
			registers[wb_in->wb_dest] = wb_in->read_data;
if(fullview)			printf("\nREG[%s] = %d", Registers[wb_in->wb_dest], wb_in->read_data);
		}
		else{
			registers[wb_in->wb_dest] = wb_in->ALU_out;
if(fullview)			printf("\nREG[%s] = %d", Registers[wb_in->wb_dest], wb_in->ALU_out);
		}
		reg_inuse[find_rt(wb_in->wb_dest)]=isfree;
	}
	else{
		if(fullview)		printf("\n         WB IS NOT EXCUTED"); else ;}
}

void initialize()
{
	int i; int j; int z;
	pc_now = (pc_state )malloc(sizeof(pc_state_s));
	id_in = (if_id )malloc(sizeof(if_id_s));
	if_in = (if_id )malloc(sizeof(if_id_s));
	if_out= (if_id )malloc(sizeof(if_id_s));
	ex_in = (id_ex )malloc(sizeof(id_ex_s));
	id_out = (id_ex )malloc(sizeof(id_ex_s));
	ex_out = (ex_mem )malloc(sizeof(ex_mem_s));
	mem_in = (ex_mem )malloc(sizeof(ex_mem_s));
	mem_out = (mem_wb )malloc(sizeof(mem_wb_s));
	wb_in = (mem_wb )malloc(sizeof(mem_wb_s));

	for(i=0;i<4;i++){
		for(j=0;j<16;j++){
			cache_a[i][j].dirty=0;
			cache_a[i][j].sec=0;
			cache_a[i][j].tag=0;
			cache_a[i][j].validity=0;
			for(z=0;z<16;z++)
				cache_a[i][j].linesite[z]=0;
		}
	}
	pc_now->pc = 0x0; if_in->instr=0; if_in->NOP=0; if_in->pc=0;
	if_out->pc=0; if_out->instr=0; id_in->pc=0; id_in->instr=0; if_out->NOP=1;
	id_out->instr=0; id_out->jump_branch=0; id_out->NOP=1; id_out->pc=0; id_out->rs_v=0; id_out->rt_v=0; id_out->unc_jump=0; id_in->NOP=1;
	ex_in->instr=0; ex_in->jump_branch =0; ex_in->NOP =1; ex_in->pc=0; ex_in->rs_v=0; ex_in->rt_v=0; ex_in->unc_jump=0; ex_out->b=0;
	ex_out->ALU_out=0; ex_out->branch_target=0; ex_out->byte_count=0; ex_out->jump_branch=0; ex_out->M_read=0; ex_out->M_write=0; ex_out->NOP=1; ex_out->pc=0; ex_out->rt_v=0; ex_out->wb_dest=0; ex_out->rd_back=0; ex_out->rt_back=0;
	mem_in->ALU_out=0; mem_in->branch_target=0; mem_in->byte_count=0; mem_in->jump_branch=0; mem_in->M_read=0; mem_in->M_write=0; mem_in->NOP=1; mem_in->pc=0; mem_in->rt_v=0; mem_in->wb_dest=0; mem_in->rd_back=0; mem_in->rt_back=0;
	mem_out->ALU_out=0; mem_out->M_read=0; mem_out->NOP=1; mem_out->read_data=0; mem_out->wb_dest=0;
	wb_in->ALU_out=0; wb_in->M_read=0; wb_in->NOP=1; wb_in->read_data=0; wb_in->wb_dest=0;
}


unsigned run_pipe()
{
	int i;
	unsigned cycle = 0;
	while ((mem_out->pc)-4 != 0xffffffff && (mem_out->pc)-4 != 1) {
		if(fullview)
			printf("=============================\nCYCLE :  %d\n=============================\n", cycle);
		WB(); 
		MEM(); 
		EXE(); 
		ID(); 
		IF(cycle);  
	cycle++;
	if(cycle==25300000){	
	if(registers[2]==85)
			i=registers[2];
	}

if(fullview){
	for(i=0;i<32; i++)
		printf("R[%s] = %-12x", Registers[i], registers[i]);
	}
	}
if(fullview==0)
	{for(i=0;i<32; i++)
	printf("\n\n\nR[%s] = %-12x", Registers[i], registers[i]);}
	return cycle;
}

void cachefill(int way, int index, int address){
	int i;
	for(i=0;i<16;i++)
		cache_a[way][index].linesite[i]=buffer[((address & 0xffffffc0)/4) + i];
}

int replace(int index, int address){
	int M_tag; int M_address; int i;
	int oldest_way= oldest[index];

	while(cache_a[oldest_way][index].sec==1)
	{
		cache_a[oldest_way][index].sec=0;
		oldest_way++;
		oldest_way = oldest_way%4;
	}
	if(cache_a[oldest_way][index].dirty==1)
	{	
		for(i=0;i<16;i++)
		{
			M_tag = cache_a[oldest_way][index].tag;
			M_address = ((i&0xf)<<2) + ((index&0xf)<<6) + ((M_tag&0x3fffff)<<10);
			buffer[(M_address & 0xffffffc0)/4 + i] = cache_a[oldest_way][index].linesite[i];
			cache_a[oldest_way][index].dirty=0;
		}
	}
	if(fullview) printf("\nReplacing cache_a[%d][%d] with MEM[%08x]",oldest_way, index, address);
	cachefill(oldest_way, index, address);
	return oldest_way;
}

unsigned MemRead(unsigned address){
	int way;
	int instr;		int i;
	int offset = (address & 0x3f)/4;
	int Tag = ((address >> 10) & 0x3fffff);
	int index = ((address >> 6) & 0xf);
	for(way=0;way<4;way++){					// When I have the same instruction that was written before
		if (cache_a[way][index].validity && cache_a[way][index].tag == Tag){
			instr= cache_a[way][index].linesite[offset];
			cache_a[way][index].sec=1;
			if(fullview) printf("\nFOUND TAG:::Reading %08x from cache_a[%d][%d].linesite[%d] ", instr, way, index, offset);
			return instr;			
		}
	}
	for(way=0;way<4;way++){
		if (!(cache_a[way][index].validity)){
			cachefill(way,index,address);
			instr= cache_a[way][index].linesite[offset];
			cache_a[way][index].validity=1;
			cache_a[way][index].tag= Tag;
			cache_a[way][index].dirty=0;
			if(fullview) printf("\nReading %08x from cache_a[%d][%d].linesite[%d] ", instr, way, index, offset);
			return instr;
		}
	}
	way = replace(index, address);
	instr= cache_a[way][index].linesite[offset];
	cache_a[way][index].validity=1;
	cache_a[way][index].tag= Tag;
	cache_a[way][index].dirty=0;
	if(fullview) printf("\nReading %08x from cache_a[%d][%d].linesite[%d] ", instr, way, index, offset);
	return instr;
				//when all values are full, and could not find the data
}
			
unsigned MemWrite(unsigned address, int writingData){
	int way;
	int instr;
	int offset = (address & 0x3f)/4;
	int Tag = ((address >> 10) & 0x3fffff); 
	int index = ((address >> 6) & 0xf);
	for(way=0; way<4; way++){										// If there already exists cache, 
		if ((cache_a[way][index].validity) && (cache_a[way][index].tag == Tag)){
			cache_a[way][index].linesite[offset] = writingData;				//cachefill(way, index, pc);	
			cache_a[way][index].validity=1;
			cache_a[way][index].dirty=1;
			if(fullview) 
				printf("\nWriting %08x to cache_a[%d][%d].linesite[%d] ====Already exist", writingData, way, index, offset);
			return 1;
		}
	}
	for(way=0;way<4;way++){
		if (cache_a[way][index].validity==0){
			cache_a[way][index].linesite[offset]=writingData;				//cachefill(way, index, pc);	
			cache_a[way][index].validity=1;
			cache_a[way][index].dirty=1;
			cache_a[way][index].tag=Tag;
			if(fullview) 
				printf("\nWriting %08x to cache_a[%d][%d].linesite[%d] ", writingData, way, index, offset);
			return 1;
		}
	}
	way= replace(index, address);
	cache_a[way][index].linesite[offset]=writingData;
	cache_a[way][index].dirty=1;
	cache_a[way][index].validity=1;
	cache_a[way][index].tag=Tag;
	if(fullview)
		printf("\nWriting %08x to cache_a[%d][%d].linesite[%d]", writingData, way, index, offset);
	return 1;	
}
