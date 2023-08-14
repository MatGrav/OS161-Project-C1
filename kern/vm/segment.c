#include <segment.h>
#include <lib.h>
#include <types.h> 

void segment_init(struct segment *s){
    s->vaddr = (vaddr_t) 0;
    s->memsize = (size_t) 0;
    s->filesize = (size_t) 0;
    s->offset = (size_t) 0;
    s->is_loaded = NOT_LOADED;
    s->permission = S_RO;
    s->file_elf = NULL;
    s->as = NULL;
}

