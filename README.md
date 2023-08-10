# os161-base-2.0.3

## COREMAP
La coremap è una struttura dati che ci serve a tenere traccia dei frame liberi (e occupati) in memoria. Ricordiamoci che una pagina può essere mappata in più address space.
La coremap è vista come una struttura dati che ha all'interno: una struct come entry generica, la size della coremap.
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
1) coremap_init inizializza le struct (la coremap static?)
2) coremap

## Nuovo gestore memoria virtuale

È stato aggiunto un nuovo gestore di memoria virtuale che sostituisce dumbvm. Il suo nome, scelto accuratamente dagli inventori, è NovaVM. Il file novavm.c si trova esattamente dove si trovava dumbvm.c: kern/arch/mips/vm
Esistono anche due "vm.h": ne creiamo uno nuovo appositamente per il nostro gestore, che si chiama novavm.h e si trova in kern/include.





