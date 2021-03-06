/*
 * file_syscall.c
 *
 *  Created on: Mar 1, 2015
 *      Author: trinity
 *
 *      Added By Mohit
 */
#include <limits.h>
#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <file_syscall.h>
#include <current.h>
#include <lib.h>
#include <synch.h>
#include <copyinout.h>
#include <vfs.h>
#include <uio.h>
#include <vnode.h>
#include <stat.h>
#include <kern/unistd.h>
#include <kern/seek.h>


struct file_descriptor*
file_descriptor_init(char *name)
{
	struct file_descriptor *fd;

		fd = kmalloc(sizeof(struct file_descriptor));
		if(fd==NULL){
			return NULL;
		}

		fd->f_name=kstrdup(name);
		if(fd->f_name==NULL){
			kfree(fd);
			return NULL;
		}

		fd->f_lock=lock_create(fd->f_name);
		   	if (fd->f_lock == NULL) {
		   		kfree(fd->f_lock);
		   		kfree(fd);
			    return NULL;
		}

	return fd;

}
void
file_descriptor_cleanup(struct file_descriptor *fd)
{
	KASSERT(fd != NULL);
	lock_destroy(fd->f_lock);
	kfree(fd->f_name);
	kfree(fd);
}

int
intialize_file_desc_tbl(struct file_descriptor *file_table[]){


	char filename[5]= "con:";

	int result;
	char con_name_in[5];
	strcpy(con_name_in, filename);
	struct vnode *v;
	result= vfs_open(con_name_in, STDIN_FILENO ,0664, &v);
	if(result){
		return result;
	}
	struct file_descriptor* fd_in;
	fd_in = intialize_file_desc_con(v, 0, filename, STDIN_FILENO);
	file_table[0]=fd_in;
	vfs_close(v);


	char con_name_out[5];
	strcpy(con_name_out, filename);
	result= vfs_open(con_name_out, STDOUT_FILENO ,0664, &v);
	if(result){
		return result;
	}
	struct file_descriptor* fd_out;
	fd_out = intialize_file_desc_con(v, 0, filename, STDOUT_FILENO);
	file_table[1]=fd_out;
	vfs_close(v);

	char con_name_err[5];
	strcpy(con_name_err, filename);
	result= vfs_open(con_name_err, STDERR_FILENO ,0664, &v);
	if(result){
		return result;
	}
	struct file_descriptor* fd_err;
	fd_err = intialize_file_desc_con(v, 0, filename, STDERR_FILENO);
	file_table[2]=fd_err;
	vfs_close(v);

	for(int i=3; i<__OPEN_MAX;i++){
		file_table[i]=0;
	}
return 0;

}
struct file_descriptor*
intialize_file_desc_con(struct vnode *v, int index, char *file_name, int open_type){
	struct file_descriptor *fd;
	fd= file_descriptor_init(file_name);
	lock_acquire(fd->f_lock);
	fd->f_flag= open_type;		//fd.f_name= k_des;
	fd->f_object= v;
	fd->f_offset=0;
	fd->reference_count=1;
	curthread->file_table[index]=fd;
	lock_release(fd->f_lock);
return fd;
}

int
sys_open(userptr_t filename, int flags, int *return_val)   /*Done using copyinstr as said in blog*/
{
	struct file_descriptor *fd;
	struct vnode *vn;
	int result;
	struct stat st;
	/*void des;
	size_t length;
	if((result=copyin((const_userptr_t)filename, &des, length ))!= 0){
			return result;
	}*/
	char k_des[NAME_MAX];
	size_t actual_length;
	if((result=copyinstr((const_userptr_t)filename, k_des, NAME_MAX, &actual_length ))!= 0){
		return result;
	}
	char *file_name= k_des;
		for(int i=3; i<__OPEN_MAX;i++){
			if(curthread->file_table[i]==  0){
				if((flags&O_ACCMODE)== O_RDONLY){
					result= vfs_open(k_des, flags, 0, &vn);
				}
				else if((flags&O_ACCMODE)== O_WRONLY){
					result= vfs_open(k_des, flags, 0, &vn);
				}
				else if((flags&O_ACCMODE)== O_RDWR){
					result= vfs_open(k_des, flags, 0, &vn);
				}
				else if((flags&O_APPEND)== O_APPEND){
					result= vfs_open(k_des, flags, 0, &vn);
					if(result){
						return result;
					}
					result= VOP_STAT(vn, &st);
					if(result){
						return result;
					}
				}
				else{
					return EINVAL;
				}
				if(result){
					return result;
				}
				fd= file_descriptor_init(file_name);
				lock_acquire(fd->f_lock);
				if((flags& O_APPEND)== O_APPEND){
					fd->f_flag= O_APPEND;
				}
				else{
					fd->f_flag= (flags&O_ACCMODE);
				}

				//fd.f_name= k_des;
				fd->f_object= vn;
					fd->f_offset=0;
				//}
				fd->reference_count=1;
				*return_val= i;
				curthread->file_table[i]=fd;
				lock_release(fd->f_lock);

				break;
			}
		}
return 0;
}

int
sys_close(int fd){						/*I have done KFREE not sure, whether we have to use this or not, Just confirm once again*/

  	if(fd <0 || fd>=__OPEN_MAX){
			return EBADF;
	}
	struct file_descriptor *fd_frm_table;
	struct vnode *vn;
	fd_frm_table= curthread->file_table[fd];
	if(fd_frm_table ==0 ){
		return EBADF;
	}
	lock_acquire(fd_frm_table->f_lock);
	fd_frm_table->reference_count--;
	if(fd_frm_table->reference_count == 0){
		vn= fd_frm_table->f_object;
		vfs_close(vn);
		lock_release(fd_frm_table->f_lock);
		file_descriptor_cleanup(fd_frm_table);
		//&return_value= 0;
		curthread->file_table[fd]=0;
		return 0;
	}
	lock_release(fd_frm_table->f_lock);
	//&return_value=0;
	return 0;
}

/*Dont know how to check whether buf is valid address space or not
 *Need to do that
 *
 *
 *Getting the corresponding file_descriptor object from the table and using to read the data from the file
 *It means that file need to be open before read
 *
 *Saw the code mainly from loadelf.c and rest read the blog and implemented
 */

int
sys_read(int fd, userptr_t buf, size_t buflen, int *return_value){




	int result;
	void* kbuf = kmalloc(buflen);
	if ((result = copyin((const_userptr_t)buf, kbuf, buflen)) != 0){
		kfree(kbuf);
		return result;
	}

	if(fd <0 || fd>=__OPEN_MAX){
		return EBADF;
	}
	struct file_descriptor *fd_frm_table;
	struct vnode *vn;
	struct iovec u_iovec;
	struct uio u_uio;
	fd_frm_table= curthread->file_table[fd];

	if(fd_frm_table !=0 ){
		lock_acquire(fd_frm_table->f_lock);
		vn= fd_frm_table->f_object;

		u_iovec.iov_ubase= buf;
		u_iovec.iov_len= buflen;
		u_uio.uio_iov= &u_iovec;
		u_uio.uio_iovcnt= 1;
		u_uio.uio_offset= fd_frm_table->f_offset;
		u_uio.uio_resid= buflen;
		u_uio.uio_segflg= UIO_USERSPACE;
		u_uio.uio_rw= UIO_READ;
		u_uio.uio_space= curthread->t_addrspace;

		result= VOP_READ(vn, &u_uio);
		if(result){
			lock_release(fd_frm_table->f_lock);
			return result;
		}
		else{
			*return_value= (buflen-u_uio.uio_resid); /*Need to return the length left to read, I think*/
			fd_frm_table->f_offset+= buflen-u_uio.uio_resid;
			lock_release(fd_frm_table->f_lock);
		}
	}
	else{
		return EBADF;
	}


return 0;
}
/*
 * There are some error codes that need to be returned but dont know whether to check them or not
 */
int
sys_write(int fd, userptr_t buf, size_t nbytes, int *return_value){


	int result;
	void* kbuf = kmalloc(nbytes);
	if ((result = copyin((const_userptr_t)buf, kbuf, nbytes)) != 0){
		kfree(kbuf);
		return result;
	}
	if(fd <0 || fd>=__OPEN_MAX){
			return EBADF;
	}
	struct file_descriptor *fd_frm_table;
	struct vnode *vn;
	struct iovec u_iovec;
	struct uio u_uio;
	fd_frm_table= curthread->file_table[fd];
	if(fd_frm_table != 0){
		lock_acquire(fd_frm_table->f_lock);
		if(!(fd_frm_table->f_flag == O_RDWR || fd_frm_table->f_flag == O_WRONLY)){
			lock_release(fd_frm_table->f_lock);
			return EROFS;
		}

		u_uio.uio_offset= fd_frm_table->f_offset;
		vn= fd_frm_table->f_object;
		u_iovec.iov_ubase= buf;
		u_iovec.iov_len= nbytes;

		u_uio.uio_iov= &u_iovec;
		u_uio.uio_iovcnt= 1;

		u_uio.uio_resid= nbytes;
		u_uio.uio_rw= UIO_WRITE;
		u_uio.uio_segflg= UIO_USERSPACE;
		u_uio.uio_space= curthread->t_addrspace;

		result= VOP_WRITE(vn, &u_uio);
		if(result){
			lock_release(fd_frm_table->f_lock);
			return result;
		}
		else{
			*return_value= (nbytes-u_uio.uio_resid);
			fd_frm_table->f_offset+= nbytes-u_uio.uio_resid;
			lock_release(fd_frm_table->f_lock);
		}
	}
	else{
		return EBADF;
	}


return 0;
}

int
dup2(int oldfd, int newfd, int *return_value){

	if(newfd<=0 || newfd>=__OPEN_MAX || oldfd<0 || oldfd>=__OPEN_MAX){
		return EBADF;
	}
	else{
		struct file_descriptor *fd;
		fd= curthread->file_table[oldfd];
		if(fd==0){
			return EBADF;
		}
		int count=0;
		for(int i=3; i<__OPEN_MAX;i++){
			if(curthread->file_table[i]!= 0){
				count++;
			}
			else{
				break;
			}
		}
		if(count== __OPEN_MAX){
			return EMFILE;
		}
		if(newfd != oldfd){
			if(curthread->file_table[newfd]!= 0){
				sys_close(newfd);
			}
			curthread->file_table[newfd]= fd;
			fd->reference_count++;
			*return_value= newfd;
		}
		else{
			*return_value= oldfd;
		}
	}
return 0;
}

int
__getcwd(userptr_t buf, size_t buflen, int *return_value){
	int result;
	char k_des[PATH_MAX];
	size_t actual_length;
	if((result=copyinstr((const_userptr_t)buf, k_des, PATH_MAX, &actual_length ))!= 0){
		return result;
	}
	struct uio u_uio;
	struct iovec u_iovec;
	u_iovec.iov_ubase= buf;
	u_iovec.iov_len= buflen;

	u_uio.uio_iov= &u_iovec;
	u_uio.uio_iovcnt= 1;
	u_uio.uio_offset=0;
	u_uio.uio_resid= buflen;
	u_uio.uio_rw= UIO_READ;
	u_uio.uio_segflg= UIO_USERSPACE;
	u_uio.uio_space= curthread->t_addrspace;

	result = vfs_getcwd(&u_uio);
	if (result) {
		return result;
	}
	*return_value= u_uio.uio_resid;

return 0;
}

int
chdir(userptr_t pathname){
	int result;
	char k_des[PATH_MAX];
	size_t actual_length;
	if((result=copyinstr((const_userptr_t)pathname, k_des, PATH_MAX, &actual_length ))!= 0){
		return result;
	}
	result= vfs_chdir(k_des);
	if(result){
		return result;
	}

return 0;
}

int
lseek(int fd, off_t pos, int32_t whence, int32_t *return_value1, int32_t *return_value2){

	if(fd <0 || fd>=__OPEN_MAX){
		return EBADF;
	}
	struct file_descriptor *fd_frm_table;
	fd_frm_table= curthread->file_table[fd];
	if(fd_frm_table == 0){
			return EBADF;
	}
	//int32_t *sz= &whence;
	int* kbuf = kmalloc(sizeof(whence));
	int result;
	if ((result = copyin((const_userptr_t)whence, kbuf, sizeof(whence))) != 0){
		kfree(kbuf);
		return result;
	}
	struct vnode *vn;
	struct stat st;
	lock_acquire(fd_frm_table->f_lock);
	vn= fd_frm_table->f_object;
	int c_pos= fd_frm_table->f_offset;

	if((*kbuf&O_ACCMODE)== SEEK_SET){
		result= VOP_TRYSEEK(vn, pos);
			if(result){
				lock_release(fd_frm_table->f_lock);
				return result;
			}
			fd_frm_table->f_offset= pos;

	}
	else if((*kbuf&O_ACCMODE)== SEEK_CUR){
		result= VOP_TRYSEEK(vn, (pos+c_pos));
			if(result){
				lock_release(fd_frm_table->f_lock);
				return result;
			}
			fd_frm_table->f_offset= pos+c_pos;
	}
	else if((*kbuf&O_ACCMODE)== SEEK_END){

		result= VOP_STAT(vn, &st);
		if(result){
		lock_release(fd_frm_table->f_lock);
		return result;
		}
		off_t eof= st.st_size;
		result= VOP_TRYSEEK(vn, (pos+eof));
		if(result){
		lock_release(fd_frm_table->f_lock);
		return result;
		}
		fd_frm_table->f_offset= pos+eof;
	}
	else{
		return EINVAL;
	}
	int64_t final_value= fd_frm_table->f_offset;
	int32_t high = (int32_t)((final_value & 0xFFFFFFFF00000000) >> 32);
	int32_t low = (int32_t)(final_value & 0xFFFFFFFF);
	*return_value1= high;
	*return_value2= low;
	lock_release(fd_frm_table->f_lock);


return 0;
}
