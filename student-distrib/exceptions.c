#include "exceptions.h"
#include "rtc.h" //needed for rtc handler
#include "i8259.h"
#include "keyboard.h"
#include "x86_desc.h"
#include "lib.h"
#include "fs.h"

//pointer to the current pcb array for each termninal
pcb_t* curr_task[MAX_TERMINALS];
//array used to store which process ids are used
uint32_t pid_used[MAX_TERMINALS][MAX_PCBS] = {{0,0,0,0,0,0},{0,0,0,0,0,0},{0,0,0,0,0,0}};
//array to store the start addresses of each pcb
static uint32_t PCB_ADDR[MAX_TERMINALS][MAX_PCBS];
//file operations table for each of the different file types
operations_table_t file_operations = {read_file, write_file, open_file, close_file};
operations_table_t dir_operations = {read_dir, write_dir, open_dir, close_dir};
operations_table_t rtc_operations = {rtc_read, rtc_write, rtc_open, rtc_close};
operations_table_t stdin_operations = {terminal_read, NULL, terminal_open, terminal_close};
operations_table_t stdout_operations = {NULL, terminal_write, NULL, NULL};

/*
* void set_pcbs()
*   Inputs: void
*   Return Value: none
*	Function: sets up the pcb address array in decreasing 8kB addresses from 8MB
*/
void set_pcbs(){
	int curr_addr = PCB_ADDR_BASE;
	int x, y;
	for(y=0; y<MAX_TERMINALS; y++){
		for(x=0; x<MAX_PCBS; x++){
			PCB_ADDR[y][x] = (curr_addr -= EIGHT_KB);
		}
	}
}

/*
* set_exeptions()
*   Inputs: void
*   Return Value: none
*	Function: sets up the interrupts from the first 20 exceptions defined by intel
* 			then sets the keyboard, rtc, and system call interrupts
*/
void set_exeptions(){
	//32 reserved by intel but only 20 are defined
	SET_IDT_ENTRY(idt[0], ex_0);
	SET_IDT_ENTRY(idt[1], ex_1);
	SET_IDT_ENTRY(idt[2], ex_2);
	SET_IDT_ENTRY(idt[3], ex_3);
	SET_IDT_ENTRY(idt[4], ex_4);
	SET_IDT_ENTRY(idt[5], ex_5);
	SET_IDT_ENTRY(idt[6], ex_6);
	SET_IDT_ENTRY(idt[7], ex_7);
	SET_IDT_ENTRY(idt[8], ex_8);
	SET_IDT_ENTRY(idt[9], ex_9);
	SET_IDT_ENTRY(idt[10], ex_10);
	SET_IDT_ENTRY(idt[11], ex_11);
	SET_IDT_ENTRY(idt[12], ex_12);
	SET_IDT_ENTRY(idt[13], ex_13);
	SET_IDT_ENTRY(idt[14], ex_14);
	SET_IDT_ENTRY(idt[15], ex_15);
	SET_IDT_ENTRY(idt[16], ex_16);
	SET_IDT_ENTRY(idt[17], ex_17);
	SET_IDT_ENTRY(idt[18], ex_18);
	SET_IDT_ENTRY(idt[19], ex_19);
	SET_IDT_ENTRY(idt[KEYBOARD_IDT], ex_33);
	SET_IDT_ENTRY(idt[RTC_IDT], ex_40);	//RTC
	SET_IDT_ENTRY(idt[SYSTEM_CALL_IDT], ex_128);//system call

	//for loop below sets up first 20 interrupt handlers
	uint8_t i;
	for(i = 0; i < 20; i++){
		set_interrupt_gate(i);
	}
	set_interrupt_gate(KEYBOARD_IDT);
	set_interrupt_gate(RTC_IDT);
	set_interrupt_gate(SYSTEM_CALL_IDT); //this needs a different dpl value since needs to be accessed by user space
}

/*
* set_interrupt_gate(uint8_t i)
*   Inputs: i = interrupt number
*   Return Value: none
*	Function: helper function that sets the interrupt gate for the input interrupt
*/
void set_interrupt_gate(uint8_t i){
	idt[i].seg_selector 	= KERNEL_CS;
	idt[i].reserved4 	= 0x00;
	idt[i].reserved3 	= 0;
	idt[i].reserved2 	= 1;
	idt[i].reserved1 	= 1;
	idt[i].size 		= 1;	//side is D, 1 = 32 bits
	idt[i].reserved0	= 0;
	if(i == SYSTEM_CALL_IDT)
		idt[i].dpl 	= DPL_SYS; 		//necessary DPL value for system call
	else
		idt[i].dpl 	= 0; 		//default DPL value when no system call
	idt[i].present 		= 1;
}

/*
* ex_error()
*   Inputs: i = interrupt number
*   Return Value: none
*	Function: helper function that is called before printing the interrupt number
* this function also disables interrupts, and clears the screen, finally printing
* Error at the bottom
*/
void ex_error(){
	disable_irq(1);			//disable keyboard Interrupts
	screen_x = 0;
	screen_y = 0;
	clear();			//clear the screen
	printf("Error #");		//let user know there is an error

}

/*
* ex_halt()
*   Inputs: none
*   Return Value: none
*	Function: helper function that is called after printing the interrupt number
* more genaric info can be added to this function to be printed after any interrupt
* finally it loops indefinitly
*/
void ex_halt(){				//loop on halt
	update_cursor(screen_x, screen_y);
	while(1){}
}


/*
* ex_#()
*   Inputs: none
*   Return Value: none
*	Function: calls the genaric error page, then displays the error number, then
* 		calls the halt function where it waits
*
* 	this function is the same for the next 20 exceptions
*/
void ex_0(){
	ex_error();
	printf("0: Divide by zero\n");
	ex_halt();
}
void ex_1(){
	ex_error();
	printf("1: Debug\n");
	ex_halt();
}
void ex_2(){
	ex_error();
	printf("2: Nonmaskable Interrupts (NMI)\n");
	ex_halt();
}
void ex_3(){
	ex_error();
	printf("3: Breakpoint\n");
	ex_halt();
}
void ex_4(){
	ex_error();
	printf("4: Overflow\n");
	ex_halt();
}
void ex_5(){
	ex_error();
	printf("5: Bounds check\n");
	ex_halt();
}
void ex_6(){
	ex_error();
	printf("6: Invalid opcode\n");
	ex_halt();
}
void ex_7(){
	ex_error();
	printf("7: Device not available\n");
	ex_halt();
}
void ex_8(){
	ex_error();
	printf("8: Double fault\n");
	ex_halt();
}
void ex_9(){
	ex_error();
	printf("9: Coprocessor segment overrun\n");
	ex_halt();
}
void ex_10(){
	ex_error();
	printf("10: Invalid TSS\n");
	ex_halt();
}
void ex_11(){
	ex_error();
	printf("11: Segment not present\n");
	ex_halt();
}
void ex_12(){
	ex_error();
	printf("12: Stack segment\n");
	ex_halt();
}
void ex_13(){
	ex_error();
	printf("13: General protection\n");
	ex_halt();
}
void ex_14(){
	ex_error();
	printf("14: Page Fault\n");
	uint32_t addr;		//address where cr2 will be stored
	asm volatile(
		"movl %%cr2, %%eax\n\
		movl %%eax, %0"
		:"=r"(addr)
		:
		:"eax"
	);
	printf("CR2= %x\n", addr);	//print the address of the page fault
	ex_halt();
}
void ex_15(){
	ex_error();
	printf("15: reserved?\n");
	ex_halt();
}
void ex_16(){
	ex_error();
	printf("16: Floating-point error\n");
	ex_halt();
}
void ex_17(){
	ex_error();
	printf("17: Alignment check\n");
	ex_halt();
}
void ex_18(){
	ex_error();
	printf("18: Machine check\n");
	ex_halt();
}
void ex_19(){
	ex_error();
	printf("19: SIMD floating point\n");
	ex_halt();
}


//referenced http://wiki.osdev.org/RTC#Interrupts_and_Register_C
/*
* void rtc_handler()
*   Inputs: none
*   Return Value: none
*	Function: the rtc handler is called whenever there is an rtc interrupt,
* the register is read, then the flag is returned to 0, and we end the interrupt
*/
void rtc_handler(){	//RTC
	//have to read register C to allow interrupt to happen again
	outb(RTC_REG_C, RTC_CMD);
	//dont care about contents
	inb(RTC_MEM);
	//test_interrupts(); //for checkpoint 1 - in lib.c
	interrupt_flag = 0; //clear flag now that interrupt is over
	send_eoi(RTC_IRQ); //interrupt is over
}


/*
* int32_t sys_halt()
*   Inputs: status, and 2 garbage values
*   Return Value: the return value from the halted function
*	Function: closes all open files, doesn't allow the last shell to be closed,
*		the child task is set to current, and the program is halted
*/
int32_t sys_halt(uint8_t status, int32_t garbage2, int32_t garbage3){

	pid_used[current_terminal][curr_task[current_terminal]->process_id] = FREE; //pid no longer used
	//close all open files before halting
	uint8_t i;
	for(i=PCB_START; i<PCB_END; i++)
			sys_close(i, 0, 0);

	//if process being killed is pid0, start shell again
	//halt terminates a process, returning the specified value to its parent process
	if(curr_task[current_terminal]->parent_task == NULL){
		curr_task[current_terminal] = NULL;
		sys_execute((uint8_t*)"shell", 0,0);
	}

	curr_task[current_terminal] = curr_task[current_terminal]->parent_task;
	pcb_t* oldtask = curr_task[current_terminal]->child_task;
	curr_task[current_terminal]->child_task = NULL;

	//restore parents paging
	uint32_t pde = calc_pde_val(8*current_terminal + curr_task[current_terminal]->process_id);
	add_page(pde, VIRT_ADDR128_INDEX);
	//set cr3 register - flush TLB
	reset_cr3();

	tss.esp0 = EIGHT_MB - ( (8*current_terminal + curr_task[current_terminal]->process_id) * EIGHT_KB);
	//jmp halt_ret_label
	uint32_t ret = status;
	//restore old ebp/esp values
	asm volatile(
		"movl %0, %%eax \n\
		movl %1, %%esp \n\
		movl %2, %%ebp \n\
		jmp HALT_RET_LABEL \n\
		"
		:
		:"r"(ret), "r"(oldtask->esp), "r"(oldtask->ebp)
		:"cc"
	);
	return -1; //should never get here
}



/*
* int32_t sys_execute()
*   Inputs: command, and 2 garbage values
*   Return Value: -1 on fail, 256 if program dies by an exception, 0 to 255 if
*		program executes halt syscall
*	Function: attempts to load and execute new program by parsing the command string,
*		command is space separated sequence of words - first word is file name of program,
*		rest of command - stripped of leading spaces, is provided to program on request via getargs syscall
*/
int32_t sys_execute(const uint8_t* command, int32_t garbage2, int32_t garbage3){
	/*parse, exe check, set up paging, file loader, new pcb,
	context switch - write tss.esp0/ebp0 with new process kernel stack?
		save current esp/ebp or anything needed in pcb
		push artificial IRET context onto stack
		IRET
		halt_ret_label?
		RET
	*/
	//check for command string
	if(command == NULL)
		return -1;
	//parse name of program and arguments
	uint32_t i;
	uint8_t argsflag = 0;
	for(i = 0; i < strlen((int8_t *)command); i++){
		if(command[i] == ' '){	//find location of first space, when found break so i will contain the location
			//check for args and get index where command ends and args start
			argsflag = 1;
			break;
		}
	}
	//if flag has been set then we have arguments and need to parse
	//clear args buffers
	char program[CHAR_BUFF_SIZE];
	char arguments[CHAR_BUFF_SIZE];
	int j;
	for(j = 0; j < CHAR_BUFF_SIZE; j++){
		arguments[j] = '\0';
	}
	if(argsflag == 1){
		//arguments exist, need to parse out arguments
		strncpy(program, (int8_t *) command, i);
		program[i] = '\0';
		i++; //increment index to start of args rather than where first space is
		strncpy((int8_t *)arguments, (int8_t *) (command + i), strlen((int8_t *)command) - i);
	}
	else //no args
		strncpy(program, (int8_t *)command, strlen((int8_t *) command) + 1); //+1 copies over null terminator


	//done parsing arguments, make sure executable
	//make sure file exists
	dentry_t fileinfo;
	if(read_dentry_by_name((uint8_t *) program, &fileinfo) == -1)
		return -1;
	//file exists, make sure executable
	unsigned char buffer[4];
	read_data(fileinfo.inode_number, 0, buffer, 4);
	if(buffer[0] != MAGIC_NUM_FOR_EXE0 || buffer[1] != MAGIC_NUM_FOR_EXE1 || buffer[2] != MAGIC_NUM_FOR_EXE2 || buffer[3] != MAGIC_NUM_FOR_EXE3)
		return -1;
	//if reach here file exists and is executable
	//set up paging
	if(get_next_pid() == -1)
		return -1;
	uint32_t pde;
	if(curr_task[current_terminal]==NULL)
		pde = calc_pde_val(8*current_terminal);
	else
		pde = calc_pde_val(8*current_terminal + get_next_pid());	//will need to change later
	add_page(pde, VIRT_ADDR128_INDEX);

	//set cr3 register
	reset_cr3();

	//File Loader
	uint8_t *progbuf = (uint8_t*) PROG_EXEC_ADDR;
	uint32_t filelength = read_file_length(fileinfo.inode_number);
	read_data(fileinfo.inode_number, 0, progbuf, filelength);

	//New PCB
	new_pcb(arguments);

	//context switch

	//need to get execution point - stored li
	curr_task[current_terminal]->eip = (progbuf[MAGIC_NUM_INDEX3] << 24) + (progbuf[MAGIC_NUM_INDEX2] << 16) + (progbuf[MAGIC_NUM_INDEX1] << 8) + (progbuf[MAGIC_NUM_INDEX0]);

	//need to save old ebp/esp into pcb
	asm volatile(
		"movl %%esp, %0"
		:"=r"(curr_task[current_terminal]->esp)
	);
	asm volatile(
		"movl %%ebp, %0"
		:"=r"(curr_task[current_terminal]->ebp)
	);

	//set tss stuff
	tss.ss0 = KERNEL_DS;
	tss.esp0 = EIGHT_MB - ((8*current_terminal + curr_task[current_terminal]->process_id) * EIGHT_KB); //see kernel.c, x86_desc for tss info

	uint32_t user_stack = USER_STACK_ADDR;
	//push IRET context onto stack, not positive my eip/esp values are correct
	asm volatile(
		"movl %0, %%eax \n\
		movw %%ax, %%ds \n\
		pushl %%eax \n\
		pushl %1 \n\
		pushfl \n\
		orl %2, (%%esp) \n\
		pushl %3 \n\
		pushl %4 \n\
		iret \n\
		"
		:
		: "r" (USER_DS), "r" (user_stack), "r" (IF_FLAG), "r" (USER_CS), "r" (curr_task[current_terminal]->eip)
		: "eax", "memory", "cc"
	);


	//IRET, halt_ret_label, RET
	asm volatile(
		"HALT_RET_LABEL: \n\
		leave \n\
		ret \n\
		"
	);


	return 0;
}

/*
* int32_t sys_read()
*   Inputs: command, buffer pointer, number of bytes
*   Return Value: -1 on fail, number of bytes read
*	Function: checks for errors, then executes the read funtion of the corresponding
*		opt table for the file descriptor
*/
int32_t sys_read(int32_t fd, void* buf, int32_t nbytes){
	// fd has to be in range AND fd cannot be 1 (stdout)
	if(fd < STDIN || fd >= PCB_END  || fd == STDOUT || nbytes <= 0 || curr_task[current_terminal]->file_array[fd].flags == FREE)
		return -1;
	if(curr_task[current_terminal]->file_array[fd].opt->read == NULL)
		return -1;

	return curr_task[current_terminal]->file_array[fd].opt->read(fd, buf, nbytes);
}

/*
* int32_t sys_write()
*   Inputs: command, buffer pointer, number of bytes
*   Return Value: -1 on fail, number of bytes read
*	Function: checks for errors, then executes the write funtion of the corresponding
*		opt table for the file descriptor
*/
int32_t sys_write(int32_t fd, const void* buf, int32_t nbytes){
	//can't be negative or stdin(0) and has to be in use
	if(fd <= STDIN || fd >= PCB_END || curr_task[current_terminal]->file_array[fd].flags == FREE)
		return -1;
	if(curr_task[current_terminal]->file_array[fd].opt->write == NULL)
		return -1;

	return curr_task[current_terminal]->file_array[fd].opt->write(fd, (uint8_t*)buf, nbytes);
}

/*
* int32_t sys_open()
*   Inputs: filename pointer, 2 garbage values
*   Return Value: -1 on fail, current available file descriptor
*	Function: checks for errors, then executes the write funtion of the corresponding
*		opt table for the file descriptor
*/
int32_t sys_open(const uint8_t* filename, int32_t garbage2, int32_t garbage3){
	dentry_t temp;
	uint32_t fd;
	uint32_t curr_available = INVALID;
	//check if file exists
	if (read_dentry_by_name(filename, &temp) == INVALID){
		return -1; 				//return value for file doesn't exist
	}

	for(fd = PCB_START; fd<PCB_END; fd++){ 		//go through the file array for the current pcb
		if(curr_task[current_terminal]->file_array[fd].flags == FREE){//when a free fd is found, assign it to curr_available and break
			curr_available = fd;
			break;
		}
	}
	//if curr_available is still invalid it was never set so there are no available file descriptors
	if(curr_available == INVALID)
		return -1;

	//SET INODE NUMBER
	curr_task[current_terminal]->file_array[curr_available].inode_number = temp.inode_number;
	curr_task[current_terminal]->file_array[curr_available].flags = USED;


	switch(temp.file_type){
		case 0:
			curr_task[current_terminal]->file_array[curr_available].opt =  &rtc_operations;
			rtc_open(0, NULL, 0);
			break;

		case 1:
			curr_task[current_terminal]->file_array[curr_available].opt = &dir_operations;  //CHECK THIS <====================
			open_dir(curr_available, NULL, 0);
			break;

		case 2:
			curr_task[current_terminal]->file_array[curr_available].opt = &file_operations;  //CHECK THIS <====================
			open_file(curr_available, NULL, 0);
			break;

		default:
			curr_task[current_terminal]->file_array[curr_available].opt = &stdin_operations;
			terminal_open(0, NULL, 0);
			break;

	}

	return curr_available;
}

/*
* int32_t sys_close()
*   Inputs: file descriptor, 2 garbage values
*   Return Value: -1 on fail, 0 on success
*	Function: checks for errors, then executes the write funtion of the corresponding
*		opt table for the file descriptor
*/
int32_t sys_close(int32_t fd, int32_t garbage2, int32_t garbage3){

	if (fd <= STDIN || fd ==  STDOUT || fd > 7)
		return -1;

	if(curr_task[current_terminal]->file_array[fd].flags == FREE)
		return -1;

	curr_task[current_terminal]->file_array[fd].opt = NULL;
	curr_task[current_terminal]->file_array[fd].inode_number = INVALID_INODE;
	curr_task[current_terminal]->file_array[fd].file_position = NULL;
	curr_task[current_terminal]->file_array[fd].flags = FREE;

	return 0;
}

/*
* int32_t sys_getargs()
*   Inputs: string buffer, number of bytes to read, garbage
*   Return Value: -1 on fail, number of bytes read
*	Function: checks for errors, then copies the arguments to the provided buffer
*/
int32_t sys_getargs(uint8_t* buf, int32_t nbytes, int32_t garbage3){
	//fail if null buff pointer
	if (buf == NULL)
		return -1;

	int i;
	for(i=0; i<nbytes; i++)	//clear buff
		buf[i] = '\0';

	uint8_t* arguments = curr_task[current_terminal]->arg;
	if(arguments[0] == '\0')
		return -1;

	uint32_t arg_length = strlen((int8_t*)arguments);

	//fits in buffer?
	if (nbytes <= arg_length)
		return -1;
	//copy arguments to buffer
	for(i=0; i<arg_length; i++)
		buf[i] = arguments[i];

	return 0;
}

/*
* int32_t sys_vidmap()
*   Inputs: pointer to screen start array, 2 garbage values
*   Return Value: -1 on fail, 0 on success
*	Function: checks for errors, maps video memory into user space
*/
int32_t sys_vidmap(uint8_t** screen_start, int32_t garbage2, int32_t garbage3){
	//check to make sure screen_start is a valid address, should it be > or >=?
	if(screen_start < (uint8_t **) _128MB || screen_start >= (uint8_t **) _132MB)
		return -1;

	add_vidpage();
	*screen_start = (uint8_t*) _132MB; //want screen_start to point to 4kb thats at 132MB
	return 0;
}

/*
* int32_t sys_set_handler()
*   Inputs: signal number, handler address
*   Return Value: -1 since never written
*	Function:
*/
int32_t sys_set_handler(int32_t signum, void* handler_address, int32_t garbage3){

	return -1;
}

/*
* int32_t sys_sigreturn()
*   Inputs: 3 garbage values
*   Return Value: -1 since never written
*	Function:
*/
int32_t sys_sigreturn(int32_t garbage1, int32_t garbage2, int32_t garbage3){

	return -1;
}

/*
* int32_t get_next_pid()
*   Inputs: none
*   Return Value: process id(value from 0-5), or -1 on fail
*	Function: helper function to return the next free process id for the current termninal
*/
int32_t get_next_pid(){
	int i;
	for(i=0; i<MAX_PCBS; i++){
		if(pid_used[current_terminal][i] == FREE)
			return i;
	}
	return -1;
}

/*
* int32_t new_pcb()
*   Inputs: arguments string pointer
*   Return Value: next process id, or -1 on fail
*	Function: helper function to set up the pcb for the next process
*/
int32_t new_pcb(int8_t* arguments){
	int next_pid = get_next_pid();
	int i;

	if(next_pid == INVALID_INODE)
		return -1;
	//mark pid as used
	pid_used[current_terminal][next_pid] = USED; 		//set pid to being used
	//get address for pcb
	pcb_t* retval = (pcb_t*) PCB_ADDR[current_terminal][next_pid];

	//setup pcb file array
	for(i=PCB_START; i<PCB_END; i++){
		retval->file_array[i].opt = NULL;
		retval->file_array[i].inode_number = INVALID_INODE; 		//set an invalid value
		retval->file_array[i].file_position = 0; 	//position 0
		retval->file_array[i].flags = FREE; 	//not in use
	}

	//set stdin:
	retval->file_array[STDIN].opt = &stdin_operations;
	retval->file_array[STDIN].inode_number = INVALID_INODE;
	retval->file_array[STDIN].file_position = 0;
	retval->file_array[STDIN].flags = USED;

	//set stdout:
	retval->file_array[STDOUT].opt = &stdout_operations;
	retval->file_array[STDOUT].inode_number = INVALID_INODE;
	retval->file_array[STDOUT].file_position = 0;
	retval->file_array[STDOUT].flags = USED;

	//if curr task is null then this task is the first task
	if(curr_task[current_terminal] == NULL){
		retval->parent_task = NULL;
		retval->child_task = NULL;
		retval->process_id = next_pid;
	}
	else{
		curr_task[current_terminal]->child_task = retval;
		retval->parent_task = curr_task[current_terminal];
		retval->child_task = NULL;
		retval->process_id = next_pid;
	}

	for(i=0; i<CHAR_BUFF_SIZE; i++)
		retval->arg[i] = arguments[i];

	//set curr task of this terminal to the pointer to the current pcb
	curr_task[current_terminal] = retval;

	return  next_pid;
}
