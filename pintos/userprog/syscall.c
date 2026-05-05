#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "kernel/stdio.h"
#include "filesys/filesys.h"
#include "filesys/directory.h"
#include "threads/vaddr.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
void syscall_exit (const int exit_code);
static void check_user_addr (const void *addr);
static void check_user_laddr (const void *buf, const size_t size);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	uint64_t sys_num = f->R.rax;
	uint64_t arg0 = f->R.rdi;
	uint64_t arg1 = f->R.rsi;
	uint64_t arg2 = f->R.rdx;
	uint64_t arg3 = f->R.r10;
	uint64_t arg4 = f->R.r8;
	uint64_t arg5 = f->R.r9;

	switch (sys_num)
	{
	case SYS_WRITE:
		uint64_t fd     = arg0; // File descriptor
		void *buf       = (void *)arg1; // buffer
		size_t buf_size = (size_t)arg2; // size
		if(fd == 1) {
			putbuf(buf, buf_size);
		}

		// 사용한 바이트 수 만큼 리턴
		f->R.rax = buf_size;
		break;
	case SYS_CREATE:
		const char* file      = (const char*)arg0; // file
		unsigned initial_size = (unsigned)arg1;    // initial_size
		check_user_laddr(file, NAME_MAX);
		bool success = filesys_create(file, initial_size);
		f->R.rax = success;
		break;
	case SYS_EXIT:
		syscall_exit(arg0);
		break;
	
	default:
		break;
	}
}

/* syscall functions */

/* EXIT_CODE로 프로세스를 종료합니다. 이후 process_exit()이 호출됩니다. 
   비정상적인 종료시 EXIT_CODE에 -1를 지정하세요. */
void
syscall_exit (const int exit_code) {
	thread_current()->exit_code = exit_code;
	thread_exit();
}

/* static functions */

/* 최대 SIZE byte만큼 주소가 유효한지 검사합니다. 여러 페이지에 걸쳐서 
   있을 경우를 검사하기 위해 페이지의 시작 주소가 유효한지 검사합니다. */
static void
check_user_laddr (const void *buf, const size_t size) {
	const char *start = buf;
	const char *end = start + size;
	if(size == 0) return;
	for(uint64_t p = pg_round_down(start); p < end; p += PGSIZE) {
		check_user_addr(p);
	}
	check_user_addr(end - 1);
}

/* 포인터가 실제로 접근 가능한지 검사합니다. 
   실패하면 thread_exit()을 하고 -1을 리턴 합니다. */
static void
check_user_addr (const void *addr) {
	if(addr == NULL || !is_user_vaddr(addr) || pml4_get_page(thread_current ()->pml4, addr) == NULL) {
		syscall_exit(-1);
	}
}
