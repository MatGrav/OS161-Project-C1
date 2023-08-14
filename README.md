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
La page table si deve occupare della traduzione da indirizzo logico a indirizzo fisico. Dunque, il kernel lavorerà allocando in maniera contigua, mentre a livello user si utilizzerà la paginazione. La pagetable è vista come una struct con all'interno un vettore di pagine fisiche e ALTRO DA DEFINIRE.
Bisogna definire (VEDERE COME) il numero di pagine della page table. Come fare? NON è uguale a nRamFrames perché non riguarda tutta la ram, compresa la zona kernel, ma solo lo spazio user.
La logica con cui si accede al corrispondente valore fisico di un indirizzo è il seguente:
pt_map() -> pongo in ingresso il fisico e il virtuale, dal virtuale dividendo per PAGE_SIZE ottengo l'indice di pagina che sfrutto anche come indice del vettore in cui inserire il corrispondente valore fisico
pt_translate()-> una volta inseriti i valori fisici, per tradurre un indirizzo virtuale basterà semplicemente ottenere l'indice (sempre dividendo per PAGE_SIZE) e accedere all'i-esima posizione del vettore all'interno della struct pagetable.

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


