#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <round.h>
#include <list.h>
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"

/* hash table 초기화*/
void init_virtual(struct hash *virtual)
{
   
   hash_init(virtual, hash_func, compare_hash, NULL);
}

/* hash값을 구하는 함수*/
static unsigned hash_func(const struct hash_elem *e, void *aux)
{
    // virtual_entry 구조체 탐색
    struct virtual_entry *ve = hash_entry(e, struct virtual_entry, elem);
    // vaddr에 대한 hash값 반환
    return hash_int(ve->vaddr);
}

/* hash element들의 크기 비교*/
static bool compare_hash(const struct hash_elem *a, const struct hash_elem *b,void *aux)
{
    // a의 vaddr이 더 작으면 false, 아니면 true 반환
    struct virtual_entry *vir_a = hash_entry(a, struct virtual_entry, elem);
    struct virtual_entry *vir_b = hash_entry(b, struct virtual_entry, elem);

    return vir_a->vaddr < vir_b->vaddr;
}

/*hash table에 virtual entry 삽입*/
bool insert_hash(struct hash *virtual, struct virtual_entry *ve)
{
    ve->locked = false;
    //printf("insert:%d\n", ve->offset);
    // hash_insert는 hash table에 값이 존재하면 그대로 값 반환, 없으면 삽입 후 NULL반환
    if (hash_insert(virtual, &ve->elem) == NULL)
    {
        //printf("success!\n");
        return true;
    }
    return false;
}

/*hash table에서 해당되는 virtual entry 삭제*/
bool delete_hash(struct hash *virtual, struct virtual_entry *ve)
{

    // hash_delete는 hash table에 값이 존재하면 삭제후 삭제된 값 반환, 없으면 NULL반환
    if (hash_delete(virtual, &ve->elem) == NULL)
    {
        return false;
    }
    free_page(pagedir_get_page(thread_current()->pagedir, ve->vaddr));
    free(ve);
    return true;
}

// vaddr에 해당하는 virtual entry 탐색 후 반환
struct virtual_entry *search_ve(void *vaddr)
{

    struct virtual_entry ve;
    // vaddr는 pointer, vaddr에 값을 대입하면 전체 구조체도 동일하게 그 vaddr의 구조체 할당
    ve.vaddr = pg_round_down(vaddr);
    //printf("search:%d\n", ve.vaddr);
    // hash_find 함수는 hash에 일치하는 elem으로 이루어진 hash 탐색
    struct hash_elem *elems = hash_find(&thread_current()->virtual, &ve.elem);

    // 존재하면 그 hash의 entry 반환 아니면 NULL 반환
    if (elems == NULL){
        
        return NULL;
    }
    else{
        return hash_entry(elems, struct virtual_entry, elem);
    }
}

// hash table제거
void kill_hash(struct hash *ve)
{

    // hash_destroy함수로 해당 hash_table을 제거한다.
    hash_destroy(ve, kill_func);
}

// hash_destroy 함수를 위한 kill_func
static void kill_func(struct hash_elem *e, void *aux)
{

    struct virtual_entry *ve = hash_entry(e, struct virtual_entry, elem);

    // physical memory에 load되어 있으면
    if (ve->phy_loaded)
    {
        // page 제거
        void *paddr = pagedir_get_page(thread_current()->pagedir, ve->vaddr);
        free_page(paddr);
        pagedir_clear_page(thread_current()->pagedir, ve->vaddr);
    }
    free(ve);
}

// fault의 주소 확인 및 stack growth확인
struct virtual_entry *check_and_growth(void *addr, void *esp)
{
    
    // user stack영역인지 확인
    if (addr < (void*)0x8048000 || addr >= (void*)0xc0000000)
    {
        //printf("not user stack\n");
        exit(-1);
    }
    // virtual memory에 있는지 확인
    struct virtual_entry *ve = search_ve(addr);
    //printf("search done\n");
    if (ve != NULL)
    {
        //존재하면 return, no need to grow
        //printf("ve!=null\n");
        return ve;
    }
    //printf("error\n");
    // stack growth 체크
    bool growth_check = check_addr(addr,esp);
    //printf("growth check done with %d\n", growth_check);
    if (growth_check)
    {
        //printf("success growth check\n");
        bool growth_result = stack_growth(addr);
        //printf("growth done with %d\n", growth_result);
        /*if (growth_result == false)
        {
            // stack growth 실패
            exit(-1);
        }*/
        
        ve = search_ve(addr);
    }
    else
    {
        //printf("fail\n");
        exit(-1);
    }
    return ve;
}

bool check_addr(void *addr, void *esp){
    
    return (is_user_vaddr(pg_round_down(addr)) && addr>=esp - 32 && addr >= (PHYS_BASE - 8*1024*1024));
}
// disk의 파일을 physical memory로 load
bool load_from_disk(void *addr, struct virtual_entry *ve)
{
    if ((int)(ve->read_bytes) != file_read_at(ve->file, addr, ve->read_bytes, ve->offset))
    {
        //printf("%d %d\n",(int)(ve->read_bytes),file_read_at(ve->file, addr, ve->read_bytes, ve->offset));
        return false;
    }
    else
    {
        memset(addr + ve->read_bytes, 0, ve->zero_bytes);
    }
    return true;
}

    


