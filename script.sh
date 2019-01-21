#!/bin/bash
 
# Controllo ci sia almeno un parametro [...]
argc="$#"
if [ $argc -eq 0 ]; then
    echo "Devi passare i seguenti parametri allo script:"
    echo "$0 [-help] configfile time"
    exit
fi
# [...] e se uno dei parametri è '-help'
for var in $@ 
do 
    if [ "$var" = "-help" ]; then
        echo "Lancia lo script con il file di configurazione e il tempo nel formato:"
        echo "$0 [-help] configfile time"
        exit
    fi
done

# Semplice controllo sul numero di argomenti
if [ "$#" != "2" ]; then
    echo "Lo script va lanciato con il seguente comando:"
    echo "$0 [-help] configfile time"
    exit
fi

# Controllo che il file di configurazione esista
if [ ! -f $1 ]; then
    echo "Il file di configurazione $1 non esiste"
    exit
fi

# Cerco il nome della Directory associata a DirName nel file di configurazione
DIR=$(grep -v '^#' $1 | grep DirName | cut -f 2 -d "=")
DIR=$(echo $DIR | tr -d ' ')

#Se non è una directory esistente
if [ ! -d $DIR ]; then        
    echo "La directory non esiste"
    exit
fi

#Se il valore passato è negativo
if [ $2 -lt 0 ]; then
    echo "Numero negativo"
    exit
fi

TIME=$2
 
# "archivia in un file con estensione .tar.gz tutti i files (e directories) contenuti in tale directory che sono pi´u vecchi di t minuti. Se l’operazione di archiviazione `e andata a buon fine, i file e le cartelle archiviati dovranno essere eliminati"
find $DIR -mmin $((-$TIME)) ! -path $DIR -exec tar -cvf script.tar {} + | xargs rm -vfd

if [ $TIME -ne 0 ]; then
	echo "Tar creato ed eliminato"
    exit
fi

#Se arrivo qua Time == 0
echo "Time == 0 -> stampo files presenti in $DIR (esce * se non c'è nulla)"

pushd "$DIR" > /dev/null
for f in *
do
	echo $f
done 
popd > /dev/null

exit