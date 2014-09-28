#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <tchar.h>
#include <psapi.h>
#include <process.h>
#include "comms.h"

gscomms * g = NULL;

int main(int argc, char *argv[]){

	printf("Mappar v0.1 - ppcasm\n\n");

	if(argc<2){
		printf("Wrong Usage: %s <filename>\n\n", argv[0]);
        exit(-1);
	}
	
    CreateProcessA(argv[1],"",NULL,NULL,TRUE,CREATE_SUSPENDED,NULL,NULL,&si,&pi);

	ResumeThread(pi.hThread);
	
    if(!pi.dwProcessId){
		printf("CreateProcessA Failed. (perhaps the file <%s> doesn't exist)\n", argv[1]);
		exit(-1);
	}

	printf("\nFound ProcessID (%ul)\n", (unsigned int) pi.dwProcessId);

	mappar();

	return 0;
}

int mappar(void){

	g = Init_Comms();

	c.ContextFlags = CONTEXT_FULL;
	
	si.cb = sizeof(STARTUPINFO);
	si.wShowWindow = SW_NORMAL;

	ZeroMemory(&si,sizeof(STARTUPINFO));
	
	//Debug hooking
	BOOL bo=DebugActiveProcess(pi.dwProcessId);
	if(bo==0){
		printf("DebugActiveProcess Failed - Exiting\n");
	}

	printf("\n\nREADY! - SET! - GO!\n\n");

	EnterDebugLoop();

	return 0;
}

void EnterDebugLoop(){

unsigned long ins = 0; 
unsigned long cnt = 0;
unsigned char proc1 = 0; //ExitProcess first byte
unsigned char proc2 = 0; //TerminateProcess first byte

//Get address of ExitProcess and TerminateProcess funcs.
HMODULE libz = LoadLibraryA("Kernel32.dll");
if(!libz){
	printf("LoadLibraryA Failed.\n");
	exit(-1);
}

FARPROC exitprocaddr = GetProcAddress(libz, "ExitProcess");
if(!exitprocaddr){
	printf("GetProcAddress Failed.\n");
	exit(-1);
}

FARPROC termprocaddr = GetProcAddress(libz, "TerminateProcess");
if(!termprocaddr){
	printf("GetProcAddress Failed.\n");
	exit(-1);
}

//Read first byte from both addresses...
proc1 = RB(pi.dwProcessId, (unsigned int)exitprocaddr);
proc2 = RB(pi.dwProcessId, (unsigned int)termprocaddr);

//Set INT3 (0xCC) breakpoint at start of ExitProcess and TerminateProcess
WB(pi.dwProcessId, (unsigned long)exitprocaddr, 0xCC);
WB(pi.dwProcessId, (unsigned long)termprocaddr, 0xCC);

	for(;;)
	{
	
	WaitForDebugEvent(&de, INFINITE); 
	
	switch (de.dwDebugEventCode)
	{
    	case EXCEPTION_DEBUG_EVENT: 
        	switch(de.u.Exception.ExceptionRecord.ExceptionCode)
        	{
        		case EXCEPTION_BREAKPOINT: 
        		
        		if(cnt==0){
        			c.Eip++;
        			cnt++;
        			break;
				}
				
             	WB(pi.dwProcessId, (unsigned long)exitprocaddr, proc1);
             	WB(pi.dwProcessId, (unsigned long)termprocaddr, proc2);
             	exit(0);
             	break;
             
        		case EXCEPTION_PRIV_INSTRUCTION: 
        		if(SuspendThread(pi.hThread)==-1){
					printf("SuspendThread Failed.\n"); 
					exit(-1);
				}
				
        		if(!GetThreadContext(pi.hThread, &c)){
        			printf("GetThreadContext Failed.\n");
        			exit(-1);
        		}
        		
        		ins = RB(pi.dwProcessId, c.Eip); //Get insn at EIP in remote thread
        		
        		switch(ins)
        		{
        			case 0xEC: // IN AL, DX
        			c.Eax = READ_PORT(g)&0xff;
        			//printf("I: %02x\n", c.Eax);
        			c.Eip++;
        			break;
        			
        			case 0xED: // IN EAX, DX
        			c.Eax = READ_PORT(g)&0xff;
        			//printf("I: %x\n", c.Eax);
        			c.Eip++;
        			break;
        			
        			case 0xEE: // OUT DX, AL
        			WRITE_PORT(g, c.Eax&0xff);
        			//printf("O: %02x\n", c.Eax);
        			c.Eip++;
        			break;
        			
        			default:
        			printf("Unimp insn: %x\n", (unsigned int)ins);
        			c.Eip++;
        			break;
        		}
		        
        		if(!SetThreadContext(pi.hThread, &c)){
        			printf("SetThreadContext Failed.\n");
        			exit(-1);
        		} 
        		
				if(ResumeThread(pi.hThread)==-1){
					printf("ResumeThread Failed.\n");
					exit(-1);
				}
            	break;
        	}
        	
    	break;
	}	

	ContinueDebugEvent(de.dwProcessId, de.dwThreadId, DBG_CONTINUE);

	}

}

//Read Byte from arbitrary process.
unsigned char RB(DWORD ProcID, unsigned int lpBaseAddr){

	DWORD OLDPROTECT[1024];

	//Get a handle to the process.
    HANDLE hProcess = OpenProcess(PROCESS_VM_OPERATION|PROCESS_VM_READ, FALSE, ProcID);

	unsigned char lpBuffer[32];
    
	if(!VirtualProtectEx(hProcess, (LPVOID)lpBaseAddr, 1, PAGE_EXECUTE_READWRITE, OLDPROTECT)){
		CloseHandle(hProcess);
		printf("READBYTEMEM: VirtualProtectEx Failed! - Exiting\n");
		exit(-1);
	}

	if(!ReadProcessMemory(hProcess, (LPCVOID)lpBaseAddr, lpBuffer, 1, NULL)){
		CloseHandle(hProcess);
		printf("ReadProcessMemory Failed! Exiting...\n");
		exit(-1);
	}

	CloseHandle(hProcess);
    return lpBuffer[0];
	
}

//Write Byte to arbitary process.
unsigned int WB(DWORD ProcID, unsigned int lpBaseAddr, unsigned char value){

	DWORD OLDPROTECT[1024];
	unsigned char lpBuffer[1];
	lpBuffer[0]=value;

	//Get a handle to the process.
    HANDLE hProcess = OpenProcess(PROCESS_VM_OPERATION|PROCESS_VM_WRITE, FALSE, ProcID);

	if(!VirtualProtectEx(hProcess, (LPVOID)lpBaseAddr, 32, PAGE_EXECUTE_READWRITE, OLDPROTECT)){
		CloseHandle(hProcess);
		printf("WRITEBYTEMEM: VirtualProtectEx Failed! - Exiting\n");
		exit(-1);
	}
		
	if(!WriteProcessMemory(hProcess, (LPVOID)lpBaseAddr, lpBuffer, 1, NULL)){
		CloseHandle(hProcess);
		printf("WriteProcessMemory Failed! Exiting...\n");
		exit(-1);
	}
 
	CloseHandle(hProcess);
	return 0;
}



