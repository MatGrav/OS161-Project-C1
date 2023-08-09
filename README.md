# os161-base-2.0.3

## COREMAP
La coremap è una struttura dati che ci serve a tenere traccia dei frame liberi (e occupati) in memoria. Ricordiamoci che una pagina può essere mappata in più address space.
La coremap è vista come una struttura dati che ha all'interno: una struct come entry generica, la size della coremap.
L'entry generica della coremap è composta così: l'indirizzo fisico della pagina, un vettore di puntatori (**) di struct addrspace (attenzione: forse va modificata la struttura?),
il numero di addrspace (inteso come: in quanti address space è mappata la pagina), un intero (o bool) che indica se la pagina è occupata o libera.

Dunque, bisogna cambiare le funzioni che allocano pagine di memoria, in quanto bisognerà far riferimento a questa struttura coremap che, a sua volta, marcherà i frame liberi o occupati
e popolerà i suoi campi. Le funzioni da modificare: getppages e free_kpages che, in teoria, non usano più la ram_stealmem perché quest'ultima prende a prescindere memoria fisica senza 
controllare effettivamente se lo spazio è libero o occupato (che ruolo ha quindi la ram_stealmem? -> Va sostituita, serve una nuova funzione che alloca memoria

Da vedere come fare se serve memoria quando non è attiva la coremap. O impedire di allocare memoria finché la coremap non è pronta



