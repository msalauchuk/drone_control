/**
* @file main.cpp
 * @brief Sistema principale del centro di controllo per una flotta di droni
 *
 * Questo file implementa il loop principale di controllo che gestisce:
 * - La sincronizzazione tra i droni
 * - L'esecuzione delle operazioni di controllo
 * - Il logging dello stato del sistema
 * - Il monitoraggio delle proprietà funzionali e non funzionali
 */

#include "cc.h"
#include "src/common/system_config.h"



/**
 * @brief Funzione principale del sistema di controllo droni
 *
 * Implementa il loop principale che gestisce l'intera simulazione del sistema
 * attraverso timestep discreti, mantenendo la sincronizzazione tra i componenti
 * e monitorando lo stato del sistema.
 *
 * @param argc Numero di argomenti da linea di comando
 * @param argv Array di argomenti da linea di comando
 * @return int Codice di uscita del programma
 */
int main(int argc, char* argv[])
{
    // defining the cc
    cc cc;
    cc.init();
    print_parameters(); //print parameters to stdoutput for readability

    //Itera per ogni timestep fino a HORIZON
    for (int timeStep = 0; timeStep < HORIZON; timeStep++)
    {
        printf("______________________\nt =  %d\n\n", timeStep);

        // Attende la sincronizzazione con Redis
        // Questo è un punto di sincronizzazione bloccante che garantisce
        // la consistenza dello stato tra tutti i componenti del sistema

        cc.sync_and_wait();
        //cc.print_status();

        // Esegue le azioni del centro di controllo per l'istante corrente:
        // - Aggiorna lo stato dei droni
        // - Calcola nuove istruzioni
        // - Gestisce ricariche e movimenti
        cc.tick(); // performs the action in a given istant of time t based on current status


        // Registra lo stato del sistema e monitora le proprietà:
        // - Salva i dati di stato
        // - Verifica proprietà funzionali
        // - Controlla metriche non funzionali
        cc.log(timeStep); // logs data + monitors
    }

    // Stampa messaggio di completamento se in modalità debug
    if (DEBUG)
    {
        printf("finished simulation\nclosing conn...\n");
    }

    // Pulizia risorse (commentato)
    // Chiude connessioni e libera memoria
    //cc.shutdown(); // cleans up everything
    return 0;
}
