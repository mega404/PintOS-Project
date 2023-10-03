#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "process.h"
#include "pagedir.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"

static void syscall_handler(struct intr_frame*);
void halt_wrapper();
void write_wrapper(struct intr_frame *f);
void wait_wrapper(struct intr_frame *f);
void exit_wrapper(struct intr_frame *f);
void create_wrapper(struct intr_frame *f);
void open_wrapper(struct intr_frame *f);
void remove_wrapper(struct intr_frame *f);
void exec_wrapper(struct intr_frame *f);
void close_wrapper(struct intr_frame *f);
void read_wrapper(struct intr_frame *f);
void size_wrapper(struct intr_frame *f);
void seek_wrapper(struct intr_frame *f);
void tell_wrapper(struct intr_frame *f);

void syscall_init(void) {

	intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Validating ADDR. Exit if ADDR is not a valid user vaddr. */
static int validate_addr(void *addr) {
	if (!addr || !is_user_vaddr(addr)
			|| !pagedir_get_page(thread_current()->pagedir, addr)) {
		exit(-1);
	}
	return 1;
}

/* Validating the address of the arguments of the syscall. */
static void validate_args(void *esp, int argc) {
	validate_addr(esp + (argc + 1) * sizeof(void*) - 1);
}

void* check_addr(const void *vaddr) {
	if (!is_user_vaddr(vaddr)) {
		exit(-1);
		return 0;
	}
	void *ptr = pagedir_get_page(thread_current()->pagedir, vaddr);
	if (!ptr) {
		exit(-1);
		return 0;
	}
	return ptr;
}

static void syscall_handler(struct intr_frame *f) {

	int *p = f->esp;


	/*
	 *  !valid(f->esp + 3) for sc-bound-3 specifically
	 */
	if (!valid(f->esp) || !valid(f->esp + 3)) {
		exit(-1);
	}

	int sys_code = *(int*) f->esp;
	switch (sys_code) {

	case SYS_HALT:
		halt_wrapper();
		break;
	case SYS_EXIT:
		exit_wrapper(f);
		break;
	case SYS_EXEC:
		exec_wrapper(f);
		break;
	case SYS_WAIT:
		wait_wrapper(f);
		break;
	case SYS_CREATE:
		create_wrapper(f);
		break;
	case SYS_REMOVE:
		remove_wrapper(f);
		break;
	case SYS_OPEN:
		open_wrapper(f);
		break;
	case SYS_FILESIZE:
		size_wrapper(f);
		break;
	case SYS_READ:
		read_wrapper(f);
		break;
	case SYS_WRITE:
		write_wrapper(f);
		break;
	case SYS_SEEK:
		seek_wrapper(f);
		break;
	case SYS_TELL:
		tell_wrapper(f);
		break;
	case SYS_CLOSE:
		close_wrapper(f);
		break;
	default:
		exit(-1);
	}
}

bool valid(void *vaddr) {
	return (is_user_vaddr(vaddr) && vaddr != NULL
			&& pagedir_get_page(thread_current()->pagedir, vaddr) != NULL);
}

void exit_wrapper(struct intr_frame *f) {
	int *p = f->esp;
	if (!valid(p + 1)) {
		exit(-1);
	}
	int status = *((int*) f->esp + 1);
	exit(status);
}

void exit(int status) {
	struct thread *t = thread_current();
//	t->parent->child_success = false;
	printf("%s: exit(%d)\n", t->name, status);
	t->exit_status = status;
	if (t->exec_file != NULL) {
		file_allow_write(t->exec_file);
//		file_close(&t->exec_file);
	}

//	close_all_files();
	thread_exit();
}

//void close_all_files() {
//	struct thread *t = thread_current();
//	for (struct list_elem *iter = list_begin(&t->files);
//			iter != list_end(&t->files); iter = list_next(iter)) {
//		struct fd_struct *l = list_entry(iter, struct fd_struct, elem);
//		list_remove(l);
//		free(l);
//	}
//}

void halt_wrapper() {
	halt();
}

void halt() {
	shutdown_power_off();
}

void wait_wrapper(struct intr_frame *f) {
	if (!valid(f->esp + 1)) {
		exit(-1);
	}
	int id = *((int*) f->esp + 1);
	f->eax = wait(id);
}

int wait(int id) {
	return process_wait(id);
}

void exec_wrapper(struct intr_frame *f) {
	uint32_t *args = ((uint32_t*) f->esp);
	char *ptr;
	validate_args(f->esp, 1);
	for (ptr = (char*) args[1]; validate_addr(ptr) && *ptr != '\0'; ++ptr)
		;
	if (!valid(f->esp + 1)) {
		exit(-1);
	}
	char *pointer = (char*) (f->esp + 1);
	while (*pointer != '\0') {
		if (!valid(pointer)) {
			exit(-1);
		}
		pointer++;
	}

	void *buffer = (void*) (*((int*) f->esp + 1));
	f->eax = exec(buffer);
}

int exec(char *name) {
	if (name == NULL) {
		exit(-1);
	}
	tid_t tid = process_execute(name);
	return tid;
}

void write_wrapper(struct intr_frame *f) {
	int *p = f->esp;
	if (!valid(p + 1) || !valid(p + 2) || !valid(*(p + 2)) || !valid(p + 3)) {
		exit(-1);
	}
	lock_acquire(&files_lock);
	int fd = *((int*) f->esp + 1);
	void *buffer = (void*) (*((int*) f->esp + 2));
	unsigned size = *((unsigned*) f->esp + 3);
	f->eax = write(fd, buffer, size);
	lock_release(&files_lock);
}

int write(int fd, void *buffer, int size) {

	if (fd == 1) {
		putbuf(buffer, size);
		return size;
	} else {
		for (struct list_elem *iter = list_begin(&thread_current()->files);
				iter != list_end(&thread_current()->files);
				iter = list_next(iter)) {
			struct fd_struct *l = list_entry(iter, struct fd_struct, elem);
			if (l->fd == fd) {
				return file_write(l->file, buffer, size);
			}
		}
	}
	return 0;
}

void create_wrapper(struct intr_frame *f) {

	int *p = f->esp;
	if (!valid(p + 1) || !valid(*(p + 1)) || !valid(f->esp + 2)) {
		exit(-1);
	}
	lock_acquire(&files_lock);
	void *buffer = (void*) (*((int*) f->esp + 1));
	unsigned size = *((unsigned*) f->esp + 2);
	f->eax = create(buffer, size);
	lock_release(&files_lock);

}

int create(char *file, unsigned size) {
	if (file == NULL) {
		exit(-1);
	}
	bool status = filesys_create(file, size);
	return status;
}

void open_wrapper(struct intr_frame *f) {
	int *p = f->esp;
	if (!valid(p + 1) || !valid(*(p + 1))) {
		exit(-1);
	}
	lock_acquire(&files_lock);
	void *buffer = (void*) (*((int*) f->esp + 1));
	f->eax = open(buffer);
	lock_release(&files_lock);
}

int open(char *name) {
	if (name == NULL) {
		exit(-1);
	}
	int new_fd = -1;
	struct file *f = filesys_open(name);
	if (f != NULL) {
		struct thread *t = thread_current();
		struct fd_struct *fd = (struct fd_struct*) malloc(
				sizeof(struct fd_struct));

		fd->file = f;
		fd->fd = t->files_fd;

		new_fd = fd->fd;
		t->files_fd++;

		list_push_back(&t->files, &fd->elem);
	}
	return new_fd;
}

void remove_wrapper(struct intr_frame *f) {
	if (!valid(f->esp + 1)) {
		exit(-1);
	}
	lock_acquire(&files_lock);
	void *buffer = (void*) (*((int*) f->esp + 1));

	f->eax = remove(buffer);
	lock_release(&files_lock);
}

bool remove(char *name) {
	if (name == NULL) {
		lock_release(&files_lock);
		exit(-1);
	}
	return filesys_remove(name);
}

void close_wrapper(struct intr_frame *f) {
	if (!valid(f->esp + 1)) {
		exit(-1);
	}
	lock_acquire(&files_lock);
	int fd = *((int*) f->esp + 1);
	f->eax = close(fd);
	lock_release(&files_lock);
}

int close(int fd) {
	for (struct list_elem *iter = list_begin(&thread_current()->files);
			iter != list_end(&thread_current()->files);
			iter = list_next(iter)) {
		struct fd_struct *l = list_entry(iter, struct fd_struct, elem);
		if (l->fd == fd) {
			file_close(l->file);
			list_remove(&l->elem);
			free(l);
			return fd;
		}
	}
	return -1;
}

void read_wrapper(struct intr_frame *f) {
	int *p = f->esp;
	if (!valid(p + 1) || !valid(*(p + 2)) || !valid(f->esp + 2)
			|| !valid(f->esp + 3)) {
		exit(-1);
	}
	lock_acquire(&files_lock);

	int fd = *((int*) f->esp + 1);
	void *buffer = (void*) (*((int*) f->esp + 2));
	unsigned size = *((unsigned*) f->esp + 3);
	f->eax = read(fd, buffer, size);
	lock_release(&files_lock);
}

int read_fd0(int fd, uint8_t *buffer, unsigned size) {
	int i;
	for (i = 0; i < size; i++)
		buffer[i] = input_getc();
	return size;
}
int read(int fd, void *buffer, unsigned size) {
	if (fd == 0) {
		uint8_t *buf = buffer;
		int i;
		for (i = 0; i < size; i++)
			buf[i] = input_getc();
		return size;
	} else {
		for (struct list_elem *iter = list_begin(&thread_current()->files);
				iter != list_end(&thread_current()->files);
				iter = list_next(iter)) {
			struct fd_struct *l = list_entry(iter, struct fd_struct, elem);
			if (l->fd == fd) {
				return file_read(l->file, buffer, size);
			}
		}
	}
	return -1;
}

void size_wrapper(struct intr_frame *f) {
	if (!valid(f->esp + 1)) {
		exit(-1);
	}
	lock_acquire(&files_lock);
	int fd = *((int*) f->esp + 1);
	f->eax = size(fd);
	lock_release(&files_lock);
}

int size(int fd) {
	for (struct list_elem *iter = list_begin(&thread_current()->files);
			iter != list_end(&thread_current()->files);
			iter = list_next(iter)) {
		struct fd_struct *l = list_entry(iter, struct fd_struct, elem);
		if (l->fd == fd) {
			return file_length(l->file);
		}
	}
	return -1;

}

void seek_wrapper(struct intr_frame *f) {
	if (!valid(f->esp + 1) || !valid(f->esp + 2)) {
		exit(-1);
	}
	lock_acquire(&files_lock);
	int fd = *((int*) f->esp + 1);
	unsigned position = *((unsigned*) f->esp + 2);
	f->eax = seek(fd, position);
	lock_release(&files_lock);
}

int seek(int fd, unsigned position) {
	for (struct list_elem *iter = list_begin(&thread_current()->files);
			iter != list_end(&thread_current()->files);
			iter = list_next(iter)) {
		struct fd_struct *l = list_entry(iter, struct fd_struct, elem);
		if (l->fd == fd) {
			return file_seek(l->file, position);
		}
	}
	return -1;

}

void tell_wrapper(struct intr_frame *f) {
	if (!valid(f->esp + 1)) {
		exit(-1);
	}
	lock_acquire(&files_lock);
	int fd = *((int*) f->esp + 1);
	f->eax = tell(fd);
	lock_release(&files_lock);
}

int tell(int fd) {
	for (struct list_elem *iter = list_begin(&thread_current()->files);
			iter != list_end(&thread_current()->files);
			iter = list_next(iter)) {
		struct fd_struct *l = list_entry(iter, struct fd_struct, elem);
		if (l->fd == fd) {
			return file_tell(l->file);
		}
	}
	return -1;

}

