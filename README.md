# Progetto Sistemi Operativi: FAT File System
Progetto per il corso di Sistemi Operativi di Giorgio Grisetti, corso di Ingegneria Informatica e Automatica dell'Università di Roma "Sapienza".

Lo scopo del progetto è implementare un file system con "pseudo" FAT tramite mmapping su un buffer. L'utente comunica con il file system tramite comandi da terminale, attraverso i quali può effettuare le seguenti operazioni:
- Creazione di un file: mk <filename>
- Creazione di una directory: mkdir <dirname>
- Eliminazione di un file: rm <filename>
- Eliminazione di una directory: rmdir <dirname>
- Elenco dei file contenuti nella directory corrente: ls
- Spostamento tra directory: cd <dirname>

Per le operazioni di lettura e scrittura sono state implementate delle funzioni di apertura e chiusura dei file, assieme ad una variabile di tipo FileHandle che tiene traccia del file attualmente aperto e la posizione di un puntatore all'interno del file. Il puntatore viene aggiornato ad ogni operazione di lettura e scrittura. L'utente può cambiarne posizione tramite l'operazione seek.
- Apertura di un file: open <filename>
- Scrittura sul file attualmente aperto: write <text>
- Lettura dal file attualmente aperto: read
- Spostamento della posizione del puntatore all'interno del file: seek <filepos>
- Chiusura del file attualmente aperto: close

A cura di Karen Kolendowska, matricola 1937724
