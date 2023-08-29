# os161-base-2.0.3

[Link per stesura di file .md](https://chat.openai.com/share/b8d52ceb-4b52-4795-b3ed-1d0a377be42b)

## COREMAP
La coremap è una struttura dati che ci serve a tenere traccia dei frame liberi (e occupati) in memoria. Ricordiamoci che una pagina può essere mappata in più address space.
La coremap è vista come una struttura dati che ha all'interno: un array di struct di entry generica, la size della coremap, un vettore di pagine libere e la sua size.
L'entry generica della coremap è composta così: l'indirizzo fisico della pagina, un vettore di puntatori (**) di struct addrspace (attenzione: forse va modificata la struttura?),
il numero di addrspace (inteso come: in quanti address space è mappata la pagina), un intero (o bool) che indica se la pagina è occupata o libera.

Dunque, bisogna cambiare le funzioni che allocano pagine di memoria, in quanto bisognerà far riferimento a questa struttura coremap che, a sua volta, marcherà i frame liberi o occupati e popolerà i suoi campi. Le funzioni da modificare: getppages e free_kpages che, in teoria, non usano più la ram_stealmem perché quest'ultima prende a prescindere memoria fisica senza controllare effettivamente se lo spazio è libero o occupato (che ruolo ha quindi la ram_stealmem? -> vedi avanti)

Quando allochiamo memoria kernel in OS161, senza la tabella delle pagine, le funzioni che vengono chiamate sono in ordine:
alloc_kpages -> getppages -> ram_stealmem

A livello user:
as_prepare_load -> getppages -> ram_stealmem

Dopo il laboratorio 2, sono state implementate delle soluzioni per liberare delle pagine.
A livello kernel:
kfree -> free_kpages -> freeppages -> freeRamFrames

Prima che la coremap venga attivata, ovviamente deve essere allocata. Come possiamo gestire l'allocazione di memoria prima dunque che venga attivata la coremap?
Si chiama la ram_stealmem che alloca memoria a partire da firstpaddr che, poiché stiamo parlando della primissima allocazione (dopo kernel ecc), prenderà la
primissima porzione di memoria disponibile subito dopo firstpaddr = firstfree - MIPS_KSEG0. Ricordiamo che firstfree, definito in start.S, è sempre lo stesso.
Parliamo dunque del primo valore di firstpaddr, prima di qualsiasi incremento. Dopo l'allocazione di coremap, firstpaddr va incrementato.

Una volta allocata la coremap, viene attivata. Da qui in poi, bisogna utilizzare le funzioni da definire in coremap.c per una giusta allocazione (free/allocated).

Nel laboratorio 2 il professore, qualora non fosse disponibile un intervallo di pagine libere per allocare una determinata porzione di memoria, allora chiamava
ram_stealmem che, a sua volta, prendeva la memoria di cui ha bisogno a partire da firstpaddr, non tenendo conto se le pagine prese fossero libere o occupate. Nel nostro caso, non possiamo fare così. Dobbiamo, qualora non ci fosse più spazio disponibile (ergo, non ci sono sufficienti frame marcati come liberi), chiamare  un
algoritmo di sostituzione delle pagine, secondo una politica di sostituzione da definire in seguito (second chance, ecc).

coremap.c:
- [x] coremap_init inizializza le struct (la coremap static?)
- [ ] coremap

## Nuovo gestore memoria virtuale

È stato aggiunto un nuovo gestore di memoria virtuale che sostituisce dumbvm. Il suo nome, scelto accuratamente dagli inventori, è NovaVM. ~~Il file novavm.c si trova esattamente dove si trovava dumbvm.c: kern/arch/mips/vm~~
Il file è attualmente posizionato in *kern/vm* insieme a tutti gli altri.
~~Esistono anche due "vm.h": ne creiamo uno nuovo appositamente per il nostro gestore, che si chiama novavm.h e si trova in kern/include.~~
Abbiamo modificato la dimensione della RAM in os161/root/sys161.conf portandola da 512K a 8M, in questo modo abbiamo più spazio per allocare.


## Modifiche addrspace
In dumbvm, sia pre che post laboratorio 2, ci sono le (ri)definizioni delle funzioni relative all'addrspace, dichiarate in kern/include/addrspace.h. Abbiamo pensato che queste vadano implementate in novavm.c.
Tuttavia, l'implementazione subirà delle modifiche perché presumiamo che la struct addrspace venga modificata (in addrspace.h) per tener conto della non-contiguità dei segmenti codice, data e stack. 

## PAGE TABLE

Un indirizzo generato dalla CPU è diviso in:
-> page number(p), usato come indice nella tabella delle pagine, che contiene l'indirizzo base di ogni pagina in memoria fisica
-> page offset(d), combinato con l'indirizzo di base definisce l'indirizzo fisico in memoria che è inviato all'unità di memoria

Inoltre, ogni entry della page table ha dei bit caratteristici. In particolare si può far riferimento alla slide 32 del blocco "Chapter 9: Main Memory".
Per semplificazione, anziché dei bit, usiamo dei campi nella pt_entry. Eccoli:
Present/Absent:
Questo campo indica se la pagina virtuale è attualmente presente in memoria fisica o se è assente. Se il bit "present" è impostato, significa che la pagina è stata caricata in memoria fisica e l'indirizzo fisico associato è valido. Se il bit è "absent", potrebbe essere necessario un page fault per caricare la pagina in memoria fisica prima che possa essere accessibile.

Protection:
Questo campo specifica i permessi di accesso alla pagina. I permessi includono la lettura, la scrittura e l'esecuzione. Ad esempio, una pagina potrebbe essere contrassegnata come "read-only" o "writable". Questo controllo dei permessi consente al sistema operativo di implementare la protezione dei dati e il controllo degli accessi.

Modified:
Questo campo indica se la pagina è stata modificata da quando è stata caricata in memoria. Viene spesso chiamato "dirty bit". Questo è utile per l'ottimizzazione: quando il sistema operativo deve liberare un frame fisico, può decidere se scrivere la pagina sulla memoria virtuale o semplicemente eliminarla.

Referenced:
Questo campo indica se la pagina è stata letta o acceduta da quando è stata caricata in memoria. È spesso chiamato "accessed bit". Come il campo "modified", questo bit aiuta nell'ottimizzazione e nella gestione della sostituzione delle pagine.

Caching Disabled:
Questo campo indica se la cache è disabilitata per la pagina. Questo può essere utilizzato per garantire che i dati specifici non siano mantenuti nella cache, ad esempio quando si accede a periferiche di I/O

La page table si deve occupare della traduzione da indirizzo logico a indirizzo fisico. Dunque, il kernel lavorerà allocando in maniera contigua, mentre a livello user si utilizzerà la paginazione. La pagetable è vista come una struct con all'interno un vettore di pagine fisiche e ALTRO DA DEFINIRE.
Bisogna definire (VEDERE COME) il numero di pagine della page table. Come fare? NON è uguale a nRamFrames perché non riguarda tutta la ram, compresa la zona kernel, ma solo lo spazio user.
La logica con cui si accede al corrispondente valore fisico di un indirizzo è il seguente:
pt_map() -> pongo in ingresso il fisico DELLA PAGINA (non di un indirizzo specifico) e il virtuale, dal virtuale dividendo per PAGE_SIZE ottengo l'indice di pagina (il cosiddetto page number) che sfrutto come indice del vettore in cui inserire il corrispondente valore fisico
pt_translate()-> una volta inseriti i valori fisici, per tradurre un indirizzo virtuale basterà semplicemente ottenere l'indice (sempre dividendo per PAGE_SIZE) e accedere all'i-esima posizione del vettore all'interno della struct pagetable. Questo valore però è l'indirizzo del frame, a cui bisogna aggiungere il displacement per ottenere l'indirizzo fisico specifico corrispondente. Per fare ciò, si utilizza una maschera chiamata DISPLACEMENT_MASK inizializzata a 0xFFF, corrispondente agli ultimi 12 bit. In questo modo, se faccio una OR con l'indirizzo VIRTUALE specifico, ottengo il diplacement specifico. Dunque, facendo una OR di questo displacement appena trovato con l'indirizzo fisico del frame p, ottengo frame+displacement, ossia la traduzione fisica dell'indirizzo virtuale.

Poiché la paginazione riguarda, nel nostro caso, solo lo spazio user che è diviso in segmenti, allora l'unità elementare che bisogna allocare è un singolo segmento, che può essere distribuito anche su più pagine.
Dunque, c'è bisogno di una funzione in segment.c (vedi avanti) oppure dove è definita la load_elf che chiama una funzione nella page table per l'allocazione.
Ricordiamoci che l'allocazione effettiva la fa la VOP_READ
In particolare, poiché noi stiamo ancora usando la load_segment (volendo potremmo implementare una nuova funzione), il flow è il seguente per l'allocazione utente:

load_elf -> load_segment -> pt_map() -> VOP_READ
(quindi questo per l'aggiornamento della pt)

Quando invece chiamiamo la pt_translate per avere una traduzione di un indirizzo virtuale e NON c'è una corrispondenza, allora significa che il frame fisico richiesto NON è stato ancora caricato in memoria. Bisogna prima quindi creare uno spazio con as_create, poi chiamare la load_segment per caricare l'effettivo segmento in memoria attingendo dal file elf e aggiornare la tabella delle pagine (e la TLB).
!!!!!! In questo modo, stiamo nella pratica implementando l'on-demand page loading!

### Algoritmi di sostituzione delle pagine
La page table contiene tutti gli indirizzi fisici degli indirizzi virtuali MAPPATI. Un indirizzo virtuale inoltre può essere non mappato in memoria, per i motivi scritti in seguito.
Se la page table teoricamente riesce a coprire qualsiasi traduzione logico-fisica degli indirizzi, allora a cosa servono gli algoritmi di sostituzione delle pagine? Servono per la MEMORIA FISICA, che è limitata e non può contenere tutti i frame possibili di un processo. Dunque, riguardano il caso in cui la memoria virtuale di un processo è superiore a quella fisica. Ogni frame in memoria fisica ha una sua mappatura in page table, dunque non appena un frame viene rimosso o aggiunto, bisogna correttamente aggiornare anche la tabella delle pagine.

## SWAPFILE
Il progetto ci richiede di scrivere su un file chiamato SWAPFILE le pagine che vanno scritte in memoria. Creiamo dunque dei file di supporto, swapfile.h e swapfile.c, nei quali scriviamo il codice per implementare tale logica.
Per quanto riguarda la lettura e scrittura, anziché usare le classiche fread e fwrite, che sarebbero andate anche bene e che avrebbero garantito una maggiore portabilità (almeno di questo modulo), sfruttiamo le tipiche funzioni forniteci dall'interfaccia di OS161: VOP_READ e VOP_WRITE. Queste due funzioni (che tra l'altro possono chiamare una read o write virtuali o non, in base alla configurazione) lavorano su una struct vnode (FCB) che ho definito come variabile globale.
Lo swapfile dunque viene inizializzato nella swap_init con vfs_open che, nel modo in cui è stato scritto, ci dà i permessi di lettura e scrittura sul file, lo apre e se grazie a O_CREAT se non è stato ancora creato lo crea.

Nelle funzioni swap_in (da cambiare forse in swap_push) e swap_out bisogna inizializzare le due classiche struct iovec e uio.
Successivamente, si fa la chiamata a VOP_WRITE o VOP_READ per la scrittura o lettura sul file.

Dopo una swap_out, allora significa che l'iesimo indirizzo scritto sullo swapfile non è più presente nello swapfile, è stato consumato. Anziché fare un'ulteriore VOP_WRITE sugli stessi bit, e quindi consumare un'ulteriore operazione di I/O, ho deciso di utilizzare una semplice bitmap, in cui l'indice dell'array indica se lo stesso indice per lo swapfile è valido oppure no, ossia se contiene un indirizzo valido oppure consumato e quindi non più valido.
Sfrutto due macro definite in swapfile.h che sono SF_ABSENT=0 e SF_PRESENT=1;

Nelle operazioni di lettura e scrittura, visto che stiamo usando anche una bitmap, ho introdotto uno spinlock per essere sicuri di stare leggendo una entry valida o non valida, per non avere race conditions.

Per quanto riguarda la dimensione, che deve essere fissata a 9MB, facciamo ancora uso delle funzioni che ci fornisce l'interfaccia di OS161 e chiamiamo la VOP_TRUNCATE. Di seguito chat gpt ci spiega a cosa serve:

La funzione VOP_TRUNCATE è una funzione definita nell'interfaccia del file system di OS/161. Essa è utilizzata per modificare la dimensione di un file specifico nel sistema di file. In particolare, VOP_TRUNCATE viene utilizzata per ridimensionare un file alla dimensione specificata.

La firma della funzione VOP_TRUNCATE è la seguente:

int VOP_TRUNCATE(struct vnode *v, off_t newlen);

Dove:

    v è un puntatore alla struttura struct vnode che rappresenta il file nel sistema di file.
    newlen è la nuova lunghezza (dimensione) desiderata per il file.

Quando chiami VOP_TRUNCATE, il sistema di file esegue le operazioni necessarie per modificare la dimensione del file in base al valore specificato in newlen. Questo potrebbe comportare il taglio del file o l'estensione del file, a seconda del valore di newlen.

Nel contesto della gestione della memoria virtuale e del file di swap, utilizzare VOP_TRUNCATE per impostare la dimensione massima del file di swap garantisce che il file abbia una dimensione fissa e non superi mai la dimensione desiderata (nel tuo caso, 9 MB).

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
2- Completare addrspace (fatto)
2.1 - Capire cosa fare con seg stack -> definire funzioni apposite per la creazione, inizializzazione e allocazione
3- Completare load_elf (fatto ma fare check insieme su dimensioni segmento e pagina)
4- Far funzionare veramente i programmi user
5- Fare in modo che premendo q nel menu si stampino le vmstats
6- Gestire i permessi di ogni pagina o segmento
7- Mettere gli #if OPT_NOVAVM
8- controllare se funziona lo swapfile (se scrive le pagine e le toglie)
