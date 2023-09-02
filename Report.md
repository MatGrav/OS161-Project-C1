# PdS: Project C1 - Virtual Memory with Demand Paging
## Variant C1.2 - Inverted Page Table

Progetto svolto per l'insegnamento di Programmazione di sistema AA 2022/23  
Il gruppo è formato da:  
- s319634 Matteo Gravagnone  
- s318083 Danilo Guglielmi

La variante scelta del progetto è la _C1.2 Inverted Page Table_. 

## Obiettivi principali del progetto:
Il progetto ha lo scopo di implementare nuove funzionalità per la gestione della memoria. Nello specifico, l'obiettivo è quello di rimpiazzare il modulo DUMBVM con un nuovo gestore della memoria virtuale basato su paginazione.

Il nuovo gestore della memoria si chiama __NovaVM__, il quale implementa diverse nuove funzionalità per la gestione della memoria utente.

Implementazione in OS161 di:
- Paginazione su richiesta
- Gestione della TLB con politica di sostituzione
- Sostituzione delle pagine (con aggiunta di swapfile)
- Statistiche relative a TLB, Page Faults e Swapfile.

## Suddivisione e metodi di lavoro
Inizialmente, si è analizzato per diversi giorni il codice di DUMBVM, per capire come funzionasse nello specifico. Successivamente, sonos stati elaborati uno schema logico, affinché fossero garantite le nuove specifiche per NovaVM, e un'approssimata time table.
Solo in una seconda fase, è iniziata la scrittura del codice.

Un repository su GitHub è stato utilizzato per tenere traccia di tutti i cambiamenti ed introduzioni nel codice.  

Ciò ha permesso ai membri del gruppo di lavorare in linea generale su computer separati e in autonomia, dividendosi equamente il lavoro, ma ciò non ha impedito una collaborazione costruttiva tra i due. 

D'altronde, la natura interconnessa del codice ha richiesto ai membri del gruppo di confrontarsi frequentemente sui lavori da farsi, per essere sempre aggiornati sulle funzioni implementate e come esse comunichino con altri nuovi moduli di OS161.
[Link per stesura di file .md](https://chat.openai.com/share/b8d52ceb-4b52-4795-b3ed-1d0a377be42b)  

## COREMAP
Il primo modulo implementato dal gruppo è relativo alla coremap, ovvero una struttura dati che consente il tracciamento dei frame liberi (e occupati) in memoria.  

Questa scelta ci consente di fornire uno "strato intermedio" tra l'hardware e **continuare**  

La coremap è vista come una struttura dati che ha all'interno: un array di struct di entry generica, la size della coremap, un vettore di pagine libere e la sua size.  

L'entry generica della coremap è composta così: l'indirizzo fisico della pagina, un vettore di puntatori (**) di struct addrspace (attenzione: forse va modificata la struttura?),
il numero di addrspace (inteso come: in quanti address space è mappata la pagina), un intero (o bool) che indica se la pagina è occupata o libera.  

Dunque, bisogna cambiare le funzioni che allocano pagine di memoria, in quanto bisognerà far riferimento a questa struttura coremap che, a sua volta, marcherà i frame liberi o occupati e popolerà i suoi campi. Le funzioni da modificare: getppages e free_kpages che, in teoria, non usano più la ram_stealmem perché quest'ultima prende a prescindere memoria fisica senza controllare effettivamente se lo spazio è libero o occupato (che ruolo ha quindi la ram_stealmem? -> vedi avanti)  


In _coremap.c_ ci sono funzioni per l'allocazione di pagine sia kernel che utente.  
Infatti, in OS161, sono utilizzate funzioni quali _alloc_kpages()_ e _as_prepare_load()_ (presenti in dumbvm e scritte in coremap.c) che sfruttano, a loro volta, _getppages()_  
Nel laboratorio 2 del corso di PdS, sono state fornite anche funzioni di liberazione di pagine/frames.  

Prima che la coremap venga attivata, è necessario poter allocare per conto kernel nonostante l'assenza della stessa. A tal scopo, sulla linea di dumbvm, è utilizzata la _ram_stealmem()_.  
Da qui in poi, bisogna utilizzare le funzioni da definire in coremap.c per una giusta allocazione (free/allocated).  

**Nel nostro caso, non possiamo fare così. Dobbiamo, qualora non ci fosse più spazio disponibile (ergo, non ci sono sufficienti frame marcati come liberi), chiamare  un
algoritmo di sostituzione delle pagine, secondo una politica di sostituzione da definire in seguito (second chance, ecc).**  

coremap.c:
- [x] coremap_init inizializza le struct (la coremap static?)
- [ ] coremap

## Nuovo gestore memoria virtuale
Come detto in precedenza, è stato aggiunto un nuovo gestore di memoria virtuale, NovaVM, che sostituisce DUMBVM.
Il gestore DUMBVM unisce, in un unico file sorgente _dumbvm.c_, funzioni di allocazione/deallocazione. Noi, invece, abbiamo diviso cià in più moduli: in _novavm.c_, c'è il cuore della gestione della memoria: ___vm_fault()___.
Nel nostro novavm.c la funzione principale è quest'ultima, in quanto si è scelto di astrarre addrspace e pagetable, che prima erano inclusi in DUMBVM, in moduli diversi.
```
int vm_fault(int faulttype, vaddr_t faultaddress){
  /* ... */
  switch (faulttype) {
	    case VM_FAULT_READONLY:
		{
			return EACCES;
		}
  /* ... */
  pid_t pid = proc_getpid();
	paddr = ipt_translate(pid, faultaddress);
  /* ... */
  /* At this point dumbvm, running out of entries, couldn't handle pf
	returning EFAULT*/
	ehi = faultaddress;
	elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
	tlb_write(ehi,elo,tlb_get_rr_victim());
	vmstats_increase_2(TLB_FAULTS,TLB_FAULTS_REPLACE);
	splx(spl);
	return 0;
}
```
Nei frammenti di codice riportato si evidenziano le soluzioni di due requisiti:

1 - IL progetto richiede che un tentativo di scrittura in un'area read only, come il segmento codice per un dato programma user, causi la terminazione del processo.
A tal scopo, giunti nella vm_fault, è sufficiente ritornare un valore di errore (già definito in OS161): in questo modo si ritorna alla mips_trap(), che esegue il seguente codice
```
if (!iskern) {
		/* Fatal fault in user mode. Kill the current user process. */
		kill_curthread(tf->tf_epc, code, tf->tf_vaddr);
		goto done;
	}
```
La kill_curthread è stata modificata in modo da terminare il processo user, attraverso una chiamata sys__exit. 
  
2- L'introduzione della inverted page table richiede che l'indirizzo fisico da inserire in una entry della TLB sia ottenuto attraverso la IPT, e, a differenza di dumbvm, è presente una politica round-robin per sostituire le entry nella TLB quando questa è piena.
Ciò consente, ad esempio, l'esecuzione di test user quali **huge**

_N.B. Abbiamo modificato la dimensione della RAM in os161/root/sys161.conf portandola da 512K a 8M, in questo modo abbiamo più spazio per allocare._

## Address Space
In dumbvm, sia pre che post laboratorio 2, ci sono le (ri)definizioni delle funzioni relative all'addrspace, dichiarate in kern/include/addrspace.h. Abbiamo pensato che queste vadano implementate in novavm.c.
Tuttavia, l'implementazione subirà delle modifiche perché presumiamo che la struct addrspace venga modificata (in addrspace.h) per tener conto della non-contiguità dei segmenti codice, data e stack. 

## (Inverted) Page Table
La scelta finale è ricaduta su una IPT, pertanto unica a livello di sistema e di dimensione pari al numero di frame.  
La entry generica della IPT memorizza l'identificativo del processo, l' indirizzo virtuale (allineato a pagina), stato e protezione.  
Si nota che, per semplicità, si è scelto di memorizzare indirizzo virtuale, stato e protezione anzichè numero di pagina virtuale, bit di stato e bits di protezione.  

Tra le funzioni principali, ci sono
```
paddr_t ipt_translate(pid_t pid, vaddr_t v){
    /* Alignment of virtual address to page */
    v &= PAGE_FRAME;

    if (v >= USERSTACK){ ipt_fault(INVALID_MAP);    }

    /* Search of the virtual address inside the IPT */
    bool found = false;
    i = ipt_search(pid, v, &found);

    /* frame number */
    paddr_t p;

    if (found){ p = PAGE_SIZE * i; }
    else{
        p = ipt_fault(NOT_MAPPED);
        ipt_map(pid, v, p);
    }return p;
}
```




La logica con cui si accede al corrispondente valore fisico di un indirizzo è il seguente:
ipt_map() -> pongo in ingresso il fisico DELLA PAGINA (non di un indirizzo specifico) e il virtuale, dal virtuale dividendo per PAGE_SIZE ottengo l'indice di pagina (il cosiddetto page number) che sfrutto come indice del vettore in cui inserire il corrispondente valore fisico
ipt_translate()-> una volta inseriti i valori fisici, per tradurre un indirizzo virtuale basterà semplicemente ottenere l'indice (sempre dividendo per PAGE_SIZE) e accedere all'i-esima posizione del vettore all'interno della struct pagetable. Questo valore però è l'indirizzo del frame, a cui bisogna aggiungere il displacement per ottenere l'indirizzo fisico specifico corrispondente. Per fare ciò, si utilizza una maschera chiamata DISPLACEMENT_MASK inizializzata a 0xFFF, corrispondente agli ultimi 12 bit. In questo modo, se faccio una OR con l'indirizzo VIRTUALE specifico, ottengo il diplacement specifico. Dunque, facendo una OR di questo displacement appena trovato con l'indirizzo fisico del frame p, ottengo frame+displacement, ossia la traduzione fisica dell'indirizzo virtuale.

Poiché la paginazione riguarda, nel nostro caso, solo lo spazio user che è diviso in segmenti, allora l'unità elementare che bisogna allocare è un singolo segmento, che può essere distribuito anche su più pagine.
Dunque, c'è bisogno di una funzione in segment.c (vedi avanti) oppure dove è definita la load_elf che chiama una funzione nella page table per l'allocazione.
Ricordiamoci che l'allocazione effettiva la fa la VOP_READ
In particolare, poiché noi stiamo ancora usando la load_segment (volendo potremmo implementare una nuova funzione), il flow è il seguente per l'allocazione utente:

load_elf -> load_segment -> ipt_map() -> VOP_READ
(quindi questo per l'aggiornamento della pt)

Quando invece chiamiamo la ipt_translate per avere una traduzione di un indirizzo virtuale e NON c'è una corrispondenza, allora significa che il frame fisico richiesto NON è stato ancora caricato in memoria. Bisogna prima quindi creare uno spazio con as_create, poi chiamare la load_segment per caricare l'effettivo segmento in memoria attingendo dal file elf e aggiornare la tabella delle pagine (e la TLB).
!!!!!! In questo modo, stiamo nella pratica implementando l'on-demand page loading!

### Algoritmi di sostituzione delle pagine
La page table contiene tutti gli indirizzi fisici degli indirizzi virtuali MAPPATI. Un indirizzo virtuale inoltre può essere non mappato in memoria, per i motivi scritti in seguito.
Se la page table teoricamente riesce a coprire qualsiasi traduzione logico-fisica degli indirizzi, allora a cosa servono gli algoritmi di sostituzione delle pagine? Servono per la MEMORIA FISICA, che è limitata e non può contenere tutti i frame possibili di un processo. Dunque, riguardano il caso in cui la memoria virtuale di un processo è superiore a quella fisica. Ogni frame in memoria fisica ha una sua mappatura in page table, dunque non appena un frame viene rimosso o aggiunto, bisogna correttamente aggiornare anche la tabella delle pagine.

## SWAPFILE
Il progetto ci richiede di scrivere su un file chiamato SWAPFILE le pagine che vanno scritte in memoria. Creiamo dunque dei file di supporto, swapfile.h e swapfile.c, nei quali scriviamo il codice per implementare tale logica.
Per quanto riguarda la lettura e scrittura, anziché usare le classiche fread e fwrite, che sarebbero andate anche bene e che avrebbero garantito una maggiore portabilità (almeno di questo modulo), sfruttiamo le tipiche funzioni forniteci dall'interfaccia di OS161: VOP_READ e VOP_WRITE. Queste due funzioni (che tra l'altro possono chiamare una read o write virtuali o non, in base alla configurazione) lavorano su una struct vnode (FCB) che ho definito come variabile globale.
Lo swapfile dunque viene inizializzato nella swap_init con vfs_open che, nel modo in cui è stato scritto, ci dà i permessi di lettura e scrittura sul file, lo apre e grazie a O_CREAT se non è stato ancora creato lo crea.

Nelle funzioni swap_in (da cambiare forse in swap_push) e swap_out bisogna inizializzare le due classiche struct iovec e uio.
Successivamente, si fa la chiamata a VOP_WRITE o VOP_READ per la scrittura o lettura sul file.

Dopo una swap_out, allora significa che l'iesimo indirizzo scritto sullo swapfile non è più presente nello swapfile, è stato consumato. Anziché fare un'ulteriore VOP_WRITE sugli stessi bit, e quindi consumare un'ulteriore operazione di I/O, ho deciso di utilizzare una semplice bitmap, in cui l'indice dell'array indica se lo stesso indice per lo swapfile è valido oppure no, ossia se contiene un indirizzo valido oppure consumato e quindi non più valido.
Sfrutto due macro definite in swapfile.h che sono SF_ABSENT=0 e SF_PRESENT=1;

Nelle operazioni di lettura e scrittura, visto che stiamo usando anche una bitmap, ho introdotto uno spinlock per essere sicuri di stare leggendo una entry valida o non valida, per non avere race conditions.

La logica è la seguente:
Viene chiamata la ipt_fault con faulttype==NOT_MAPPED;
La prima cosa che si fa è verificare se c'è spazio libero con p=alloc_upage();
-> Se p!=0 allora c'è spazio libero e si alloca normalmente, con una conseguente mappatura.
-> Se p==0 allora NON c'è spazio libero, c'è bisogno di sostituire. Si calcola con pt_fifo e la queue_fifo le pagine da eliminare. Poi, una volta liberato lo spazio di quella pagina facendo lo SWAP IN della pagina nello swapfile

Per quanto riguarda la dimensione, che deve essere fissata a 9MB, facciamo ancora uso delle funzioni che ci fornisce l'interfaccia di OS161 e chiamiamo la VOP_TRUNCATE. Di seguito chat gpt ci spiega a cosa serve:

La funzione VOP_TRUNCATE è una funzione definita nell'interfaccia del file system di OS/161. Essa è utilizzata per modificare la dimensione di un file specifico nel sistema di file. In particolare, VOP_TRUNCATE viene utilizzata per ridimensionare un file alla dimensione specificata.

La firma della funzione VOP_TRUNCATE è la seguente:

int VOP_TRUNCATE(struct vnode *v, off_t newlen);

Dove:

    v è un puntatore alla struttura struct vnode che rappresenta il file nel sistema di file.
    newlen è la nuova lunghezza (dimensione) desiderata per il file.

Quando chiami VOP_TRUNCATE, il sistema di file esegue le operazioni necessarie per modificare la dimensione del file in base al valore specificato in newlen. Questo potrebbe comportare il taglio del file o l'estensione del file, a seconda del valore di newlen.

Nel contesto della gestione della memoria virtuale e del file di swap, utilizzare VOP_TRUNCATE per impostare la dimensione massima del file di swap garantisce che il file abbia una dimensione fissa e non superi mai la dimensione desiderata (nel tuo caso, 9 MB).

### Come interagiscono lo swapfile e la queue_fifo? quando faccio push e pop su ognuna delle 2 strutture?
queue_fifo:
Ogni volta che carichi una pagina in memoria fisica (quando avviene un page fault), aggiungi l'indice o l'ID della pagina nella coda FIFO. Questo indica l'ordine di caricamento delle pagine.
Ogni volta che rimuovi una pagina dalla memoria fisica (a causa del page replacement), rimuovi il primo elemento dalla coda FIFO poiché è la pagina più vecchia.

SWAPFILE:
Quando una pagina viene rimossa dalla memoria fisica a causa del page replacement, devi scrivere questa pagina nel file di swap prima di liberare la sua posizione in memoria. Questo assicura che la pagina sia salvata in modo persistente e possa essere recuperata successivamente.
Quando si verifica un page fault per una pagina non in memoria fisica, dovresti controllare prima se la pagina è presente nel file di swap. Se lo è, caricala dal file di swap nella posizione corretta in memoria fisica.

## SEGMENTI
L'ELF header e il program header (PHDR) sono entrambi elementi fondamentali dei file ELF (Executable and Linkable Format), ma svolgono ruoli diversi all'interno di un file ELF.

ELF Header (ELF Header File):
L'ELF header (intestazione ELF) è la prima parte di un file ELF e contiene informazioni generali e di alto livello sul file stesso. Queste informazioni includono:

-> Il tipo di file ELF (eseguibile, condiviso, oggetto, ecc.).
-> L'architettura di destinazione (es. x86, ARM, MIPS, ecc.).
-> La versione dell'ELF.
-> L'indirizzo di punto di ingresso (entry point) del programma, cioè dove inizia l'esecuzione.
-> Offset in cui inizia la tabella dei program headers (PHDRs).
-> Offset in cui inizia la tabella delle section headers (SHDRs).
-> Altri dettagli di gestione.
In sostanza, l'ELF header fornisce informazioni di alto livello sull'organizzazione del file ELF e su come dovrebbe essere trattato e caricato dal sistema operativo o dal linker.

Program Header (PHDR):
I program headers (PHDRs) sono un tipo di entry nella tabella dei program headers all'interno del file ELF. Ogni entry PHDR definisce un segmento di memoria e fornisce informazioni su come quel segmento dovrebbe essere caricato in memoria durante l'esecuzione. Queste informazioni includono:

-> Il tipo di segmento (caricabile, dinamico, ecc.).
-> L'offset all'interno del file ELF da cui inizia il segmento.
-> L'indirizzo virtuale a cui il segmento dovrebbe essere mappato in memoria.
-> Le dimensioni del segmento nel file e in memoria.
-> Autorizzazioni di accesso (lettura, scrittura, esecuzione).
-> Altre informazioni specifiche al segmento.
In sintesi, mentre l'ELF header contiene informazioni di alto livello sul file ELF nel suo complesso, i program headers forniscono dettagli specifici su come le diverse parti del file dovrebbero essere caricate in memoria durante l'esecuzione. I PHDRs guidano il processo di caricamento e mappatura delle sezioni del file ELF nella memoria virtuale quando il programma viene eseguito.

https://chat.openai.com/share/3393f322-c9cf-4b4b-8d6d-8a38ffb470a0

In os161 non abbiamo una struct segment (forse) perché allochiamo tutto in un'unica botta e l'indirizzo è unico, con tutti gli altri contigui i think.
Nel nostro caso, potremmo avere una cosa del genere
```struct segment {
    vaddr_t vaddr;        // Indirizzo virtuale del segmento
    size_t memsize;       // Dimensione in memoria virtuale del segmento
    size_t filesize;      // Dimensione effettiva del segmento nel file ELF
    bool is_loaded;       // Flag per indicare se il segmento è stato caricato in memoria
    // Altre informazioni specifiche del segmento, se necessario
};
```

memsize rappresenta lo spazio riservato nello spazio di indirizzamento del processo, mentre filesize rappresenta la quantità di dati effettivamente archiviata nel file ELF per quel segmento. "filesize" può essere più piccolo, uguale o persino più grande di "memsize".

Esempio pratico per capirne la differenza al seguente link:
https://chat.openai.com/share/3393f322-c9cf-4b4b-8d6d-8a38ffb470a0/continue

## TLB

In os161 è già presente una TLB con 64 entries (kern/arch/mips/include/tlb.h). Il meccanismo della traduzione da indirizzo virtuale a logico che passa tramite la ricerca nella TLB NON va implementato da noi, essendo già presente (presumibilmente in toolbuild/sys161-2.0.8/mipseb/mips.c in cui un'architettura MIPS e una MMU? vengono simulate), per cui il flow è:


indirizzo virtuale --> TLB look up  -->  NO TLB HIT? -->   [HW sets BADVADDR,raises exception; OS161 sees VM_FAULT_READ or VM_FAULT_WRITE ] ==> gestiamo la vm_fault all'interno della quale chiediamo alla tabella delle pagine l'indirizzo fisico così da aggiungere/sostituire una entry nella TLB

LA vm_fault viene chiamata in mips_trap(..) in 'os161-base-2.0.3/kern/arch/mips/locore/trap.c' che controlla il codice di "eccezione", codici relativi a IRQ e syscall sono gestiti rispettivamente  da mainbus_interrupt() e syscall(), altrimenti chiama la vm_fault per codici relativi a eccezioni TLB, che sono VM_FAULT_READONLY/READ/WRITE.
 
Nel caso VM_FAULT_READONLY.Il progetto prevede che non si possa scrivere sul segmento di testo, pertanto è possibile che si arrivi a questo codice, che ha il compito di uccidere il processo user corrente. Se la vm_fault ritorna EACCES (definito in kern/errno per permission denied), ritorneremo alla mips_trap() che arriverà alla chiamata a kill_curthread(..)  che avrà l'obiettivo di uccidere il processo user corrente. Pertanto modifichiamo quest'ultima mettendo una sys__exit nel caso EX_MOD.
Nei casi di VM_FAULT READ e WRITE, la vm_fault() prosegue normalmente, come in dumbvm, ma chiede il paddr alla page table, la quale si occuperà di gestire l'eventuale assenza in RAM di quanto richiesto (?).

programma per testare la sostituzione nella tlb? (p testbin/huge)


La TLB comunica con la page table principalmente nei seguenti casi:

-> TLB Miss: Quando la CPU cerca di accedere a un indirizzo virtuale e non trova la corrispondente traduzione nell'TLB (questo è noto come TLB miss), la CPU deve consultare la page table per ottenere la traduzione corretta dell'indirizzo virtuale in un indirizzo fisico.

-> TLB Invalidation: La TLB deve essere aggiornata o invalidata quando ci sono cambiamenti nel mapping tra indirizzi virtuali e indirizzi fisici. Questo può accadere a seguito di operazioni come la creazione o la terminazione di processi, l'allocazione o la deallocazione di pagine di memoria, o la modifica dei permessi di accesso. In questi casi, la TLB deve essere aggiornata per riflettere gli aggiornamenti nella page table.

-> Context Switch: Quando avviene un cambio di contesto tra due processi, il contenuto della TLB deve essere invalidato o sostituito con le nuove traduzioni della page table del nuovo processo. Questo è necessario perché il mapping tra gli indirizzi virtuali e gli indirizzi fisici è diverso per ciascun processo.


## TO DO

Da fare:
1- Controllare tutti i commenti TO DO nel codice
1.1- Rimuovere tutte le include commentate

5- Controllare rem_head() e rem_qualcosaltro in coremap.c
6- Gestire i permessi di ogni pagina o segmento (davvero ci servono?)
7- Mettere gli #if OPT_NOVAVM
8- controllare se funziona lo swapfile (se scrive le pagine e le toglie)
9- Funzionerebbe con 4MB di RAM ma l'introduzione dei file di lab5 richiedono il passaggio a 8MB
DANILO:
10- Rivedere page table (cose)
11- Swapfile di nuovo porcoddue(non ho voglia di rivedere stammerda)
12- Come funziona la queue_fifo
13- Hashing per ricerca nella pt_search
13.5- Aggiungere KASSERT dove vanno

14- Rivedere le stats che ancora non funzionano
