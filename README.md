# Backup Software PDS

## Network Application Protocol

1. Il client invia le credenziali
2. Il server risponde
    - `OK` credenziali accettate
    - `KO` credenziali rifiutate e chiude la conessione
3. Il client invia la modalità in cui vuole operare:
    - `MODE_SYNC` -> Ricezione dei dati dal server
    - `MODE_BACK` -> Invio i dati al server
4. Il server invia un messaggio di conferma
   1. `OK` -> Ho ricevuto il messaggio
   2. `KO` -> Si è verificato un errore, la connessione verrà chiusa

Qui il software si biforca nelle due modalità:

- SYNC
  1. Invio file
     1. Il server invia un JSON della classe SyncedFile contenente
          - Path
          - Hash
          - Dimensione del file
          - Flag per dire se è un file regolare (false se cartella o il file non esiste)
     2. Il server attende la risposta del client e si possono verificare due casi
          - `OK` -> Il file è già presente sul client e il server non deve fare altro
          - `NO` -> Il file non è presente sul client quindi bisogna procedere nel seguente modo:
              1. Invio del file binario
              2. Attendo la risposta che può essere:
                  - `OK` caricamento completato, il client non deve fare altro
                  - `KO` si sono verificati errori, il server deve rieffettuare l'invio a partire dal punto 1.1
     3. Il server può comportarsi in due modi:
        1. Torna al punto 1.1 se non ha finito l'invio dei file
        2. Invia `END` e chiude la connessione se ha terminato
- BACK
  1. Invio file
      1. Il client invia un JSON della classe SyncedFile contenente
          - Path
          - Hash
          - Dimensione del file
          - Flag per dire se è un file regolare (false se cartella o il file non esiste)
      2. Attendo la risposta del server e si possono verificare due casi
          - `OK` -> Il file è già presente sul server e il client non deve fare altro
          - `NO` -> Il file non è presente sul server quindi bisogna procedere nel seguente modo:
              1. Invio del file binario
              2. Attendo la risposta che può essere:
                  - `OK` caricamento completato, il client non deve fare altro
                  - `KO` si sono verificati errori, il client deve rieffettuare l'invio a partire dal punto 1.1

I dati sono inviati su una sessione tls che rende sicuro il canale.

### "Pacchetto" credenziali

Le credenziali sono inviate in un json contenente 'username' e 'password'. Dopo la fine del json aggiungo i caratteri `\\n` (`\\\n` considerando gli escape) per permettere al server di identificare la fine del "pacchetto".
La scelta dei caratteri di terminazione è dovuto al fatto che sono caratteri che non è possibile trovare all'interno di un json corretto.
L'username verrà usato per creare una cartella con quel nome, quindi non sono ammessi i seguenti caratteri: `spazio`, `.`, `/`, `\`, `"`, `[`, `]`, `:`, `,`, `;`, `|`, `=`, `\n` e dovrà essere lungo almeno 3 caratteri.
La password invece non dovrà contenere `spazio` e `\n` e dovrà essere lunga almeno 8 caratteri.

### "Pacchetto" di risposta

Le risposte da parte del server verso il client possono essere di tre tipi `OK`, `NO`, `KO`, tutti gli altri valori verranno interpretati come `KO` e tutti devono essere obbligatoriamente seguiti da `\\n` per il riconoscimento del "paccheto dal server"

### "Pacchetto" con le informazioni sul file

Le informazioni del file sono inviate in un json, anche questo terminato con `\\n` contenente:

- hash => sha256 del file
- path => percorso del file a partire dalla cartella di sincronizzazione designata
- is_file => flag per indicare se è un file valido (o una cartella)
- file_size => dimensione del file (0 se cartella)
- fila_status => stato del file (0 creato, 1 modificato, 2 eliminato, 3 non valido)

## Client

Il programma si può avviare nelle due modalità designate tramite l'utilizzo di alcuni flag da linea di comando:

- `-r` significa che il client verrà avviato in primis in modalità `MODE_SYNC` e successivamente dopo aver terminato il restore dei file dal server continuerà in modalità backup. In questa modalità l'utente deve essere conscio che i file con lo stesso nome, all'interno della cartella di sincronizzazione scelta, verranno sovrascritti.
- `-b` o nessun parametro, significa che il client si avvierà esclusivamente in modalità backup.

Il client all'avvio legge un file di configurazione, se non è presente o non è corretto il programma si chiude. Dopodichè controlla se la cartella designata nel file di configurazione esiste altrimenti il programma termina nuovamente.

### SYNC_MODE

Durante questa modalità, il client si connette direttamente al server dal main thread utilizzando il protocollo sopracitato. Fino a quando il server non invierà il messaggio `END` il client rimarrà in attesa dei file mancanti. Quando il server invia il messaggio `END`, la sessione viene chiusa. A questo punto il client provvede a creare un'altra connessione in modalità `MODE_BACK`.

### MODE_BACK

A questo punto il client fa partire un thread secondario per svolgere due principali funzioni:

- Scansione delle modifiche del file system, eseguito dal thread principale
- Upload dei file sul server, eseguito dal thread secondario

Le due parti sono unite grazie all'uso di una coda FIFO thread safe implementata nella classe UploadJobs.
Il primo thread si occupa di scansionare il file system ogni un tempo definito nel file di configurazione e quando rileva un cambiamento di un file(presenza, assenza o hash diverso) lo aggiunge alla coda inserito in un'apposita struttura e ad una mappa in cui sono presenti tutti i file conosciuti e sincronizzati.
L'altro si occupa prima di tutto di effettuare la conessione al server e mandare la richiesta di autenticazione, se va a buon fine il programma procede, altrimenti verrà chiuso. Se non riesce a connettersi o la connessione cade rieffettuerà un tentativo di connessione dopo il tempo inserito nel file di configurazione.
Successivamente si occuperà di prendere il file dalla coda, aspettando su una condition variable se non ci sono elementi, inviare prima le informazioni del file tramite il JSON e il file binario se non è presente sul server. Se il trasferimento va a buon fine salva la mappa dei file sul disco (ignorando i file in coda) e ricomincierà estraendone un altro dalla coda.

L'invio dei dati è gestito in maniera sincrona, attendendo l'invio delle informazioni prima di uscire dalla funzione chiamante, questa scelta è stata fatta perchè il client può connettersi solo ad un server per volta e viene gestito l'invio di un solo file per volta.

Le strutture dati condivise tra i thread sono la coda precedente menzionata, la mappa con le informazioni dei file e il puntatore alla struttura dati che contiene le informazioni sul file. La mappa è usata da thread di scansione del filesystem quando nota dei cambiamenti e dal thread per l'upload quando deve salvare la mappa su disco e per proteggerla è stata usato un `std::unique_lock`, similmente anche nel puntatore al file sono stati usati `std::unique_lock` per i metodi in scrittura e `std::shared_lock` per i metodi in lettura e un `std::atomic<bool>` per proteggere la variabile che indica se il file è in attesa di sincronizzazione o no.

Se un invio di dati o una ricezione vanno in timeout, verrà chiusa la connessione e rieffettuata dopo il tempo definito nel file di configurazione.

## Server

Il server a differenza del client usa le librerie di boost::asio in maniera asincrona, permettendo di gestire più client per ogni singolo thread, inoltre per aumentare la parallelizzazione vengono lanciati più thread per gestire le operazioni.
Anche qui le informazioni di configurazione riguardandi la porta su cui ascoltare, il numero di thread di usare, il percorso del certificato, il percorso della chiave, la password della chiave e il file temporaneo per diffie helman.
All'avvio del programma vengono caricati da un file di configurazione gli utenti e per ogni utente viene letta una mappa serializzata in un file json con il suo username nel nome.
In caso di problemi nel leggere il file il programma procede considerando tutti gli utenti come nuovi utenti e senza conoscere i file al momento sincronizzati.
Gli utenti sono salvati nel file uno per riga, con formato: ``username hash_password_e_sale sale``, mentre le mappe dei file sono salvate come nel client.
Successivamente vengono effettuati accept, handshake e autenticazione, se quest'ultima va a buon fine si procede, altrimenti viene chiusa la connessione.
Tramite una mappa statica condivisa tra le molteplici sessioni tcp durante l'autenticazione verifichiamo che l'utente non sia già connesso, l'utente viene aggiunto alla mappa dopo avere effettuato l'autenticazione e rimosso durante la distruzione di essa.

Le strutture condivise sono una mappa di puntatori a mappe passata ad ogni sessione dopo avere effuato l'autenticazione, la mappa degli utenti e la mappa degli utenti connesi, tutte protette da degli `std::shared_lock` in lettura e `std::unique_lock` in scrittura.

Come prima cosa il server si mette in attesa di una connessione e dopo aver istaurato un canale TLS e aver autenticato l'utente, il server attende la modalità che il client vuole utilizzare. In base all'una o l'altra scelta il server si comporterà scegliendo uno tra i protocolli sopracitati.
