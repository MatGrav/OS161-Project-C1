# PdS: Project C1 - Virtual Memory with Demand Paging
## Variant C1.2 - Inverted Page Table

Progetto svolto per l'insegnamento di Programmazione di sistema AA 2022/23  
Il gruppo è formato da:  
- s319634 Matteo Gravagnone  
- s318083 Danilo Guglielmi

La variante scelta del progetto è la _C1.2 Inverted Page Table_. 

## Obiettivi principali del progetto:
Il progetto ha lo scopo di implementare nuove funzionalità per la gestione della memoria. Nello specifico, l'obiettivo è quello di rimpiazzare il modulo DUMBVM con un nuovo gestore della memoria virtuale basato su paginazione.

Il nuovo gestore della memoria si chiama __NovaVM__, il quale implementa diverse nuove funzionalità per la gestione della memoria.

Implementazione in OS161 di:
- Paginazione su richiesta
- Gestione della TLB con politica di sostituzione
- Sostituzione delle pagine (con aggiunta di swapfile)
- Statistiche relative a TLB, Page Faults e Swapfile.

## Suddivisione e metodi di lavoro
Inizialmente, si è analizzato per diversi giorni il codice di DUMBVM, per capire come funzionasse nello specifico. Successivamente, sono stati elaborati uno schema logico, affinché fossero garantite le nuove specifiche per NovaVM, e un'approssimata time table.
Solo in una seconda fase, è iniziata la scrittura del codice.

Un repository su GitHub è stato utilizzato per tenere traccia di tutti i cambiamenti ed introduzioni nel codice. Ciò ha permesso ai membri del gruppo di lavorare in linea generale su computer separati e in autonomia, dividendosi equamente il lavoro. TUttavia, la natura interconnessa del codice ha richiesto ai membri del gruppo di confrontarsi frequentemente sui lavori da farsi, per essere sempre aggiornati sulle funzioni implementate, sulle problematiche comuni e su come esse comunichino con altri nuovi moduli di OS161.

## COREMAP
Il primo modulo implementato dal gruppo è relativo alla coremap, ovvero una struttura dati che consente il tracciamento dei frame liberi (e occupati) in memoria.  
La coremap è vista come una struttura dati che ha all'interno: un array di struct di entry generica, la size della coremap, un vettore di pagine libere e la sua size.  

L'entry generica della coremap è composta così: l'indirizzo fisico della pagina, un vettore di puntatori (**) di struct addrspace (attenzione: forse va modificata la struttura?),
il numero di addrspace (inteso come: in quanti address space è mappata la pagina), un intero (o bool) che indica se la pagina è occupata o libera.  

Dunque, bisogna cambiare le funzioni che allocano pagine di memoria, in quanto bisognerà far riferimento a questa struttura coremap che, a sua volta, marcherà i frame liberi o occupati e popolerà i suoi campi. Le funzioni da modificare: getppages e free_kpages che, in teoria, non usano più la ram_stealmem perché quest'ultima prende a prescindere memoria fisica senza controllare effettivamente se lo spazio è libero o occupato (che ruolo ha quindi la ram_stealmem? -> vedi avanti)  

In _coremap.c_ ci sono funzioni per l'allocazione di pagine sia kernel che utente. Infatti, in OS161, sono utilizzate funzioni quali _alloc_kpages()_ e _as_prepare_load()_ (presenti in dumbvm e scritte in coremap.c) che sfruttano, a loro volta, _getppages()_. In maniera analoga, sono state fornite funzioni per la deallocazione di pagine.  
Prima che la coremap venga attivata, è necessario poter allocare per conto kernel nonostante l'assenza della stessa. A tal scopo, sulla linea di dumbvm, è utilizzata la _ram_stealmem()_.  
Verificata la presenza e l'attivazione della coremap, si procede utilizzando le funzioni fornite in coremap.c per una giusta allocazione (free/allocated).  

## NovaVM: Nuovo gestore memoria virtuale e TLB
Come detto in precedenza, è stato aggiunto un nuovo gestore di memoria virtuale, NovaVM, che sostituisce DUMBVM.
Il gestore DUMBVM unisce, in un unico file sorgente _dumbvm.c_, funzioni di allocazione/deallocazione. Noi, invece, abbiamo scelto di suddividerle in più moduli: in _novavm.c_, c'è il cuore della gestione della memoria: ___vm_fault()___.
In OS161 un supporto basilare per la TLB da 64 entries è già presente: in caso di TLB Miss, viene chiamata la mips_trap() che, con eccezione di tipo VM_FAULT_READONLY / READ / WRITE, chiamata la funzione in esame.
Nel nostro novavm.c la funzione principale è quest'ultima, in quanto si è scelto di astrarre addrspace e pagetable, che prima erano inclusi in DUMBVM, in moduli diversi.
```
int vm_fault(int faulttype, vaddr_t faultaddress){
  /* ... */
  	switch (faulttype) {
	    case VM_FAULT_READONLY:
			return EACCES;
  /* ... */
  	pid_t pid = proc_getpid();
	paddr = ipt_translate(pid, faultaddress);
  /* ... */
  /* At this point dumbvm, running out of entries, couldn't handle pf, therefore returning EFAULT*/
	ehi = faultaddress;
	elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
	tlb_write(ehi,elo,tlb_get_rr_victim());
	vmstats_increase_2(TLB_FAULTS,TLB_FAULTS_REPLACE);
	splx(spl);
	return 0;}
```
Nei frammenti di codice riportato si evidenziano le soluzioni di due requisiti:

1 - Il progetto richiede che un tentativo di scrittura in un'area read only, come il segmento codice per un dato programma user, causi la terminazione del processo.
A tal scopo, giunti nella vm_fault, è sufficiente ritornare un valore di errore (già definito in kern/errno.h): in questo modo si ritorna alla mips_trap(), che esegue il seguente codice
```
if (!iskern) {
		/* Fatal fault in user mode. Kill the current user process. */
		kill_curthread(tf->tf_epc, code, tf->tf_vaddr);
		goto done;
	}
```
La kill_curthread è stata modificata in modo da terminare il processo user, attraverso una chiamata sys__exit. 
  
2- L'introduzione della inverted page table richiede che l'indirizzo fisico da inserire in una entry della TLB sia ottenuto attraverso una ricerca nella IPT, e, a differenza di dumbvm, è presente una politica round-robin per sostituire le entry nella TLB quando questa è piena.
Ciò consente, ad esempio, l'esecuzione di test user quali **huge**.
Infine, si menziona la necessità di invalidare le entries di pid diversi: a tal scopo si veda il paragrafo Addrspace.

_N.B. Abbiamo modificato la dimensione della RAM in os161/root/sys161.conf portandola da 512K a 8M, in questo modo abbiamo più spazio per allocare._


## Address Space
Ogni processo ha un proprio spazio di indirizzamento, che in OS161 può essere visto come l'unione di 3 segmenti: codice, dati e stack. Abbiamo scelto di utilizzare questa rappresentazione, utilizzando la __struct segment__ (vedi paragrafo).

```
	struct segment* code;
        struct segment* data;
        struct segment* stack;
```

A supporto di tale struttura, sono state implementate diverse funzioni: `as_create()` per l'allocazione della struct (che a sua volta chiama la `segment_create()`, che alloca ogni segmento), `as_destroy()` per la deallocazione e distruzione della struct, `as_copy()` per copiarla.
È importante da menzionare anche **`as_activate()`** la quale, facendo il **flush della TLB** (invalida ogni entry), ci permette di gestire al meglio anche i cambi di contesto.
Le funzioni `as_define_region()` - che inizializza uno specifico segmento codice o dati, lo stack viene inizializzato in `as_define_stack()` - e `as_prepare_load()` - che prepara il caricamento dell'address space in memoria chiamando a sua volta la `segment_prepare_load()` - sono chiamate dalla `load_elf()` per il caricamento dell'eseguibile ELF nell'address space corrente.

La `load_elf()` è stata a sua volta modificata, introducendo la nuova logica dei segmenti. Conseguentemente, è stata reimplementata la `load_segment()` la quale comunica con la Inverted Page Table per ottenere gli indirizzi fisici di cui ha bisogno.

## (Inverted) Page Table
La scelta finale è ricaduta su una IPT, pertanto unica a livello di sistema e di dimensione pari al numero di frame (_nRamFrames_).  
La entry generica della IPT memorizza l'identificativo del processo, l' indirizzo virtuale (allineato a pagina), stato (_ABSENT_ o _PRESENT_) e protezione.
Si nota che, per semplicità, si è scelto di memorizzare indirizzo virtuale, stato e protezione anzichè numero di pagina virtuale, bit di stato e bits di protezione.  
```
struct ipt_entry{
    pid_t p_pid; /* process pid */
    vaddr_t page_number;
    uint8_t status; /* Present or absent */
    uint8_t protection; /* read-only, write, read-write*/
};
```  
Oltre alle tipiche funzioni per la creazione e distruzione della page table e delle sue entry, 
ci sono nuove funzioni per la mappatura degli indirizzi, per la traduzione logico-fisica e per la gestione dei page fault.
Nello specifico:
- `ipt_fault()` gestisce i page fault nei casi in cui l'indirizzo sia non valido (_INVALID_MAP_) oppure non mappato (_NOT_MAPPED_)
- `ipt_translate()` per la traduzione degli indirizzi, qualora non ci fosse una traduzione chiamarebbe la `ipt_fault()`
- `ipt_map` per la mappatura calcola l'indice _i_ della tabella delle pagine secondo la logica della IPT, ossia dividendo l'indirizzo fisico allineato con la dimensione di una pagina _PAGE_SIZE_.

## Come velocizzare la ricerca delle pagine nella IPT
La ricerca all'interno della page table è gestita da una funzione statica chiamata `ipt_search()`. Tale funzione, chiamando `ipt_mra` (Most Recently Added), ottiene _BUFF_SIZE_ indici di pagina in cui verifica se l'elemento cercato è presente in tali pagine. Poiché il buffer utilizzato per contenere gli indici li ottiene prelevandoli dalla coda di __queue_fifo__, allora si cercherà l'elemento prima nelle più recenti pagine aggiunte. In una seconda fase, qualora non avesse ancora trovato l'elemento cercato, farà una ricerca lineare all'interno della IPT.

## On-Demand Page Loading
L'On-Demand Page Loading è implementato nella `ipt_translate()`: quando si vuole tradurre un indirizzo, si cerca inizialmente nella tabella delle pagine, grazie a `ipt_search()`. Se l'indirizzo è presente, avviene la traduzione. Se l'indirizzo non è presente, verrà scatenato un `ipt_fault()` nel caso _NOT_MAPPED_, dunque il frame fisico non è stato ancora caricato in memoria. Quest'ultima funzione, quindi, prova ad allocare una pagina con `alloc_upage()`, se trova spazio alloca, altrimenti procede con il page replacement. Alla fine di `ipt_fault()` è stato caricato un frame fisico in memoria, che verrà mappato dalla `ipt_translate()`, proseguendo con l'esecuzione del programma.
Dunque, in questo modo stiamo caricando una pagina solo quando è necessario utilizzarla.

### Algoritmi di sostituzione delle pagine
Quando sono occupati tutti gli slot dei frame fisici nella RAM, verrà adoperato un algoritmo di sostituzione delle pagine per calcolare quella da eliminare. L'algoritmo utilizzato è FIFO e fa uso di una struttura dati chiamata __queue_fifo__: è un array, in cui si scrive in coda e si preleva in testa, nel quale sono scritti gli indici di accesso alla tabella delle pagine (0,1,2,...,_IPT_SIZE_-1). Viene aggiunto un elemento all'array ogni volta che c'è una chiamata a `ipt_map` perché si sta aggiungendo una nuova pagina alla tabella delle pagine, mentre viene eliminato un elemento dall'array ogni volta che c'è un page replacement, scegliendo come vittima la pagina con l'indice alla testa di __queue_fifo()__.

### SWAPFILE
Riprendendo il page replacement, le pagine vittime espulse dalla tabella delle pagine sono scritte in un file chiamato SWAPFILE. Al contrario, esse vanno tolte dallo SWAPFILE quando c'è l'allocazione di una nuova pagina, verificando prima se tale pagina è presente nello SWAPFILE.
Per leggere e scrivere sul file, abbiamo scelto di utilizzare l'interfaccia proposta da OS161 con _VOP_READ_ e _VOP_WRITE_, incapsulate rispettivamente in `swap_in()` e `swap_out()`, mentre per definirne la dimensione _VOP_TRUNCATE_ in `swap_init()`.
A supporto, è stata aggiunta anche una bitmap, per segnare una specifica pagina assente o "consumata" (_SF_ABSENT_) oppure presente (_SF_PRESENT_) nello SWAPFILE.

## Segmenti
Come detto in precedenza, in OS161 ogni programma utente è diviso in segmenti che possono essere di codice, dati o stack. A supporto di tale logica, abbiamo implementato una nuova struttura dati, per il segmento, che è la seguente:
```
struct segment{
    vaddr_t vaddr; /* Virtual address of the segment */
    size_t memsize; /* Segment size reserved for the segment in virtual memory */
    size_t npage; /* Number of pages, useful for paging algorithm */
    size_t filesize; /* Segment size in the ELF file */
	/* flags vari */
    struct vnode* file_elf; // serve per la load_segment
    struct addrspace* as; //puntatore all'address space a cui appartiene, serve alla load_segment
};
```
Le proprietà e il contenuto dei segmenti codice e dati sono scritti nel file ELF, dunque sicuramente andranno caricati in memoria, a un certo punto, quando c'è la chiamata alla `load_elf()`. Lo stack, invece, viene definito in _runprogram.c_ con la chiamata di `as_define_stack()`, nel quale c'è l'inizializzazione di ogni parametro. Ad esempio:
l'indirizzo virtuale è `USERSTACK-(NOVAVM_STACKPAGES*PAGE_SIZE)`, mentre la dimensione del segmento stack è pari a `NOVAVM_STACKPAGES*PAGE_SIZE`.

A supporto della struct segment, sono state create funzioni per la gestione tra cui quelle per la creazione e distruzione, per la copia, e anche la `segment_prepare_load` che ha lo scopo di preparare un segmento al caricamento in memoria.
