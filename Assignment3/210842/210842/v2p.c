#include <types.h>
#include <mmap.h>
#include <fork.h>
#include <v2p.h>
#include <page.h>

/* 
 * You may define macros and other helper functions here
 * You must not declare and use any static/global variables 
 * *//*

*/			
	
void FLUSH(){
	
	u64 cr3_val;

	asm volatile (
		"mov %%cr3, %0"
		: "=r" (cr3_val)
	);

	asm volatile (
		"mov %0, %%rax\n\t"
		"mov %%rax, %%cr3"
		:
		: "r" (cr3_val)
		:"eax"
	);
}
void free_pfns(struct exec_context *current, u64 addr);

void protect_pfns(struct exec_context *current, u64 addr,u64 prot){
	struct vm_area *c = current->vm_area->vm_next;
        while(c!=NULL){
                if(addr >= c->vm_start && addr < c->vm_end) break;
                c=c->vm_next;
        }
        if(c==NULL) return;
        u64 pgd=(u64)osmap(current->pgd);
        u64 pgd_off= addr>>39 & 0x1FF;
        u64 pgdt=pgd+pgd_off*8;
        u64 pgd_t = *((u64 *)pgdt);
        if((pgd_t & 0x1)==0) return;
        u64 pud_=pgd_t>>12;
        u64 pud=pud_<<12;
        u64 pud_off= addr>>30 & 0x1FF;
        u64 pudt=pud+pud_off*8;
        u64 pud_t = *((u64 *)pudt);
        if((pud_t & 0x1)==0) return;
        u64 pmd_=pud_t>>12;
        u64 pmd=pmd_<<12;
        u64 pmd_off= addr>>21 & 0x1FF;
        u64 pmdt=pmd+pmd_off*8;
        u64 pmd_t = *((u64 *)pmdt);
        if((pmd_t & 0x1)==0) return;
        u64 pte_=pmd_t>>12;
        u64 pte=pte_<<12;
        u64 pte_off= addr>>12 & 0x1FF;
        u64 ptet=pte+pte_off*8;
        u64 pte_t = *((u64 *)ptet);
        if((pte_t & 0x1)==0) return;
	//printk("3rd bit ptet %d\n",(pte_t & 0x8));
        if(prot == PROT_READ) {pte_t = pte_t | 0x8;pte_t=pte_t^0x8;}

	//printk("3rd bit ptet %d\n",(pte_t & 0x8));
	if(prot == (PROT_READ|PROT_WRITE)) {pte_t =pte_t|0x8;}
	//printk("6rd bit ptet %d\n",(pte_t & 0x8));
	//if((pte_t & 0x8) ==0) printk("correct done\n");
	//printk("not done\n");
        //u64 pte_=pte_t>>12;
        //u64 pt=pte_<<12;
        //os_pfn_free(USER_REG,pt);
        *((u64 *)ptet)=pte_t;
}

/**
 * mprotect System call Implementation.
 */
long vm_area_mprotect(struct exec_context *current, u64 addr, int length, int prot)
{
	length = (length%PAGE_SIZE)? (length/PAGE_SIZE +1)*PAGE_SIZE : length;
	if((addr < MMAP_AREA_START) || ((addr+length) > MMAP_AREA_END)) return -1;
	int val=0;
	if(current->vm_area->vm_next==NULL) return -1;
	
	struct vm_area* c=current->vm_area->vm_next;
        struct vm_area* p=current->vm_area;
        while(c!=NULL){
                if(addr<= c->vm_start && c->vm_end <= addr+length){
                        /*stats->num_vm_area--;
                        struct vm_area * temp=c;
                        c=c->vm_next;
                        p->vm_next=c;
                        os_free(temp,sizeof(struct vm_area));*/
			c->access_flags=prot;
			p=c;
			c=c->vm_next;
                }
                else if(addr<= c->vm_start && c->vm_start < addr+length){
                        /*c->vm_start= addr+length;*/
			stats->num_vm_area++;
			if(stats->num_vm_area>128) return -1;
			struct vm_area* temp= os_alloc(sizeof(struct vm_area));
			temp->vm_start=c->vm_start;
			temp->vm_end=addr+length;
			temp->access_flags=prot;
			p->vm_next=temp;
			c->vm_start=addr+length;
			temp->vm_next=c;
                        p=c;
                        c=c->vm_next;
                }
                else if(addr > c->vm_start && c->vm_end > addr+length){
                        stats->num_vm_area+=2;
                        if(stats->num_vm_area>128) return -1;
                        struct vm_area * temp = os_alloc(sizeof(struct vm_area));
			struct vm_area * temp1 = os_alloc(sizeof(struct vm_area));
                        temp->vm_start = addr + length;
                        temp->vm_end= c->vm_end;
                        temp->vm_next=c->vm_next;
                        temp->access_flags=c->access_flags;
			temp1->vm_start=addr;
			temp1->vm_end=addr+length;
			temp1->vm_next=temp;
			temp1->access_flags=prot;
                        c->vm_end=addr;
                        c->vm_next=temp1;
                        p=temp;
                        c=temp->vm_next;
                }
                else if(addr > c->vm_start && c->vm_end > addr){
                        stats->num_vm_area++;
                        if(stats->num_vm_area>128) return -1;
                        struct vm_area* temp= os_alloc(sizeof(struct vm_area));
                        temp->vm_start=addr;
                        temp->vm_end=c->vm_end;
                        temp->access_flags=prot;
                        //p->vm_next=temp;
                        c->vm_end=addr;
                        temp->vm_next=c->vm_next;
			c->vm_next=temp;
                        p=temp;
                        c=temp->vm_next;


			//c->vm_end=addr;
                        //p=c;
                        //c=c->vm_next;
                }
                else{
                        p=c;
                        c=c->vm_next;
                }



        }

	//printk("HH\n");
	 c = current->vm_area;
                p = NULL;
                while (c != NULL)
                {
                        if (p != NULL)
                        {
                        //check whether to merge this vm_area with the previous one
                                if (p->vm_end == c->vm_start && p->access_flags == c->access_flags)
                                {
                         //merge this vm_Area with the previous one
                                    p->vm_end = c->vm_end;
                                    p->vm_next = c->vm_next;
                                    //dealloc_vm_area(y);
                                    os_free(c,sizeof(struct vm_area));
                                    stats->num_vm_area--;
                                    c = p->vm_next;
                                    continue;
                             //remove the pages allocated for this area
                                }
                        }
                        p = c;
                        c = c->vm_next;
                }
	
	// printk("22\n");
	for(u64 i=addr;i<addr+length;i+=PAGE_SIZE){
		protect_pfns(current,i,prot);
	}
	FLUSH();
    	return val;
}

/**
 * mmap system call implementation.
 */
long vm_area_map(struct exec_context *current, u64 addr, int length, int prot, int flags)
{
	// printk("fekk %d\n", stats->num_vm_area);
	if((flags!=0 && flags!=MAP_FIXED)) return -1;
	// printk("kyun:\n");
        if(prot!=PROT_READ && prot!= (PROT_READ | PROT_WRITE)) return -1;
	// printk("kuyn ddnbcj\n");
	if(length<0) return -1;
	length = (length%PAGE_SIZE)? (length/PAGE_SIZE +1)*PAGE_SIZE : length;
	if(length>524288) return -1;
    	long ret_addr = -1;
	// printk("helele\n");
	// if(current->vm_area==NULL) printk("NAhi hai:%d\n",sizeof(struct vm_area));
	if(stats->num_vm_area==0){
		stats->num_vm_area++;
		struct vm_area * dummy = os_alloc(sizeof(struct vm_area));
		dummy->vm_next=NULL;
		dummy->vm_start= MMAP_AREA_START;
		dummy->vm_end= MMAP_AREA_START+ PAGE_SIZE;
		dummy->access_flags=0x0;
		current->vm_area=dummy;
	}
	//if((flags!=0 && flags!=MAP_FIXED) || (prot!=PROT_READ && prot!=PROT_READ | PROT_WRITE)|| length<0 || length>524288) return -1;
	if(!addr){
		if(flags==MAP_FIXED) return -1;
		// printk("inside null add fekk de\n");
		struct vm_area* c=current->vm_area->vm_next;
		struct vm_area* p=current->vm_area;
		u64 curr_addr= current->vm_area->vm_end;
		if(c==NULL){
			//  printk("fekk na\n");
			stats->num_vm_area++;
			struct vm_area * temp = os_alloc(sizeof(struct vm_area));
                	temp->vm_next=NULL;
                	temp->vm_start= curr_addr;
                	temp->vm_end= curr_addr + length;
                	temp->access_flags=prot;
			current->vm_area->vm_next=temp;
			return temp->vm_start;
		}
		while(c!=NULL){
			//  printk("fekk hi do\n");
			if(curr_addr + length <= c->vm_start){
				//  printk("fekk sale\n");
				//if(curr_addr+length
				stats->num_vm_area++;
                        	struct vm_area * temp = os_alloc(sizeof(struct vm_area));
                        	temp->vm_next=NULL;
                        	temp->vm_start= curr_addr;
                        	temp->vm_end= curr_addr + length;
                        	temp->access_flags=prot;
				p->vm_next=temp;
				temp->vm_next=c;
				ret_addr= curr_addr;
				break;
			}
			curr_addr=c->vm_end;
			p=c;
			c=c->vm_next;
		}
		if(c==NULL){
			// printk("fekk sa\n");
			if(p->vm_end + length <=MMAP_AREA_END){
				// printk("fekk sle\n");
				stats->num_vm_area++;
				if(stats->num_vm_area>128) return -1;
				struct vm_area * temp = os_alloc(sizeof(struct vm_area));
                                temp->vm_next=NULL;
                                temp->vm_start= p->vm_end;
                                temp->vm_end= p->vm_end + length;
                                temp->access_flags=prot;
                                p->vm_next=temp;
				ret_addr= p->vm_end;
			}
		}

		c = current->vm_area;
	        p = NULL;
        	while (c != NULL)
        	{
            		if (p != NULL)
            		{
                	//check whether to merge this vm_area with the previous one
                		if (p->vm_end == c->vm_start && p->access_flags == c->access_flags)
                		{
                	 //merge this vm_Area with the previous one
                		    p->vm_end = c->vm_end;
                		    p->vm_next = c->vm_next;
                		    //dealloc_vm_area(y);
				    os_free(c,sizeof(struct vm_area));
                		    stats->num_vm_area--;
                		    c = p->vm_next;
                		    continue;
               		     //remove the pages allocated for this area
                		}
            		}
            		p = c;
            		c = c->vm_next;
        	}
	}

	else{
		// printk("Inside mapfixed\n");
		struct vm_area* c=current->vm_area->vm_next;
                struct vm_area* p=current->vm_area;
                int fl=0;
		u64 curr_addr= current->vm_area->vm_end;
                if(c==NULL){
                        //  printk("fekk na\n");
                        stats->num_vm_area++;
                        struct vm_area * temp = os_alloc(sizeof(struct vm_area));
                        temp->vm_next=NULL;
                        temp->vm_start= addr;
                        temp->vm_end= addr + length;
                        temp->access_flags=prot;
                        current->vm_area->vm_next=temp;
                        return temp->vm_start;
                }
		while(c!=NULL){
			if(curr_addr <= addr && addr + length <= c->vm_start){
				fl=1;
				stats->num_vm_area++;
                                struct vm_area * temp = os_alloc(sizeof(struct vm_area));
                                temp->vm_next=NULL;
                                temp->vm_start= addr;
                                temp->vm_end= addr + length;
                                temp->access_flags=prot;
                                p->vm_next=temp;
                                temp->vm_next=c;
                                ret_addr= addr;
                                break;
                        }
                        curr_addr=c->vm_end;
                        p=c;
                        c=c->vm_next;
                }
		if(c==NULL){
                        // printk("fekk sale nhi mila\n");
                        if(p->vm_end<= addr && addr + length <=MMAP_AREA_END){
                                // printk("fekk sle %d\n",addr);
				fl=1;
                                stats->num_vm_area++;
                                if(stats->num_vm_area>128) return -1;
                                struct vm_area * temp = os_alloc(sizeof(struct vm_area));
                                temp->vm_next=NULL;
                                temp->vm_start= addr;
                                temp->vm_end= addr + length;
                                temp->access_flags=prot;
                                p->vm_next=temp;
                                ret_addr= addr;
                        }
                }
		if(fl==0){
			if(flags==MAP_FIXED)
				return -1;
			return vm_area_map(current,0,length,prot,flags);
		}
		c = current->vm_area;
                p = NULL;
                while (c != NULL)
                {
                        if (p != NULL)
                        {
                        //check whether to merge this vm_area with the previous one
                                if (p->vm_end == c->vm_start && p->access_flags == c->access_flags)
                                {
                         //merge this vm_Area with the previous one
                                    p->vm_end = c->vm_end;
                                    p->vm_next = c->vm_next;
                                    //dealloc_vm_area(y);
                                    os_free(c,sizeof(struct vm_area));
                                    stats->num_vm_area--;
                                    c = p->vm_next;
                                    continue;
                             //remove the pages allocated for this area
                                }
                        }
                        p = c;
                        c = c->vm_next;
                }
	}

    return ret_addr;
}

/**
 * munmap system call implemenations
 */

long vm_area_unmap(struct exec_context *current, u64 addr, int length)
{
	length = (length%PAGE_SIZE)? (length/PAGE_SIZE +1)*PAGE_SIZE : length;
	int val=0;
	struct vm_area* c=current->vm_area->vm_next;
       	struct vm_area* p=current->vm_area;
	while(c!=NULL){
		if(addr<= c->vm_start && c->vm_end <= addr+length){
			// printk("addr<= c->vm_start && c->vm_end <= addr+length\n");
			stats->num_vm_area--;
			struct vm_area * temp=c;
			c=c->vm_next;
			p->vm_next=c;
			os_free(temp,sizeof(struct vm_area));
		}
		else if(addr<= c->vm_start && c->vm_start < addr+length){
			// printk("addr<= c->vm_start && c->vm_start < addr+length\n");
                	c->vm_start= addr+length;
			p=c;
			c=c->vm_next;
		}
		else if(addr > c->vm_start && c->vm_end > addr+length){
			// printk("addr > c->vm_start && c->vm_end > addr+length\n");
                	stats->num_vm_area++;
			if(stats->num_vm_area>128) return -1;
			struct vm_area * temp = os_alloc(sizeof(struct vm_area));
			temp->vm_start = addr + length;
			temp->vm_end= c->vm_end;
			temp->vm_next=c->vm_next;
			temp->access_flags=c->access_flags;
			c->vm_end=addr;
			c->vm_next=temp;
			p=temp;
			c=temp->vm_next;
		}
		else if(addr > c->vm_start && c->vm_end > addr){
			c->vm_end=addr;
			p=c;
			c=c->vm_next;
			// printk("addr > c->vm_start && c->vm_end > addr\n");
                }
		else{
			p=c;
			c=c->vm_next;
		}

		

	}

	for(u64 i= addr;i<addr+length;i+=PAGE_SIZE){
		free_pfns(current,i);
	}
	FLUSH();
    return val;
}

void free_pfns(struct exec_context *current, u64 addr){
        u64 pgd=(u64)osmap(current->pgd);
	u64 pgd_off= addr>>39 & 0x1FF;
	u64 pgdt=pgd+pgd_off*8;
	u64 pgd_t = *((u64 *)pgdt);
        if((pgd_t & 0x1)==0) return;
        u64 pud_=pgd_t>>12;
	u64 pud=pud_<<12;
	u64 pud_off= addr>>30 & 0x1FF;
        u64 pudt=pud+pud_off*8;
        u64 pud_t = *((u64 *)pudt);
	if((pud_t & 0x1)==0) return;
	u64 pmd_=pud_t>>12;
        u64 pmd=pmd_<<12;
        u64 pmd_off= addr>>21 & 0x1FF;
        u64 pmdt=pmd+pmd_off*8;
        u64 pmd_t = *((u64 *)pmdt);
        if((pmd_t & 0x1)==0) return;
	u64 pte_=pmd_t>>12;
        u64 pte=pte_<<12;
        u64 pte_off= addr>>12 & 0x1FF;
        u64 ptet=pte+pte_off*8;
        u64 pte_t = *((u64 *)ptet);
        if((pte_t & 0x1)==0) return;
	pte_t = pte_t - 1;
	u64 pt_=pte_t>>12;
        u64 pt=pt_<<12;
	os_pfn_free(USER_REG,pt);
	*((u64 *)ptet)=pte_t;
}
/**
 * Function will invoked whenever there is page fault for an address in the vm area region
 * created using mmap
 */

long vm_area_pagefault(struct exec_context *current, u64 addr, int error_code)
{
	int val=-1;
	struct vm_area *c = current->vm_area->vm_next;
	while(c!=NULL){
		if(addr >= c->vm_start && addr < c->vm_end) break;
		c=c->vm_next;
	}
	if(c==NULL) return -1;
	u64 pgd=(u64)osmap(current->pgd);
	if(error_code==0x7) {
		if(c->access_flags == PROT_READ) return -1;
		return handle_cow_fault(current,addr,c->access_flags);
	}
	if(error_code == 0x6 && c->access_flags==PROT_READ) return -1; 
	else {	
		u64 pgd_off= (addr>>39 & 0x1FF);
		
		u64 pfn_t= pgd+pgd_off*8;
		u64 pgd_t = *((u64 *)pfn_t);
		//printk("fault write3 %d\n",pgd_t);
		if(((pgd_t & 0x8)>>3)==0) {
		//	printk("nikal\n");
			return -1;
		}
		//printk("fault write4\n");
		if((pgd_t & 0x1)==0){
		//	printk("fault write6\n");
			u64 pfn_g = os_pfn_alloc(OS_PT_REG);
			pgd_t = 0x19;
			pgd_t |= pfn_g << 12;
			*((u64*)pfn_t) = pgd_t;
		}
		//printk("fault write5\n");
		u64 pgd_ = pgd_t>>12;
		//printk("fault write7\n");
		u64 pud = pgd_<<12;
		//printk("fault write8\n");
		u64 pud_off = (addr>>30 & 0x1FF);
		//printk("fault write9\n");
		u64 pudt = pud+pud_off*8;
		//printk("fault write10\n");
		u64 pud_t=*((u64 *)pudt);
		//printk("fault write11\n");
		if((pud_t & 0x1)==0){
		//	printk("fault write12\n");
                        u64 pfn_u=(u64)os_pfn_alloc(OS_PT_REG);
                        pud_t= 0x19;
			pud_t |= pfn_u << 12;
                        *((u64 *)pudt)=pud_t;
                }
		//printk("fault write13\n");
    		u64 pud_ = pud_t>>12;
		//printk("fault write14\n");
                u64 pmd=pud_<<12;
		//printk("fault write15\n");
                u64 pmd_off=(addr>>21 & 0x1FF);
		//printk("fault write16\n");
                u64 pmdt=pmd+pmd_off*8;
		//printk("fault write17\n");
                u64 pmd_t=*((u64 *)pmdt);
		//printk("fault write18\n");
                if((pmd_t & 0x1)==0){
		//	printk("fault5 write1\n");
                        u64 pfn_m=(u64)os_pfn_alloc(OS_PT_REG);
                        pmd_t= 0x19;
			pmd_t |= pfn_m << 12;
                        *((u64 *)pmdt)=pmd_t;
                }
		//printk("fault write19\n");
		u64 pte_ = pmd_t>>12;
		//printk("fault write21\n");
                u64 pte=pte_<<12;
		//printk("fault write22\n");
                u64 pte_off=(addr>>12 & 0x1FF);
		//printk("fault write23\n");
                u64 ptet=pte+pte_off*8;
		//printk("fault write24\n");
                u64 pte_t=*((u64 *)ptet);
		///printk("fault write41\n");
                if((pte_t & 0x1)==0){
		//	printk("fault 4write1\n");
                        u64 pfn_t=(u64)os_pfn_alloc(USER_REG);
                        pte_t = 0x11;
			pte_t |= pfn_t << 12;
			if(c -> access_flags == 3){
				pte_t |= 0x8;
			}
		//	printk("fault 44write1\n");
                        *((u64 *)ptet)=pte_t;
		//	printk("fault 444write1\n");
                }
		//printk("f4ault write1\n");
		return 1;
	}
	return -1;
}

/**
 * cfork system call implemenations
 * The parent returns the pid of child process. The return path of
 * the child process is handled separately through the calls at the 
 * end of this function (e.g., setup_child_context etc.)
 */

u64 user_pte(struct exec_context* current,u64 addr){
	u64 pgd=(u64)osmap(current->pgd);
        u64 pgd_off= addr>>39 & 0x1FF;
        u64 pgdt=pgd+pgd_off*8;
        u64 pgd_t = *((u64 *)pgdt);
        if((pgd_t & 0x1)==0 ) return 0;
        u64 pud_=pgd_t>>12;
        u64 pud=pud_<<12;
        u64 pud_off= addr>>30 & 0x1FF;
        u64 pudt=pud+pud_off*8;
        u64 pud_t = *((u64 *)pudt);
        if((pud_t & 0x1)==0) return 0;
        u64 pmd_=pud_t>>12;
        u64 pmd=pmd_<<12;
        u64 pmd_off= addr>>21 & 0x1FF;
        u64 pmdt=pmd+pmd_off*8;
        u64 pmd_t = *((u64 *)pmdt);
        if((pmd_t & 0x1)==0 ) return 0;
        u64 pte_=pmd_t>>12;
        u64 pte=pte_<<12;
        u64 pte_off= addr>>12 & 0x1FF;
        u64 ptet=pte+pte_off*8;
        u64 pte_t = *((u64 *)ptet);
        if((pte_t & 0x1)==0) return 0;
	return pte_t;
}

void make_pfns(u64 addr, struct exec_context *ctx, struct exec_context *ctx_c){
 u64 pfn;
        u64 cr3 = (u64)osmap(ctx -> pgd);
        u64 pgd_t_offset = addr >> 39;
        u64 pgd_t_address = cr3 + 8 * pgd_t_offset;
        u64 pgd_t_pte = *((u64*)pgd_t_address);
        if((pgd_t_pte & 1) == 0){
            return;
        }
        pfn = (pgd_t_pte >> 12) << 12;
        
        u64 pud_t_offset = (addr >> 30) & (0x1FF);
        u64 pud_t_address = pfn + pud_t_offset * 8; 
        u64 pud_t_pte = *((u64*)pud_t_address);
        if((pud_t_pte & 1) == 0){
            return;
        }
        pfn = (pud_t_pte >> 12) << 12;
        
        u64 pmd_t_offset = (addr >> 21) & (0x1FF);
        u64 pmd_t_address = pfn + 8 * pmd_t_offset;
        u64 pmd_t_pte = *((u64*)pmd_t_address);
        if((pmd_t_pte & 1) == 0){
            return;
        }
        pfn = (pmd_t_pte >> 12) << 12;
        
        u64 pte_t_offset = (addr >> 12) & (0x1FF);
        u64 pte_t_address = pfn + 8 * pte_t_offset;
        u64 pte_t_pte = *((u64*)pte_t_address);
        if((pte_t_pte & 1) == 0){
            return;
        }
         
        
        
        u64 cpfn;
        u64 cr3_c = (u64)osmap(ctx_c -> pgd);
        // checking pgd_t
        u64 pgd_t_offset_c = addr >> 39;
        u64 pgd_t_address_c = cr3_c + 8 * pgd_t_offset;
        u64 pgd_t_pte_c = *((u64*)pgd_t_address_c);
        if((pgd_t_pte_c & 1) == 0){
            u64 pfn = os_pfn_alloc(OS_PT_REG);
            pgd_t_pte_c = 17;
            pgd_t_pte_c |= (pfn << 12);
            pgd_t_pte_c |= (1 << 3);
        }
        *((u64*)pgd_t_address_c) = pgd_t_pte_c;
        cpfn = (pgd_t_pte_c >> 12) << 12;
        // checking pud_t
        u64 pud_t_offset_c = (addr >> 30) & (0x1FF);
        u64 pud_t_address_c = cpfn + pud_t_offset_c * 8; 
        u64 pud_t_pte_c = *((u64*)pud_t_address_c);
        if((pud_t_pte_c & 1) == 0){
            u64 pfn = os_pfn_alloc(OS_PT_REG);
            pud_t_pte_c = 17;
            pud_t_pte_c |= (pfn << 12);
            pud_t_pte_c |= (1 << 3);
        }
        *((u64*)pud_t_address_c) = pud_t_pte_c;
        cpfn = (pud_t_pte_c >> 12) << 12;
        // checking pmd_t
        u64 pmd_t_offset_c = (addr >> 21) & (0x1FF);
        u64 pmd_t_address_c = cpfn + 8 * pmd_t_offset_c;
        u64 pmd_t_pte_c = *((u64*)pmd_t_address_c);
        if((pmd_t_pte_c & 1) == 0){
            u64 pfn = os_pfn_alloc(OS_PT_REG);
            pmd_t_pte_c = 17;
            pmd_t_pte_c |= (pfn << 12);
            pmd_t_pte_c |= (1 << 3);
        }
        *((u64*)pmd_t_address_c) = pmd_t_pte_c;
        cpfn = (pmd_t_pte_c >> 12) << 12;
        // checking pte_t
        u64 pte_t_offset_c = (addr >> 12) & (0x1FF);
        u64 pte_t_address_c = cpfn + 8 * pte_t_offset_c;
        u64 pte_t_pte_c = *((u64*)pte_t_address_c);
        if((pte_t_pte >> 3) & 1){
            pte_t_pte ^= (1 << 3);
        }
        pte_t_pte_c = pte_t_pte;   //Important
        *((u64*)pte_t_address_c) = pte_t_pte_c;
        *((u64*)pte_t_address) = pte_t_pte;
        get_pfn(pte_t_pte >> 12);
        FLUSH();
}




long do_cfork(){
    u32 pid;
    struct exec_context *new_ctx = get_new_ctx();
    struct exec_context *ctx = get_current_ctx();
     /* Do not modify above lines
     * 
     * */   
     /*--------------------- Your code [start]---------------*/
  


      pid = new_ctx -> pid;
      *new_ctx=*ctx;
      new_ctx->pid=pid;
    //new_ctx -> ppid = ctx -> pid;
    //new_ctx -> type = ctx -> type;
    //new_ctx -> state = ctx -> state;
   // new_ctx -> used_mem = ctx -> used_mem;
    new_ctx -> pgd = os_pfn_alloc(OS_PT_REG);

	struct mm_segment * seg;
	seg=&(ctx -> mms[MM_SEG_CODE]);
    for(u64 i = (seg-> start); i < (seg -> next_free); i += 4096){
        if(user_pte(ctx,i))
	    make_pfns(i, ctx, new_ctx);
    }
    seg=&(ctx -> mms[MM_SEG_RODATA]);
    for(u64 i = (seg -> start); i < (seg-> next_free); i += 4096){
        if(user_pte(ctx,i))
	    make_pfns(i, ctx, new_ctx);
    }
    seg=&(ctx -> mms[MM_SEG_DATA]);
    for(u64 i = (seg -> start); i < (seg -> next_free); i += 4096){
        if(user_pte(ctx,i))
		make_pfns(i, ctx, new_ctx);
    }
    seg=&(ctx -> mms[MM_SEG_STACK]);
    for(u64 i = (seg -> start); i < (seg-> end); i += 4096){
        if(user_pte(ctx,i))
	    make_pfns(i, ctx, new_ctx);
    }
 struct vm_area *vm_current = ctx -> vm_area -> vm_next;
    struct vm_area* node = (struct vm_area*)os_alloc(sizeof(struct vm_area));
    node -> vm_start = MMAP_AREA_START;
    node -> vm_end = MMAP_AREA_START + PAGE_SIZE;
    node -> access_flags = 0x0;
    node -> vm_next = NULL;
    new_ctx -> vm_area = node;
    struct vm_area *vm_new = new_ctx -> vm_area;
    while(vm_current != NULL){
        struct vm_area *first = (struct vm_area *)os_alloc(sizeof(struct vm_area));
        first -> vm_start = vm_current -> vm_start;
        first -> vm_end = vm_current -> vm_end;
        first -> access_flags = vm_current -> access_flags;
        for(u64 i = vm_current -> vm_start; i < vm_current -> vm_end; i += 4096){
            make_pfns(i, ctx, new_ctx);
        }
        vm_new -> vm_next = first;
        vm_new = first;
        vm_current = vm_current -> vm_next;
    }
     /*--------------------- Your code [end] ----------------*/
    
     /*
     * The remaining part must not be changed
     */
    copy_os_pts(ctx->pgd, new_ctx->pgd);
    do_file_fork(new_ctx);
    setup_child_context(new_ctx);
    return pid;
}



/* Cow fault handling, for the entire user address space
 * For address belonging to memory segments (i.e., stack, data) 
 * it is called when there is a CoW violation in these areas. 
 *
 * For vm areas, your fault handler 'vm_area_pagefault'
 * should invoke this function
 * */

long handle_cow_fault(struct exec_context *current, u64 addr, int access_flags)
{

        u64 pfn;
        u64 cr3 = (u64)osmap(current -> pgd);
        // checking pgd_t
        u64 pgd_t_offset = addr >> 39;
        u64 pgd_t_address = cr3 + 8 * pgd_t_offset;
        u64 pgd_t_pte = *((u64*)pgd_t_address);
        pfn = (pgd_t_pte >> 12) << 12;
        // checking pud_t
        u64 pud_t_offset = (addr >> 30) & (0x1FF);
        u64 pud_t_address = pfn + pud_t_offset * 8;
        u64 pud_t_pte = *((u64*)pud_t_address);
        pfn = (pud_t_pte >> 12) << 12;
        // checking pmd_t
        u64 pmd_t_offset = (addr >> 21) & (0x1FF);
        u64 pmd_t_address = pfn + 8 * pmd_t_offset;
        u64 pmd_t_pte = *((u64*)pmd_t_address);
        pfn = (pmd_t_pte >> 12) << 12;
        // checking pte_t
        u64 pte_t_offset = (addr >> 12) & (0x1FF);
        u64 pte_t_address = pfn + 8 * pte_t_offset;
        u64 pte_t_pte = *((u64*)pte_t_address);
        pfn = ((pte_t_pte >> 12) << 12);
        if((get_pfn_refcount(pfn >> 12)) > 1){
            put_pfn(pfn >> 12);
            u64 pfn_ = os_pfn_alloc(USER_REG);
            pte_t_pte = 17;
            pte_t_pte |= (pfn_ << 12);
            if(access_flags == 3){
                if(!((pte_t_pte >> 3) & 1)){
                    pte_t_pte ^= (1 << 3);
                }
            }
            else if(access_flags == PROT_READ){
                if((pte_t_pte >> 3) & 1){
                    pte_t_pte ^= (1 << 3);
                }
            }
            memcpy((void*)(pfn_ << 12),(void*)pfn, 4096);
        }
        else{
            if(access_flags == 3){
                if(!((pte_t_pte >> 3) & 1)){
                    pte_t_pte ^= (1 << 3);
                }
            }
            else if(access_flags == PROT_READ){
                if((pte_t_pte >> 3) & 1){
                    pte_t_pte ^= (1 << 3);
                }
            }
        }
        *((u64*)pte_t_address) = pte_t_pte;
        FLUSH();
        return 1;
}


