#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>
#include<tracer.h>


int nparam(u64 syscall_num){
        switch(syscall_num){
                case 2:
                case 10:
                case 11:
                case 13:
                case 15:
                case 20:
                case 21:
                case 22:
                case 38:
                case 61:
                        return 0;
                case 1:
                case 6:
                case 7:
                case 12:
                case 14:
                case 19:
                case 27:
                case 29:
                case 36:
                        return 1;
                case 4:
                case 8:
                case 9:
                case 17:
                case 28:
                case 37:
                case 40:
                        return 2;
                case 5:
                case 18:
                case 24:
                case 25:
                case 30:
                case 39:
                case 41:
                        return 3;
                case 16:
                case 23:
                case 35:
                        return 4;
                default:
                        return 0;
        }
        return 0;
}

///////////////////////////////////////////////////////////////////////////
////            Start of Trace buffer functionality                   /////
///////////////////////////////////////////////////////////////////////////

int is_valid_mem_range(unsigned long buff, u32 count, int access_bit)
{
        struct exec_context *ctx = get_current_ctx();
        int is_valid = 0; 

        
        for (int i = 0; i < MAX_MM_SEGS; i++) {
                if (ctx->mms[i].next_free) {
                        if (buff >= ctx->mms[i].start && buff + count <= ctx->mms[i].next_free) {
                        
                                if ((ctx->mms[i].access_flags & access_bit) == access_bit) {
                                        is_valid = 1; 
                                }
                                break; 
                        }
                }
        }

        
        struct vm_area *vm_area = ctx->vm_area;
        while (vm_area) {
                if (buff >= vm_area->vm_start && buff + count <= vm_area->vm_end) {
                
                        if ((vm_area->access_flags & access_bit) == access_bit) {
                        is_valid = 1; 
                        }
                        break; 
                }
                vm_area = vm_area->vm_next;
        }

    return is_valid ? 0 : -EBADMEM;
}



long trace_buffer_close(struct file *filep)
{
        if(filep->ref_count > 0){
                filep->ref_count--;

                if(filep->ref_count == 0) {
                        if(filep->trace_buffer) {
                                os_page_free(USER_REG, filep->trace_buffer);
                                filep->trace_buffer =NULL;
                        }

                        if(filep->fops){
                                os_free(filep->fops, sizeof(struct fileops));
                                filep->fops = NULL;
                        }
                        os_page_free(USER_REG, filep);
                }
        }
        return 0;
}



int trace_buffer_read(struct file *filep, char *buff, u32 count)
{

         if(!(filep->mode & O_READ)) {
                return -EINVAL;
        }

        if(count==0) {
                return 0;
        }

        struct trace_buffer_info *trace_buffer= filep-> trace_buffer;
        if(!trace_buffer){
                return -EINVAL;
        }
        u32 bytes_available = 0;
        if(trace_buffer->W==trace_buffer->R && trace_buffer->f==0){

                 return 0;
        }
        if(trace_buffer->W > trace_buffer->R) {
                bytes_available=(trace_buffer->W - trace_buffer->R);
        }else {
                bytes_available = (trace_buffer->W - trace_buffer->R) + 4096;
        }
        u32 bytes_to_read = (count < bytes_available) ? count : bytes_available;
        char *src = trace_buffer->data + trace_buffer->R;
        u32 bytes_read=0;

        while(bytes_read<bytes_to_read){
                u32 remaining_bytes = TRACE_BUFFER_MAX_SIZE - (src - trace_buffer->data);

                
                u32 bytes_in_this_iteration = (bytes_to_read - bytes_read < remaining_bytes)
                                          ? bytes_to_read - bytes_read
                                          : remaining_bytes;

                for (u32 i = 0; i < bytes_in_this_iteration; i++) {
                        buff[bytes_read + i] = src[i];
                }

                
                trace_buffer->R += bytes_in_this_iteration;
                bytes_read += bytes_in_this_iteration;

                
                if (trace_buffer->R >= TRACE_BUFFER_MAX_SIZE) {
                        
                        trace_buffer->R = 0;
                        src = trace_buffer->data;
                } else {
                        
                        src += bytes_in_this_iteration;
                }
        }
//      printk("Read Offset:%d\n",trace_buffer->R);
        if(trace_buffer->R==trace_buffer->W) trace_buffer->f=0;
        return bytes_read;

}


int trace_buffer_write(struct file *filep, char *buff, u32 count)
{

        if(!(filep->mode & O_WRITE)) {
                return -EINVAL;
        }

//      if(is_valid_mem_range((long unsigned)buff,count, 1)!=0) return -EINVAL;
        if(count==0) {
                return 0;
        }

        struct trace_buffer_info *trace_buffer= filep-> trace_buffer;
        if(!trace_buffer){
                return -EINVAL;
        }

        u32 available_space= TRACE_BUFFER_MAX_SIZE - trace_buffer->W;
        u32 bytes_to_write = count;
        u32 bytes_written=0;
        
        char * dest = trace_buffer->data + trace_buffer->W;

        while (bytes_written < bytes_to_write) {
        
                u32 remaining_space = TRACE_BUFFER_MAX_SIZE - (dest - trace_buffer->data);

        
                u32 bytes_in_this_iteration = (bytes_to_write - bytes_written < remaining_space)
                                          ? bytes_to_write - bytes_written
                                          : remaining_space;
//              printk("Remspace %d , bytes_in_this_iteration %d\n",remaining_space,bytes_in_this_iteration);
                if(trace_buffer->R > trace_buffer->W){
                        bytes_in_this_iteration=(trace_buffer->R -trace_buffer->W > bytes_in_this_iteration)
                                                ? bytes_in_this_iteration : trace_buffer->R -trace_buffer->W;
  //                    printk("Remspace %d , bytes_in_this_iteration %d\n",remaining_space,bytes_in_this_iteration);
                }
        // Copy data from the user buffer to the trace buffer
                for (u32 i = 0; i < bytes_in_this_iteration; i++) {
                        dest[i] = buff[bytes_written + i];
                }

        // Update the write offset and bytes_written
                trace_buffer->W += bytes_in_this_iteration;
                bytes_written += bytes_in_this_iteration;

        // Check if the write offset reaches the end of the buffer
                if (trace_buffer->W == TRACE_BUFFER_MAX_SIZE) {
            // Wrap around to the beginning of the buffer
                trace_buffer->W = 0;
                dest = trace_buffer->data;
                } else {
            // Move the destination pointer forward
                    dest += bytes_in_this_iteration;
                }
                if (dest == trace_buffer->data + trace_buffer->R) {
                        trace_buffer->f=1;
                        break; // Stop writing when we encounter the read offset
                }
        }
//      printk("W off:%d\n",trace_buffer->W);
        return bytes_written;

}

int sys_create_trace_buffer(struct exec_context *current, int mode)
{
//      printk("CHEck1\n");

        if(mode!=O_READ && mode!=O_WRITE && mode!=O_RDWR) {
                return -EINVAL;
        }
//       printk("CHEck2\n");
        int fd;
        for(fd=0;fd<MAX_OPEN_FILES;fd++){
                if(current->files[fd]==NULL){
                        break;
                }
        }
        if(fd==MAX_OPEN_FILES){
                return -EINVAL;
        }
//       printk("CHEck3\n");

        struct file* filep= (struct file*)os_page_alloc(USER_REG);
        if(!filep){
                return -ENOMEM;
        }
//       printk("CHEck4\n");
        filep->type = TRACE_BUFFER;
        filep->mode= mode;
        filep->offp=0;
        filep->ref_count=1;
        filep->inode=NULL;
        filep->trace_buffer=NULL;
        filep->fops=NULL;

        struct trace_buffer_info * trace_buffer = (struct trace_buffer_info*)os_page_alloc(USER_REG);
        if(!trace_buffer){
                os_page_free(USER_REG, filep);
                return -ENOMEM;
        }
//       printk("CHEck5\n");
        trace_buffer->R=0;
        trace_buffer->W=0;
        trace_buffer->mode=mode;
        trace_buffer->f=0;
        filep->trace_buffer=trace_buffer;

        struct fileops *fops = (struct fileops *)os_alloc(sizeof(struct fileops));

        if(!fops){
                os_page_free(USER_REG, filep);
                os_page_free(USER_REG, trace_buffer);
                return -ENOMEM;
        }
//       printk("CHEck6\n");
        fops->read=trace_buffer_read;
        fops->write=trace_buffer_write;
        fops->lseek=NULL;
        fops->close= trace_buffer_close;

        filep->fops=fops;

        current->files[fd]=filep;


        return fd;
}

///////////////////////////////////////////////////////////////////////////
////            Start of strace functionality                         /////
///////////////////////////////////////////////////////////////////////////

int is_exist(int syscall_num){
        struct exec_context* current = get_current_ctx();
        if(current->st_md_base==NULL) return 0;
        struct strace_head* st_md_base = current->st_md_base;
        struct strace_info* next = st_md_base->next;
        if(next==NULL) return 0;
        while(next){
                if(next->syscall_num==syscall_num) return 1;
                next=next->next;
        }
        return 0;
}


int strace_buffer_read(struct file *filep, char *buff, u32 count)
{

         if(!(filep->mode & O_READ)) {
                return -EINVAL;
        }
//      if(is_valid_mem_range((unsigned long)buff,count, 0)!=0) return -EBADMEM;
        if(count<0) return -EINVAL;
        if(count==0) {
                return 0;
        }

        struct trace_buffer_info *trace_buffer= filep-> trace_buffer;
        if(!trace_buffer){
                return -EINVAL;
        }
        u32 bytes_available = 0;
        if(trace_buffer->W==trace_buffer->R && trace_buffer->f==0){
//              printk("W=%d R=%d\n",trace_buffer->W,trace_buffer->R);
                 return 0;
        }
        if(trace_buffer->W > trace_buffer->R) {
                bytes_available=(trace_buffer->W - trace_buffer->R);
        }else {
                bytes_available = (trace_buffer->W - trace_buffer->R) + 4096;
        //if(  bytes_available <0){
        //       bytes_available = TRACE_BUFFER_MAX_SIZE + trace_buffer->W - trace_buffer->R;
        }
        u32 bytes_to_read = (count < bytes_available) ? count : bytes_available;
        char *src = trace_buffer->data + trace_buffer->R;
        u32 bytes_read=0;
//      printk("bytes_available: %d bytes_to_read: %d\n",bytes_available,bytes_to_read);
        while(bytes_read<bytes_to_read){
                u32 remaining_bytes = TRACE_BUFFER_MAX_SIZE - (src - trace_buffer->data);

                // Calculate the number of bytes to read in this iteration
                u32 bytes_in_this_iteration = (bytes_to_read - bytes_read < remaining_bytes)
                                          ? bytes_to_read - bytes_read
                                          : remaining_bytes;
                for (u32 i = 0; i < bytes_in_this_iteration; i++) {
                        buff[bytes_read + i] = src[i];
                }

                // Update the read offset and bytes_read
                trace_buffer->R += bytes_in_this_iteration;
                bytes_read += bytes_in_this_iteration;

                // Check if the read offset reaches or exceeds the buffer size
                if (trace_buffer->R >= TRACE_BUFFER_MAX_SIZE) {
                        // Wrap around to the beginning of the buffer
                        trace_buffer->R = 0;
                        src = trace_buffer->data;
                } else {
                        // Move the source pointer forward
                        src += bytes_in_this_iteration;
                }
        }
//      printk("Read Offset:%d\n",trace_buffer->R);
        if(trace_buffer->R==trace_buffer->W) trace_buffer->f=0;
        return bytes_read;

}




int strace_buffer_write(struct file *filep, char *buff, u32 count)
{

        if(!(filep->mode & O_WRITE)) {
                return -EINVAL;
        }
        if(count<0) return -EINVAL;
        if(count==0) {
                return 0;
        }

        struct trace_buffer_info *trace_buffer= filep-> trace_buffer;
        if(!trace_buffer){
                return -EINVAL;
        }

        u32 available_space= TRACE_BUFFER_MAX_SIZE - trace_buffer->W;
        u32 bytes_to_write = count;
        u32 bytes_written=0;
        
        char * dest = trace_buffer->data + trace_buffer->W;

        while (bytes_written < bytes_to_write) {
        // Calculate the remaining space until the end of the buffer
                u32 remaining_space = TRACE_BUFFER_MAX_SIZE - (dest - trace_buffer->data);

        // Calculate the number of bytes to write in this iteration
                u32 bytes_in_this_iteration = (bytes_to_write - bytes_written < remaining_space)
                                          ? bytes_to_write - bytes_written
                                          : remaining_space;
//              printk("Remspace %d , bytes_in_this_iteration %d\n",remaining_space,bytes_in_this_iteration);
                if(trace_buffer->R > trace_buffer->W){
                        bytes_in_this_iteration=(trace_buffer->R -trace_buffer->W > bytes_in_this_iteration)
                                                ? bytes_in_this_iteration : trace_buffer->R -trace_buffer->W;
  //                    printk("Remspace %d , bytes_in_this_iteration %d\n",remaining_space,bytes_in_this_iteration);
                }
        // Copy data from the user buffer to the trace buffer
                for (u32 i = 0; i < bytes_in_this_iteration; i++) {
                        dest[i] = buff[bytes_written + i];
                }

        // Update the write offset and bytes_written
                trace_buffer->W += bytes_in_this_iteration;
                bytes_written += bytes_in_this_iteration;

        // Check if the write offset reaches the end of the buffer
                if (trace_buffer->W == TRACE_BUFFER_MAX_SIZE) {
            // Wrap around to the beginning of the buffer
                trace_buffer->W = 0;
                dest = trace_buffer->data;
                } else {
            // Move the destination pointer forward
                    dest += bytes_in_this_iteration;
                }
                if (dest == trace_buffer->data + trace_buffer->R) {
                        trace_buffer->f=1;
                        break; // Stop writing when we encounter the read offset
                }
        }
//      printk("W off:%d\n",trace_buffer->W);
        return bytes_written;


}


int perform_tracing(u64 syscall_num, u64 param1, u64 param2, u64 param3, u64 param4)
{
        //printk("Trcing Started %d\n",syscall_num);
        struct exec_context *current = get_current_ctx();
//      if(!current || !current->st_md_base || current->st_md_base->is_traced!=1) {
  //              return -EINVAL;
    //    }
        if(!current){
                printk("NO CURRE\n");
                return 0;
        }
        if(syscall_num==38) return 0;
//      struct strace_head* st_md_base= current->st_md_base;
//      printk("count %d\n",st_md_base->count);
//      if(st_md_base==NULL){
  //              printk("NO st base\n");
    //            return -EINVAL;
      //  }
//      if(current->st_md_base->is_traced!=1){
//               printk("NO trace\n");
  //              return -EINVAL;
//      }
//      printk("Trcing Started %d is_trace%d\n",current->st_md_base->count,current->st_md_base->is_traced);
        int ist= current->st_md_base->is_traced && 1;
        if(ist==0){
//              printk("BYE\n");
                return 0;
        }
//      printk("Trcingfd Started %d ist %d kk %d\n",current->st_md_base->strace_fd,ist,syscall_num);
        struct trace_buffer_info* trace_buffer= current->files[current->st_md_base->strace_fd]->trace_buffer;

        if (!trace_buffer) {
           return 0; // Trace buffer is not available
        }
//      printk("All is well %d\n",syscall_num);
        if(current->st_md_base->tracing_mode== FULL_TRACING){
        u64 valid=nparam(syscall_num);

        u64 size_req= 2+valid;
//      printk("Syscall::%d params1::%d params2::%d params3::%d params4::%d\n",syscall_num,param1,param2,param3,param4);

        struct strace_info * last = (struct strace_info *)os_alloc(sizeof(struct strace_info));
        if(current->st_md_base->next==NULL){
        struct strace_info * next = (struct strace_info *)os_alloc(sizeof(struct strace_info));
        next->syscall_num=syscall_num;
        last->syscall_num=syscall_num;
        next->next=NULL;
        last->next=NULL;
        current->st_md_base->next=next;
        current->st_md_base->last=last;
        }

//      struct strace_info * last = (struct strace_info *)os_alloc(sizeof(struct strace_info));
        last->syscall_num=syscall_num;
        last->next=NULL;
        current->st_md_base->last->next=last;
        current->st_md_base->last=last;
        if(current->st_md_base->last == current->st_md_base->next) current->st_md_base->next->next=last;
//      printk("NEXT::%d last::%d\n",current->st_md_base->next->syscall_num,current->st_md_base->last->syscall_num);
        current->st_md_base->count++;
//      printk("count %d\n",current->st_md_base->count);
        u64  ret[size_req];
        u64 params[]={param1,param2,param3,param4};
        ret[0]=syscall_num;
        for(int i=0;i<valid;i++){
                ret[i+1]=params[i];
        }
        ret[size_req-1]=-1;
//      printk("ssd %d syscallnum=%d\n",sizeof(ret),syscall_num);
        //char * bw=(char *) ret;
        int wr=strace_buffer_write(current->files[current->st_md_base->strace_fd] , (char *)ret, 8*size_req);

        return 0;
        }
        if(is_exist(syscall_num)==1){
                u64 valid=nparam(syscall_num);
//              printk("Syscall_num: %d valid is %d\n",syscall_num,valid);
                u64 size_req= 2+valid;
  //            printk("Syscall filter::%d params1::%d params2::%d params3::%d params4::%d\n",syscall_num,param1,param2,param3,param4);
                u64  ret[size_req];
        u64 params[]={param1,param2,param3,param4};
        ret[0]=syscall_num;
        for(int i=0;i<valid;i++){
                ret[i+1]=params[i];
        }
        ret[size_req-1]=-1;
    //    printk("ssd %d syscall_num=%d\n",sizeof(ret),syscall_num);
        //char * bw=(char *) ret;
        int wr=strace_buffer_write(current->files[current->st_md_base->strace_fd] , (char *)ret, 8*size_req);
        }
        return 0;
}


int sys_strace(struct exec_context *current, int syscall_num, int action)
{
        if(current->st_md_base==NULL){
                if(action!=0) return -EINVAL;

                struct strace_head *st_md_base = (struct strace_head *)os_alloc(sizeof(struct strace_head));
                 st_md_base->is_traced=1;
                st_md_base->count=1;

                struct strace_info * last = (struct strace_info *)os_alloc(sizeof(struct strace_info));
                struct strace_info * next = (struct strace_info *)os_alloc(sizeof(struct strace_info));

                next->syscall_num=syscall_num;
                last->syscall_num=syscall_num;
                next->next=NULL;
                last->next=NULL;
                st_md_base->next=next;
                st_md_base->last=last;
                current->st_md_base=st_md_base;
                return 0;
        }

        if(action==0){
//              printk("inside 0 action\n");
                if(is_exist(syscall_num)==1 || current->st_md_base->count==STRACE_MAX) return -EINVAL;
//              printk("after action\n");
                struct strace_head *st_md_base=current->st_md_base;
                st_md_base->count++;
//              printk("count %d\n",st_md_base->count);
                struct strace_info * last = (struct strace_info *)os_alloc(sizeof(struct strace_info));
                if(st_md_base->next==NULL){
//                      printk("NEXT null\n");
                        struct strace_info * next = (struct strace_info *)os_alloc(sizeof(struct strace_info));
                        next->syscall_num=syscall_num;
                        last->syscall_num=syscall_num;
                        next->next=NULL;
                        last->next=NULL;
                        st_md_base->next=next;
                        st_md_base->last=last;
                        return 0;
                }
//              printk("something\n");
                last->syscall_num=syscall_num;
                last->next=NULL;
                struct strace_info* temp=st_md_base->next;
                if(temp==NULL){
                        st_md_base->next=last;
                        st_md_base->last=last;
                        return 0;
                }
                while(temp->next){
                        temp=temp->next;
                }
                temp->next=last;
                st_md_base->last=last;
                return 0;
        }

        if(current->st_md_base->next==NULL || is_exist(syscall_num)==0) return -EINVAL;
        current->st_md_base->count--;
        struct strace_info * temp=current->st_md_base->next;
        if(temp->syscall_num==syscall_num){
                current->st_md_base->next=temp->next;
                os_free(temp,sizeof(struct strace_info));
                return 0;
        }
        int flag=0;
        while(temp->next){
                if(temp->next->syscall_num==syscall_num){
                        struct strace_info *del=temp->next;
                        temp->next=temp->next->next;
                        os_free(del,sizeof(struct strace_info));
                        return 0;
                }
                temp=temp->next;
        }
        return -EINVAL;

}

int sys_read_strace(struct file *filep, char *buff, u64 count)
{
        struct exec_context * current = get_current_ctx();
        struct strace_head * st_md_base=current->st_md_base;
        //int cnt=st_md_base->count;
        //int num=(cnt>count) ? count:cnt;
        int wrtn=0;
        int rea=0;
        //char * buff1=buff;
        //printk("numm:%d co%d cn%d\n",num,count,cnt);
        while(wrtn<count){
                char * buff1;
                char  sys[8];
                if(strace_buffer_read(filep,sys,8)==0) break;
                //if(rd==0) break;
                //printk("rd=======%d\n",rd);
                u64 *syscall_num=(u64 *)sys;
                u64 sys_num=syscall_num[0];
                u64 prm=nparam(sys_num);
                //rea=rea+8*(1+prm);
                printk("no:%d np:%d\n",sys_num,prm);
                char  *params;
                //char buff1[8+8*prm];
                strace_buffer_read(filep,params,8*nparam(syscall_num[0]));
                wrtn++;
                printk("sys1 %d param1 %d\n",sizeof(sys),sizeof(params));
                //memcpy(buff1,sys,8);
                //memcpy(buff1+8,params,8*nparam(syscall_num[0]));
        //      printk("sys1::: %d\n",((u64 *)buff1)[1]);
                for(int i=0;i<8;i++){
                        buff[i+rea]=sys[i];
                }
                for(int j=0;j<8*prm;j++){
                        buff[rea+8+j]=params[j];
                }
                char * temp;
                int x=strace_buffer_read(filep,temp,8);
                //memcpy(buff+rea,buff1,8*(1+prm));
                rea=rea+8*(1+prm);
        //      printk("x::%d\n",x);
                //rea+=8+8*nparam(syscall_num[0]);
        //      if(x==8){
        //              braaaaaeak;
        //      }
                printk("rea is equal to%d\n",rea);
        }
        printk("iterations %d:\n",wrtn);
        return rea;
}

int sys_start_strace(struct exec_context *current, int fd, int tracing_mode)
{
        if(fd<0 || fd>=MAX_OPEN_FILES || !current->files[fd]){
                return -EINVAL;
        }
        struct file* filep =current->files[fd];
        if(!filep->trace_buffer){
                return -EINVAL;
        }

        if(tracing_mode!= FULL_TRACING && tracing_mode!=FILTERED_TRACING){
                return -EINVAL;
        }
        if(current->st_md_base==NULL){
                printk("correct interpre\n");
                struct strace_head *st_md_base = (struct strace_head *)os_alloc(sizeof(struct strace_head));
                if(!st_md_base){
                        //os_free(USER_REG,st_md_base);
                        return -EINVAL;
                }
        //current->st_md_base=st_md_base;
//      if(tracing_mode==FULL_TRACING) {
                st_md_base->is_traced=1;
//      }
                st_md_base->strace_fd=fd;
                st_md_base->tracing_mode = tracing_mode;
                st_md_base->count=0;
                st_md_base->next =NULL;
                st_md_base->last=NULL;
                current->st_md_base=st_md_base;
                printk("Count  fbbdbdfbgfb  %d\n",current->st_md_base->strace_fd);
                return 0;
        }
        struct strace_head* st_md_base= current->st_md_base;
        st_md_base->strace_fd=fd;
        st_md_base->tracing_mode=tracing_mode;
        printk("next %d last %d\n",st_md_base->next->syscall_num,st_md_base->last->syscall_num);
        return 0;
}

int sys_end_strace(struct exec_context *current)
{
//      struct exec_context *current = get_current_ctx();

        if(!current) {
                return -EINVAL;
        }

        struct strace_head* st_base = current->st_md_base;
        //st_base->is_traced=0;
        //st_base->strace_fd=-1;
        //st_base->count=0;

        struct strace_info * ptr = st_base->next;
        while(ptr){
                struct strace_info* temp =ptr;
                ptr=ptr->next;
                os_free(temp,sizeof(struct strace_info));
        }
        struct strace_info * ptr1 =st_base->last;
        if(ptr1) os_free(ptr1,sizeof(struct strace_info));
        os_free(st_base,sizeof(struct strace_head));
        current->st_md_base=NULL;
        return 0;
}



///////////////////////////////////////////////////////////////////////////
////            Start of ftrace functionality                         /////
///////////////////////////////////////////////////////////////////////////

int exist(struct ftrace_head* ft_md_base,unsigned long faddr){
        if(ft_md_base->next==NULL) return 0;
        struct ftrace_info* temp=ft_md_base->next;
        while(temp){
                if(temp->faddr==faddr){
                        return 1;
                }
                temp=temp->next;
        }
        return 0;
}

long do_ftrace(struct exec_context *ctx, unsigned long faddr, long action, long nargs, int fd_trace_buffer)
{
        if(ctx->ft_md_base==NULL){
                if(action!=ADD_FTRACE) return -EINVAL;
                struct ftrace_head *ft_md_base=(struct ftrace_head *)os_alloc(sizeof(struct ftrace_head));
                ft_md_base->count=1;
                struct ftrace_info* info=(struct ftrace_info*)os_alloc(sizeof(struct ftrace_info));
                info->faddr=faddr;
                info->num_args=nargs;
                info->fd=fd_trace_buffer;
                info->next=NULL;
                info->capture_backtrace=0;
                ft_md_base->next=info;
                ft_md_base->last=info;
        }
        if(action==ADD_FTRACE){
                if(exist(ctx->ft_md_base,faddr)==1) return -EINVAL;
                if(ctx->ft_md_base->count==FTRACE_MAX) return -EINVAL;
                struct ftrace_info* info=(struct ftrace_info*)os_alloc(sizeof(struct ftrace_info));
                info->faddr=faddr;
                info->num_args=nargs;
                info->fd=fd_trace_buffer;
                info->next=NULL;
                info->capture_backtrace=0;
                struct ftrace_info* temp=ctx->ft_md_base->next;
                if(temp==NULL){
                        ctx->ft_md_base->next=info;
                        ctx->ft_md_base->last=info;
                        return 0;
                }
                while(temp->next){
                        temp=temp->next;
                }
                temp->next=info;
                ctx->ft_md_base->last=info;
                return 0;
        }
        if(action==REMOVE_FTRACE){
                if(exist(ctx->ft_md_base,faddr)==0) return -EINVAL;
                struct ftrace_info* temp=ctx->ft_md_base->next->next;
                struct ftrace_info* prev=ctx->ft_md_base->next;
                if(prev->faddr==faddr){
                        ctx->ft_md_base->next=NULL;
                        ctx->ft_md_base->last=NULL;
                }
                while(temp){
                        if(temp->faddr==faddr){
                                if(temp==ctx->ft_md_base->last){
                                        ctx->ft_md_base->last=prev;
                                }
                                prev->next=temp->next;
                                os_free(temp,sizeof(struct ftrace_info));
                                return 0;
                        }
                        temp=temp->next;
                        prev=prev->next;
                }
                return -EINVAL;
        }
        if(action == ENABLE_FTRACE){
                if(exist(ctx->ft_md_base,faddr)==0) return -EINVAL;
                /*asm volatile(
                        "movl $%0, (%1)"
                        :a
        aa: "r" (INV_OPCODE), "r" (faddr)
                );
        }       */      }
        return 0;
}

//Fault handler
long handle_ftrace_fault(struct user_regs *regs)
{
        return 0;
}


int sys_read_ftrace(struct file *filep, char *buff, u64 count)
{
    return 0;
}
