#include <segment.h>
#include <lib.h>
#include <types.h> 

struct segment*
segment_create(){
    struct segment* s = kmalloc(sizeof(struct segment));
    segment_init(s);

    return s;
}

void
segment_init(struct segment *s){
    s->vaddr = (vaddr_t) 0;
    s->memsize = (size_t) 0;
    s->filesize = (size_t) 0;
    s->offset = (size_t) 0;
    s->is_loaded = NOT_LOADED;
    s->permission = S_RO;
    s->file_elf = NULL;
    s->as = NULL;
}

int
segment_copy(struct segment *old, struct segment **ret){
struct addrspace *newas;

	newas = segment_create();
	if (newas==NULL) {
		return -1;
	}

	newas->vaddr=old->vaddr;
    newas->memsize=old->memsize;
    newas->npage=old->npage;
    newas->filesize=old->filesize;
    newas->offset=old->offset;
    newas->is_loaded=NOT_LOADED; //LOADED?
    //Have to load?
    newas->permission=old->permission;
    newas->file_elf=old->file_elf;
    newas->as=old->as;

	*ret = newas;
	return 0;
}
