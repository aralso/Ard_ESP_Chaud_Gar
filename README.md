# Ard_ESP_Thermometre_Garches :

Modification adresse IP : Message type 4 (à envoyer sur port série 115200 bauds)
2-1:13         // mode Ethernet filaire
4-1:192.168.251.31  // adresse IP du module
4-2:192.168.251.1   // adresse IP de la gateway
4-3:255.255.255.0   // subnet IP
4-4:8.8.8.8         // DNS primaire
4-5:8.8.4.4         // DNS secondaire
4-6:Garches        // nom routeur pour wifi
4-7: xxx            // mdp routeur pour wifi
4-8:1               // websocket (1:off, 2:on)
4-9:ws://webcam.hd.free.fr:8081       // websocket IP
4-10:3              // Websocket ID
puis ARST0   // Reset

Periode cycle : 
rapide : 2-5:12

Programme ota : C:\Users\<toi>\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.x\tools\espota.py


il faut que tu rajoutes la gestion des 2 variables suivantes : 
uint8_t forcage_consigne; // en  dixieme de degré : consigne forcée
uint16_t forcage_horaire; // heure qui indique jusqu'à quand dure le forcage de la consigne
A l'initialisation ces valeurs sont égales à 0.
Dans le site web, en dessous de hors gel (avant le premier graphique), il faut faire apparaitre ces 2 champs pour qu'on puisse les saisir : 
le premier champs (forcage consigne) est une liste déroulante par 0,2 degré autour de la consigne actuelle, mais peut aussi être saisi pour renseigner une valeur donnée.
le 2e champ est aussi une liste déroulante, par 15 minutes, qui présente à la fois la durée, et à coté l'heure correspondante. 
A droite il y a 2 boutons : 1 pour valider consigne et horaire, l'autre à coté de la consigne avec indiqué  "jusqu'à xx" xx étant la fin de la tranche horaire actuelle ou le début de la suivante, si on n'est pas dans une tranche horaire.
 dans maj_etat_chaudiere, il faut prendre en compte ce forcage. Puis quand l'horaire est dépassé, il faut remettre à zéro ces variables et arreter ce forcage
Sur le site web, à la place de consigne et hors-gel, il faut prévoir les 4 lignes 
- Programmes actifs : slider
- Forcage consigne (avec puce rouge si actif) : mettre à coté les champs consigne et horaire
- consigne fixe : slider (avec puce rouge si actif)
- hors gel : slider (avec puce rouge si actif) :  valeur  consigne HG
Si consigne fixe est coché, les 3 autres sont désactivés
Si hors gel est coché, les 3 autres sont désactivés