# Ard_ESP_Chaud_Garches :

Modification adresse IP : Message type 4 (à envoyer sur port série 115200 bauds)
2-1:14         // mode Ethernet filaire
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