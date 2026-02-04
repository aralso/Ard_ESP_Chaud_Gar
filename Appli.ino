
#include "variables.h"

// Variables pour le PID
double Consigne, Input, Output;
double Kp = 1.7, Ki = 0.5, Kd = 0.5;
uint16_t Consigne_G, Consigne_HG;
#define MIN_MAX 5
uint8_t min_max_pid;
uint8_t  WIFI_CHANNEL;

// Loi d'eau
int8_t Pt1;
uint8_t Pt2, Pt1Val, Pt2Val;

uint8_t HG, MMCh;
uint8_t mode_regul;

// planning programme chaudiere
planning_t plan[NB_MAX_PGM];
uint16_t forcage_horaire;
uint8_t forcage_consigne;
uint8_t Mode_Fixe;
uint8_t ch_arret, chaudiere;
unsigned long last_chaudiere_change = 0;

char mac_chaudiere[40]="";   // B0:CB:D8:E9:0C:74  adresse mac esp_chaudiere

PID myPID(&Input, &Output, &Consigne, Kp, Ki, Kd, DIRECT);

#ifdef Temp_int_DS18B20
  // Capteur temperature Dallas DS18B20  Temperature intérieure
  typedef uint8_t DeviceAddress[8];
  const int PIN_Tint = 13;      // Tint:Entrée onewire GPIO DS18B20
  //OneWire oneWire(PIN_Tint);
  //DallasTemperature ds(&oneWire);
  int nb_capteurs_temp = 1;  //DS18B20
  DeviceAddress Thermometer[5];
  DeviceAddress adds;
#endif

DHT dht[] = {
  { PIN_Tint22, DHT22 },
};
#ifdef Temp_int_HDC1080
  ClosedCube_HDC1080 hdc1080;
#endif

// Temperature intérieure
float Tint;
float loi_eau_Tint;

float T_obj;
float T_loi_eau;

// Entrée analogique pour mesurer la température exterieure
uint16_t Text1, Text2, Text1Val, Text2Val;  // valeurs pour calibration (T*10)
float Text;
#define resolutionADC 4095
#define TBETA 3950  // Coefficient Beta de la thermistance
uint16_t TBeta;
#define R0E 10000  // Résistance à 25° pour une NTC10K
uint16_t Therm0;

uint16_t S_analo_max;  // voltage pour 20°C
float Teau = 18;
float TRef = 18;

#define TPACMIN 10
#define TPACMAX 30
uint8_t TPacMin, TPacMax;

// Régulation
#define MODE 1  // 1:STM32 PID  2:ESP PID
uint8_t Mode;

uint16_t heure_arret, dernier_fct;

float loi_deau(float temp_ext, float temp_cons, float *Tloi);

#ifdef Temp_int_DS18B20
  void printAddress(DeviceAddress deviceAddress) {
    for (uint8_t i = 0; i < 8; i++) {
      Serial.print("0x");
      if (deviceAddress[i] < 0x10) Serial.print("0");
      Serial.print(deviceAddress[i], HEX);
      if (i < 7) Serial.print(", ");
    }
    Serial.println("");
  }
#endif

void init_10_secondes()
{
}

//setup au debut
void setup_0()
{

  if (NB_Graphique)
  {
    graphique[0][0] = 180;  //int - vert
    graphique[1][0] = 185;
    //graphique[2][0] = 190;
    //graphique[3][0] = 185;
    //graphique[4][0] = 180;
    //graphique[5][0] = 185;
  }
  /*graphique[0][1] = 110;  // ext - bleu
  graphique[1][1] = 80;
  graphique[2][1] = 103;
  graphique[3][1] = 95;
  graphique[0][2] = 245;  // eau - rouge
  graphique[1][2] = 253;
  graphique[2][2] = 273;
  graphique[3][2] = 255;*/

  if (NB_Graphique==6)
  {
    /*graphique[0][3] = 35; 
    graphique[1][3] = 38;
    graphique[2][3] = 42;
    graphique[3][3] = 32;
    graphique[0][4] = 50;  // divisé par 10
    graphique[1][4] = 55;
    graphique[2][4] = 48;  
    graphique[3][4] = 52;*/
  }

}

// setup : lecture nvs
void setup_nvs()
{

    // periode du cycle : lecture Temp ext par internet
    periode_cycle = preferences_nvs.getUChar("cycle", 0);  // de 10 a 120
    if ((periode_cycle < 10) || (periode_cycle > 120)) {
      periode_cycle = 60;
      preferences_nvs.putUChar("cycle", periode_cycle);
      Serial.printf("Raz periode cycle : val par defaut %imin\n\r", periode_cycle);
    }
    else Serial.printf("periode cycle : %imin\n", periode_cycle);


    mode_rapide = preferences_nvs.getUChar("Rap", 0);  // mode=12 => mode_rapide
    if ((mode_rapide) && (mode_rapide != 12)) {
      mode_rapide=0;
      preferences_nvs.putUChar("Rap", 0);
      Serial.println("Raz Mode rapide:0");
    }
    else
      Serial.printf("Mode rapide : %i\n\r", mode_rapide);


  #ifndef ESP_THERMOMETRE

      // Initialisation des valeurs de PID
    Kp = (float)preferences_nvs.getUShort("Kp", 0) / 100;    //  *100
    Ki = (float)preferences_nvs.getUShort("Ki", 0) / 10000;  //  *10000
    Kd = (float)preferences_nvs.getUShort("Kd", 0) / 10000;  //  *10000
    uint8_t err_coeff = 0;
    if ((!Kp)) err_coeff = 1;
    if (Kp > 2) err_coeff = 1;
    if (err_coeff) {
      Serial.println("PID: nouvelles valeurs par défaut");
      Kp = 1.66;
      Ki = 2;
      Kd = 0.1;
      preferences_nvs.putUShort("Kp", 166);  // *100
      preferences_nvs.putUShort("Ki", 10000);  // *10000
      preferences_nvs.putUShort("Kd", 1000);  // *10000
      Serial.printf("Raz PID: Kp:%.2f  Ki:%.4f  Kd:%.4f\n\r", Kp, Ki, Kd);
    }
    else
      Serial.printf("PID: Kp:%.2f  Ki:%.4f  Kd:%.4f\n\r", Kp, Ki, Kd);
    myPID.SetTunings(Kp, Ki, Kd);




    /*  // Lecture des temperatures PAC minimum et Maximum
      TPacMin = preferences_nvs.getUChar("Tmin", 0);  // 10°C
      TPacMax = preferences_nvs.getUChar("Tmax", 0);  // 30°C
      if ((TPacMin < 5) || (TPacMin > 30) || (TPacMax < 15) || (TPacMax > 40) || (TPacMin >= TPacMax)) {
        TPacMin = TPACMIN;
        TPacMax = TPACMAX;
        Serial.printf("Raz Min-Max: %i°C  %i°C\n", TPacMin, TPacMax);
        preferences_nvs.putUChar("Tmin", TPacMin);  //
        preferences_nvs.putUChar("Tmax", TPacMax);  //
      } else
        Serial.printf("TPAC Min-Max: %i°C  %i°C\n", TPacMin, TPacMax);*/


      /* Text1 = preferences_nvs.getUShort("T1", 0);   // 10°C  *10
      Text2 = preferences_nvs.getUShort("T2", 0);   // 20°C
      Text1Val = preferences_nvs.getUShort("T1V", 0); // 500
      Text2Val = preferences_nvs.getUShort("T2V", 0); // 2000
      uint8_t err_calib_sonde=0;
      if ((Text1<10) || (Text1>200)) err_calib_sonde=1;  // 1°C à 20°C
      if ((Text2<150) || (Text2>300)) err_calib_sonde=1; // 15°C à 30°C
      if ((Text1>Text2) || (Text2-Text1<50)) err_calib_sonde=1; // au moins 5°C d'écart
      if ((Text1Val<Text2Val) || (Text1Val-Text2Val<200)) err_calib_sonde=1; // au moins 200 d'écart
      if (err_calib_sonde)
      {
        Serial.println("Calib T.Ext: nouvelles valeurs par défaut");
        Text1 = 70;  // 7°C
        Text2 = 250;  // 25°C
        Text1Val = 2673;
        Text2Val = 1630;
        preferences_nvs.putUShort("T1", Text1);   // 
        preferences_nvs.putUShort("T2", Text2);   // 
        preferences_nvs.putUShort("T1V", Text1Val); // 
        preferences_nvs.putUShort("T2V", Text2Val); // 
      } 
      Serial.printf("Calib T.Ext: Point1: %i pour %.1f°C   Point2: %i°C -> %.1f°C\n", Text1Val, (float)Text1/10, Text2Val, (float)Text2/10) ;*/

      // Mode regulation : 1:normal, 2:loi d'eau avec Text, 3:fixe
      mode_regul = preferences_nvs.getUChar("REGUL", 0);
      if ((!mode_regul) || (mode_regul > 3)) {
        mode_regul = 1;
        preferences_nvs.putUChar("REGUL", mode_regul);
        Serial.printf("Raz mode Regul:%i\n\r", mode_regul);
      } else
        Serial.printf("mode Regul:%i\n\r", mode_regul);


      TRef = (float)preferences_nvs.getUShort("TRef", 0) / 10;  // pour le cas de mode_regul=fixe
      if ((TRef < 10) || (TRef > 30)) {
        TRef = 18.0;
        preferences_nvs.putUShort("TRef", 180);
        Serial.printf("Raz TRef:%f\n", TRef);
      } else
        Serial.printf("TRef=%f\n", TRef);


      // Mode 1 : PID=STM32  2:PID=ESP32
      Mode = preferences_nvs.getUChar("Mode", 0);
      if ((!Mode) || (Mode > 3)) {
        Mode = MODE;
        preferences_nvs.putUChar("Mode", Mode);
        Serial.printf("Raz Mode PID:%i\n", Mode);
      } else
        Serial.printf("Mode PID :%i\n", Mode);

      // valeurs de la loi d'eau
      Pt1 = preferences_nvs.getUChar("Pt1", 0) - 30;  // -5°C +30 =>25
      Pt1Val = preferences_nvs.getUChar("Pt1V", 0);   // 35°C
      Pt2 = preferences_nvs.getUChar("Pt2", 0);       // 20°C
      Pt2Val = preferences_nvs.getUChar("Pt2V", 0);   // 20°C
      uint8_t err_loi_eau = 0;
      if ((Pt1 < -10) || (Pt1 > 15)) err_loi_eau = 1;                   // -10°C à 15°C
      if ((Pt2 < 10) || (Pt2 > 30)) err_loi_eau = 1;                    // 10°C à 30°C
      if ((Pt1 > Pt2) || (Pt2 - Pt1 < 10)) err_loi_eau = 1;             // au moins 10°C d'écart
      if ((Pt1Val < Pt2Val) || (Pt1Val - Pt2Val < 5)) err_loi_eau = 1;  // au moins 5° d'écart
      if (err_loi_eau) {
        Serial.println("Loi d'eau: nouvelles valeurs par défaut");
        Pt1 = -5;     // -5°C
        Pt1Val = 35;  // 35°C
        Pt2 = 20;     // 20°C
        Pt2Val = 20;  // 20°C
        preferences_nvs.putUChar("Pt1", Pt1 + 30);
        preferences_nvs.putUChar("Pt1V", Pt1Val);
        preferences_nvs.putUChar("Pt2", Pt2);
        preferences_nvs.putUChar("Pt2V", Pt2Val);
        Serial.printf("Raz Loi d'eau: Point1: %i°C -> %i°C   Point2: %i pour %i°C\n\r", Pt1, Pt1Val, Pt2, Pt2Val);
      }
      else
        Serial.printf("Loi d'eau: Point1: %i°C -> %i°C   Point2: %i pour %i°C\n\r", Pt1, Pt1Val, Pt2, Pt2Val);



      min_max_pid = preferences_nvs.getUChar("MPid", 0);
      if ((min_max_pid < 2) || (min_max_pid > 10)) {
        min_max_pid = MIN_MAX;  //5
        preferences_nvs.putUChar("MPid", min_max_pid);
        Serial.printf("Raz Min_max_Pid: %i\n", min_max_pid);
      }
      else
        Serial.printf("Min_max_Pid: %i\n", min_max_pid);


      // Initialisation des variables de consignes/HG/MMC
      Consigne_G = preferences_nvs.getUChar("Cons", 0);   //  *10
      MMCh = preferences_nvs.getUChar("MMC", 0);   //  *10
      Consigne_HG = preferences_nvs.getUChar("C_HG", 0);  //  *10
      HG = preferences_nvs.getUChar("HG", 0);
      if ((Consigne_G < 130) || (Consigne_G > 220))  // entre 13°C et 22°C
      {
        Serial.println("Raz consigne :  valeur par defaut:15°C");
        Consigne_G = 150;  //15°C
        preferences_nvs.putUChar("Cons", Consigne_G);
      }
      if ((Consigne_HG < 80) || (Consigne_HG > 160))  // entre 8°C et 16°C
      {
        Serial.println("consigne Hors-gel: nouvelle valeur par defaut");
        Consigne_HG = 120;  //12°C
        preferences_nvs.putUChar("C_HG", Consigne_HG);
      }
      if ((!HG) || (HG > 2))  // 1:non 2:HG
      {
        Serial.println("HG : non actif par defaut");
        HG = 1;
        preferences_nvs.putUChar("HG", HG);
      }
      if (HG == 2) Consigne = (float)Consigne_HG / 10;
      else Consigne = (float)Consigne_G / 10;

      if ((!MMCh) || (MMCh>2))
      {
        Serial.println("Raz Chauffage : inactif pas défaut");
        MMCh = 1;
        preferences_nvs.putUChar("MMC", MMCh);
      }
      Serial.printf("Chauf:%i Consigne:%.1f Consigne_G:%i HG:%i Consigne_HG:%i \n", MMCh, Consigne, Consigne_G, HG, Consigne_HG);




      // Initialisation ratio Tint sur loi d'eau (écart par rapport à 20°C)
      loi_eau_Tint = float(preferences_nvs.getUChar("LoiTint", 0))/100;
      if ((loi_eau_Tint<0.5) || (loi_eau_Tint > 2.5))  // autour de 1.5
      {
        loi_eau_Tint = 1.5;
        Serial.printf("Raz loi_eau_Tint : valeur par defaut:%.2f\n\r", loi_eau_Tint);
        preferences_nvs.putUChar("LoiTint", (uint8_t)(loi_eau_Tint*100));
      }
      else
        Serial.printf("loi_eau_Tint : %.2f\n\r", loi_eau_Tint);

      // Initialisation du PID
      //myPID.SetMode(AUTOMATIC);                          // Active le PID
      //myPID.SetOutputLimits(-min_max_pid, min_max_pid);  // Limites de la commande (-10+10)


      // Lecture des programmes 1, 2, 3
      char key[10];
      for (uint8_t i = 0; i < NB_MAX_PGM; i++) {
        sprintf(key, "P%d_deb", i);
        plan[i].ch_debut = preferences_nvs.getUChar(key, 0); // 0 à 143
        if (plan[i].ch_debut > 143) {
          plan[i].ch_debut = 0;
          preferences_nvs.putUChar(key, 0);
          Serial.printf("Raz Plan %d debut\n", i);
        }

        sprintf(key, "P%d_fin", i);
        plan[i].ch_fin = preferences_nvs.getUChar(key, 0); // 0 à 143
        if (plan[i].ch_fin > 143) {
          plan[i].ch_fin = 0;
          preferences_nvs.putUChar(key, 0);
          Serial.printf("Raz Plan %d fin\n", i);
        }

        sprintf(key, "P%d_typ", i);
        plan[i].ch_type = preferences_nvs.getUChar(key, 0); // 0 à 2
        if (plan[i].ch_type > 2) {
          plan[i].ch_type = 0;
          preferences_nvs.putUChar(key, 0);
          Serial.printf("Raz Plan %d type\n", i);
        }

        sprintf(key, "P%d_con", i);
        plan[i].ch_consigne = preferences_nvs.getUChar(key, 190); // 50 à 230
        if ((plan[i].ch_consigne < 50) || (plan[i].ch_consigne > 230)) {
          plan[i].ch_consigne = 190; // 19°C
          preferences_nvs.putUChar(key, 190);
          Serial.printf("Raz Plan %d consigne\n", i);
        }

        sprintf(key, "P%d_apr", i);
        plan[i].ch_cons_apres = preferences_nvs.getUChar(key, 170); // 30 à 230
        if ((plan[i].ch_cons_apres < 30) || (plan[i].ch_cons_apres > 230)) {
          plan[i].ch_cons_apres = 170; // 17°C
          preferences_nvs.putUChar(key, 170);
          Serial.printf("Raz Plan %d cons_apres\n", i);
        }
        Serial.printf("Plan %d : %d-%d Typ:%d Cons:%d Apr:%d\n", i, plan[i].ch_debut, plan[i].ch_fin, plan[i].ch_type, plan[i].ch_consigne, plan[i].ch_cons_apres);
      }
    #endif  // Fin ESP_chaudiere

    #ifdef ESP_THERMOMETRE
      // Initialisation variable adresse Mac chaudiere
      String storedString = preferences_nvs.getString("MacC", "");
      if ((storedString.length() < 40) && (storedString.length() > 3)) {
        storedString.toCharArray(mac_chaudiere, sizeof(mac_chaudiere));
        Serial.printf("Adresse MAc Chaudiere : %s\n\r", mac_chaudiere);
      }

      // Initialisation du channel préférentiel wifi-esp-now
      WIFI_CHANNEL = preferences_nvs.getUChar("WifiC", 0);
      if ((WIFI_CHANNEL < 1) || (WIFI_CHANNEL > 13)) {
        WIFI_CHANNEL = 6;  // 1 à 13
        preferences_nvs.putUChar("WifiC", WIFI_CHANNEL);
        Serial.printf("Raz Wifi Channel: %i\n", WIFI_CHANNEL);
      }
      else
        Serial.printf("Wifi channel preferentiel: %i\n", WIFI_CHANNEL);
      if (!reset_deep_sleep) last_wifi_channel = WIFI_CHANNEL;

    #endif

}


// setup apres la lecture nvs, avant démarrage reseau
void setup_1()
{
  // initialisation capteur de température intérieur
  #ifdef ESP_THERMOMETRE
    Tint = 15;
    #ifdef Temp_int_DHT22
      dht[0].begin();
    #endif
 
    #ifdef Temp_int_HDC1080
      Wire.begin(21, 22); // Forçage des pins SDA=21, SCL=22 pour ESP32 DevKit V1
      hdc1080.begin(0x40);
    #endif

 
    #ifdef Temp_int_DS18B20
      ds.begin();  // Startup librairie DS18B20
      nb_capteurs_temp = ds.getDeviceCount();
      Serial.print("Nb Capteurs DS18B20:");
      Serial.println(nb_capteurs_temp);
      if (nb_capteurs_temp > 1) nb_capteurs_temp = 1;
      int j;
      for (j = 0; j < nb_capteurs_temp; j++) {
        Serial.print(" Capteur :");
        ds.getAddress(Thermometer[j], j);
        printAddress(Thermometer[j]);
      }
    #endif

    // lecture initiale temperature interieure
    uint8_t Tint_err = lecture_Tint(&Tint);
    if ((Tint < 1) || (Tint > 45)) {
      Tint = 20.0;
      Tint_err = 7;
    }
    if (Tint_err) log_erreur(Code_erreur_Tint, Tint_err, 0);
  #endif
}

// apres demarrage reseau
void setup_2()
{
  #ifdef ESP_CHAUDIERE
    // Configuration WiFi en mode Station pour ESP-NOW
    WiFi.mode(WIFI_STA);
    
    // 🔍 DIAGNOSTIC: Forcer le canal WiFi
    uint8_t current_channel;
    wifi_second_chan_t second;
    esp_wifi_get_channel(&current_channel, &second);
    Serial.printf("Canal WiFi AVANT config ESP-NOW: %d\n", current_channel);
    
    // Forcer le canal si nécessaire (doit correspondre au routeur)
    // esp_wifi_set_promiscuous(true);
    // esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    // esp_wifi_set_promiscuous(false);
    
    if (esp_now_init() != ESP_OK) {
      Serial.println("Erreur initialisation ESP-NOW");
      return;
    }
    esp_now_register_recv_cb(OnDataRecv);
    
    // Vérifier le canal après init
    esp_wifi_get_channel(&current_channel, &second);
    
    Serial.println("\n\n======================================");
    Serial.println("🔵 ESP-NOW Initialisé (RÉCEPTEUR)");
    Serial.print("   MAC Address: ");
    Serial.println(WiFi.macAddress());
    Serial.printf("   Canal WiFi: %d\n", current_channel);
    Serial.println("   En attente de messages...");
    Serial.println("======================================\n\n");
    delay(2000); // 2 secondes de pause pour lire
  #endif
}



void appli_event_on(systeme_eve_t evt)
{
}

void appli_event_off(systeme_eve_t evt)
{
}

// type 1
uint8_t requete_Get_appli(const char* var, float *valeur)
  //uint8_t requete_Get_appli (String var, float *valeur) 
{
  uint8_t res=1;

  if (strncmp(var, "Tint",5) == 0) {
    res = 0;
    *valeur = Tint;
  }
  if (strncmp(var, "Text",5) == 0) {
    res = 0;
    *valeur = Text;
  }
  if (strncmp(var, "Teau",5) == 0) {
    res = 0;
    *valeur = Teau;
  }
  if (strncmp(var, "Kp",3) == 0) {
    res = 0;
    *valeur = Kp;
  }
  if (strncmp(var, "Ki",3) == 0) {
    res = 0;
    *valeur = Ki;
  }
  if (strncmp(var, "Kd",3) == 0) {
    res = 0;
    *valeur = Kd;
  }
  if (strncmp(var, "consigne",9) == 0) {
    res = 0;
    *valeur = Consigne;
  }
  if (strncmp(var, "HG",3) == 0) {
    res = 0;
    *valeur = HG-1;
  }
/*  if (strncmp(var, "Ballon",7) == 0) {
    res = 0;
    *valeur = Ballon;
  }*/
  if (strncmp(var, "MMC",4) == 0) {
    res = 0;
    *valeur = MMCh-1;
  }
  if (strncmp(var, "codeR_pac",10) == 0) {
    res = 0;
    if (cpt_securite)  *valeur=1;
    else *valeur=0;
  }
  if (var[0] == 'P' && strlen(var) == 4 && var[2] == '_')
  {
    uint8_t i = var[1] - '0';
    uint8_t f = var[3] - '0';

    if (i < NB_MAX_PGM) {
      if (f == 0) *valeur = plan[i].ch_debut;
      else if (f == 1) *valeur = plan[i].ch_fin;
      else if (f == 2) *valeur = plan[i].ch_type;
      else if (f == 3) *valeur = plan[i].ch_consigne;
      else if (f == 4) *valeur = plan[i].ch_cons_apres;
    res=0;
    }
  }
  return res;
}

// Mise à jour de l'état de la chaudière avec sécurité 20s
void maj_etat_chaudiere()
{
  #ifdef ESP_CHAUDIERE
  Serial.printf("MMCh:%i\n\r", MMCh);
  if (MMCh==2)
  {
    if (init_time) lectureHeure();
    
    // Ordre de priorité : 
    // 1. Hors Gel (HG)
    // 2. Consigne Fixe (Mode_Fixe)
    // 3. Forcage (forcage_horaire)
    // 4. Planning

    if (HG == 2) {
       // Consigne déjà gérée (Consigne_HG)
       Serial.println("Mode HORS GEL actif");
    }
    else if (Mode_Fixe == 1) {
       Consigne = (float)Consigne_G / 10.0;
       Serial.println("Mode CONSIGNE FIXE actif");
    }
    else 
    {
      // Vérification Forcage
      float h_actuelle = heure; 
      float h_fin_forcage = forcage_horaire / 60.0;
      bool forcage_actif = false;

      if (forcage_horaire > 0) {
        if (h_actuelle < h_fin_forcage) {
          Consigne = (float)forcage_consigne / 10.0;
          forcage_actif = true;
          Serial.printf("Mode FORCAGE actif jusqu'à %.2f (%.2f°C)\n", h_fin_forcage, Consigne);
        } else {
           // Fin du forcage
           forcage_horaire = 0;
           forcage_consigne = 0;
           Serial.println("Fin du Forcage");
        }
      }

      // Si pas de forcage, on lance le Planning
      if (!forcage_actif)
      {
          uint8_t planning_actif = 0;
          float consigne_apres_max = -1; // Pour stocker la consigne "après" du dernier programme terminé
          float fin_max = -1; // Heure de fin la plus tardive parmi les programmes terminés
    
          // 1. Parcours des programmes pour trouver une tranche active
          for (uint8_t i = 0; i < NB_MAX_PGM; i++) 
          {
            float h_debut = plan[i].ch_debut / 6.0; // Conversion index 10min -> heure float
            float h_fin = plan[i].ch_fin / 6.0;
    
            // Si programme definis (non nul)
            if (plan[i].ch_debut != 0 || plan[i].ch_fin != 0) 
            {
               // Vérification si on est dans la tranche horaire
               if (h_actuelle >= h_debut && h_actuelle < h_fin) 
               {
                  Consigne = (float)plan[i].ch_consigne / 10.0;
                  planning_actif = 1;
                  Serial.printf("Planning %d Actif : Consigne = %.1f\n", i, Consigne);
                  break; // Priorité trouvée, on sort
               }
    
               // Mémorisation du programme terminé le plus tardif pour la gestion "après"
               if (h_actuelle >= h_fin) 
               {
                 if (h_fin > fin_max) 
                 {
                   fin_max = h_fin;
                   consigne_apres_max = (float)plan[i].ch_cons_apres / 10.0;
                 }
               }
            }
          }
    
          // 2. Si aucune tranche horaire active, on applique la consigne "après" du dernier programme terminé
          if (!planning_actif) 
          {
            if (consigne_apres_max != -1) 
            {
              Consigne = consigne_apres_max;
              Serial.printf("Planning Inactif. Last Fin: %.2f. Consigne Apres = %.1f\n", fin_max, Consigne);
            }
            else 
            {
              // Si aucun programme n'est terminé, Consigne défaut (Consigne_G)
              Consigne = (float)Consigne_G / 10.0;
            }
          }
      }
    }

    unsigned long now = millis();
    if (now - last_chaudiere_change > 20000) // Sécurité 20s
    {
      if (Consigne < Tint) 
      {
        chaudiere = 1; 
        digitalWrite(PIN_Chaudiere, LOW);  // DesActivation chaudiere
        last_chaudiere_change = now;
        Serial.println("Régulation : Arrêt Chaudière (Consigne < Tint)");
      }
      else 
      {
        chaudiere = 2;
        digitalWrite(PIN_Chaudiere, HIGH);  // Activation chaudiere
        last_chaudiere_change = now;
        Serial.println("Régulation : Marche Chaudière (Consigne >= Tint)");
      }
    }
  }
  #endif
}

// type 1
uint8_t requete_Set_appli (String param, float valf) 
{
  uint8_t res=1;
  int8_t val = round(valf);

  if (cpt_securite)
  {
    if (param == "consigne")     // Forcage consigne, rajouter duree
    {
      if (HG == 2)  // Hors Gel
      {
        if ((valf >= 8.0) && (valf <= 16.0))  // 8°C a 16°C
        {
          Consigne_HG = round(valf * 10);
          Consigne = valf;
          preferences_nvs.putUChar("C_HG", Consigne_HG);
          res = 0;
        }
      } else if (HG == 1)  // normal
      {
        if ((valf >= 13.0) && (valf <= 22.0))  // 13°C à 22°C
        {
          Consigne_G = round(valf * 10);
          Consigne = valf;
          preferences_nvs.putUChar("Cons", Consigne_G);
          res = 0;
        }
      }
      maj_etat_chaudiere(); // Mise à jour immédiate
    }

    if (param == "RTint") {
      res = 0;
      Tint = valf;
      last_remote_temp_time = millis();
      cpt24_Tint++;
      tempI_moy24h += Tint;
      Serial.printf("Réception RTint : %.2f°C\n", Tint);
    }


    if (param == "HG") 
    {
      if (valf)  // activation Hors gel
      {
        HG = 2;  // HG
        Consigne = (float)Consigne_HG / 10;
        //Ballon = 1;  // ballon eteint
        //save_modbus(160, 0);
        preferences_nvs.putUChar("HG", 2);
        //preferences_nvs.putUChar("Bal", 1);
        res = 0;
      }
      else
      {
        HG = 1;  // mode normal
        Consigne = (float)Consigne_G / 10;
        //Ballon = 2;  // ballon allumé
        //save_modbus(160, 1);
        preferences_nvs.putUChar("HG", 1);
        //preferences_nvs.putUChar("Bal", 2);
        res = 0;
      }
      maj_etat_chaudiere(); // Mise à jour immédiate
      //Serial.printf("HG:%i valf:%.1f Consigne:%.1f\n", HG, valf, Consigne);
    } 


    // --- Gestion du Planning P0_0 à P2_4 ---
    if (param.startsWith("P") && param.length() == 4 && param[2] == '_')
    {
      uint8_t i = param[1] - '0';
      uint8_t f = param[3] - '0';
      if (i < NB_MAX_PGM)
      {
        char key[10]; 
        if (f == 0) {
            plan[i].ch_debut = val;
            sprintf(key, "P%d_deb", i);
            preferences_nvs.putUChar(key, val);
        }
        else if (f == 1) {
            plan[i].ch_fin = val;
            sprintf(key, "P%d_fin", i);
            preferences_nvs.putUChar(key, val);
        }
        else if (f == 2) {
            plan[i].ch_type = val;
            sprintf(key, "P%d_typ", i);
            preferences_nvs.putUChar(key, val);
        }
        else if (f == 3) {
            plan[i].ch_consigne = val;
            sprintf(key, "P%d_con", i);
            preferences_nvs.putUChar(key, val);
        }
        else if (f == 4) {
            plan[i].ch_cons_apres = val;
            sprintf(key, "P%d_apr", i);
            preferences_nvs.putUChar(key, val);
        }
        
        // Synchronisation avec STM32
        res = 0;
        maj_etat_chaudiere(); // Mise à jour après modif planning
      }
    }

    if (param == "vbatt")
    {
      res = 0;
      Vbatt_Th = valf;
      Serial.printf("Réception Vbatt Distante : %.2fV\n", Vbatt_Th);
    }

    if (param == "MMC")
    {
      unsigned long mil_tmp = millis();
      if (mil_tmp - last_chaudiere_change > 20000)  // min 20s
      {
        Serial.printf("MMC:%.2f\n\r", valf);
        if (val==1)  // activation chaudiere
        {
          res=0;
          Serial.println("MMC actif");
          MMCh = 2;  // chaudiere active
          preferences_nvs.putUChar("MMC", 2);
          maj_etat_chaudiere(); // Mise à jour immédiate
        }
        if (val==0)  // arret chaudiere
        {
          res=0;
          Serial.println("MMC inactif");
          MMCh = 1;  // chaudiere éteinte
          preferences_nvs.putUChar("MMC", 1);
          maj_etat_chaudiere(); // Mise à jour immédiate
        }
        if (res==0)       last_chaudiere_change = mil_tmp;
      }
    }

    if (param == "MFix") {
       Mode_Fixe = (uint8_t)round(valf);
       Serial.printf("Mode_Fixe: %d\n", Mode_Fixe);
       res = 0;
       maj_etat_chaudiere();
    }
    if (param == "ForcC") {
       forcage_consigne = (uint8_t)round(valf * 10);
       Serial.printf("Forcage Consigne: %.1f\n", valf);
       res = 0;
       maj_etat_chaudiere();
    }
    if (param == "ForcH") {
       forcage_horaire = (uint16_t)round(valf);
       Serial.printf("Forcage Horaire: %d min\n", forcage_horaire);
       res = 0;
       maj_etat_chaudiere();
    }
  }

  return res;
}

// type 2
uint8_t requete_GetReg_appli(int reg, float *valeur)
{
  uint8_t res=1;

  if (reg == 10)  // registre 10 : PID mode : 4
  {
    res = 0;
    *valeur = mode_regul;
  }
  if (reg == 11)  // registre 11 : Teau fixe
  {
    res = 0;
    *valeur = TRef;
  }
  if (reg == 12)  // registre 12 : Kp
  {
    res = 0;
    *valeur = Kp;
  }
  if (reg == 13)  // registre 13 : Ki
  {
    res = 0;
    *valeur = Ki;
  }
  if (reg == 14)  // registre 14 : Kd
  {
    res = 0;
    *valeur = Kd;
  }
  if (reg == 15)  // registre 15 : Loi d'eau Point 1
  {
    res = 0;
    *valeur = Pt1;
  }
  if (reg == 16)  // registre 16 : Loi d'eau Valeur au Point 1
  {
    res = 0;
    *valeur = Pt1Val;
  }
  if (reg == 17)  // registre 17 : Loi d'eau Point 2
  {
    res = 0;
    *valeur = Pt2;
  }
  if (reg == 18)  // registre 18 : Loi d'eau Valeur au Point 2
  {
    res = 0;
    *valeur = Pt2Val;
  }
  if (reg == 19)  // registre 19 : Loi d'eau Tint : ecart a 20°C
  {
    res = 0;
    *valeur = loi_eau_Tint;
  }
  if (reg == 21)  // registre 21 : valeur PID
  {
    res = 0;
    *valeur = Output;
  }
  if (reg == 22)  // registre 22 : min_max_pid
  {
    res = 0;
    *valeur = min_max_pid;
  }
  if (reg == 23)  // registre 23 : Teau min
  {
    res = 0;
    *valeur = TPacMin;
  }
  if (reg == 24)  // registre 24 : Teau max
  {
    res = 0;
    *valeur = TPacMax;
  }


  if (reg == 40)  // registre 40 : Mode
  {
    res = 0;
    *valeur = Mode;
  }
  
  if (reg == 41)  // registre 41 : canal WiFi actuel
  {
    res = 0;
    uint8_t current_channel;
    #ifdef ESP_THERMOMETRE
      current_channel = last_wifi_channel;
    #else
      wifi_second_chan_t second;
      esp_wifi_get_channel(&current_channel, &second);
    #endif
    *valeur = (float)current_channel;
  }
  if (reg == 42)  // registre 42 : canal WiFi preferentiel
  {
    res = 0;
    *valeur = (float)WIFI_CHANNEL;
  }

  return res;
}

// type 2
uint8_t requete_SetReg_appli(int param, float valeurf)
{
  int16_t valeur = int16_t(valeurf);
  uint8_t res = 1;

  if (param == 10)  // registre 10 : mode_regul: 1, 2, 3
  {
    if ((valeur) && (valeur < 4)) {
      res = 0;
      mode_regul = valeur;
      preferences_nvs.putUChar("REGUL", mode_regul);
    }
  }

  if (param == 11)  // registre 11 : forcage Teau
  {
    res = 0;
    if ((valeurf >= 10) && (valeurf <= 30))  // 10 a 30°C
    {
      TRef = valeurf;
      preferences_nvs.putUShort("TRef", (uint16_t)(valeurf*10));  // enregistrement si reboot , pour le cas de mode_regul=fixe
    }
  }
  if (param == 12)  // registre 12 : Kp
  {
    if ((valeurf) && (valeurf < 2)) {
      res = 0;
      Kp = valeurf; //(float)valeur / 100;
      myPID.SetTunings(Kp, Ki, Kd);
      preferences_nvs.putUShort("Kp", (uint16_t)(valeurf*100));
    }
  }
  if (param == 13)  // registre 13 : Ki
  {
    res = 0;
    Ki = valeurf;
    myPID.SetTunings(Kp, Ki, Kd);
    preferences_nvs.putUShort("Ki", (uint16_t)(valeurf*10000));
  }
  if (param == 14)  // registre 14 : Kd
  {
    res = 0;
    Kd = valeurf;
    myPID.SetTunings(Kp, Ki, Kd);
    preferences_nvs.putUShort("Kd", (uint16_t)(valeurf*10000));
  }

  if (param == 15)  // registre 15 : Loi d'eau Point 1
  {
    uint8_t err_loi_eau = 0;
    int8_t Pt1_prov = valeur - 30;
    if ((Pt1_prov < -10) || (Pt1_prov > 15)) err_loi_eau = 1;        // -10°C à 15°C
    if ((Pt1_prov > Pt2) || (Pt2 - Pt1_prov < 10)) err_loi_eau = 1;  // au moins 10°C d'écart
    if (!err_loi_eau) {
      res = 0;
      Pt1 = valeur - 30;
      preferences_nvs.putUChar("Pt1", valeur);
    }
  }

  if (param == 16)  // registre 16 : Loi d'eau Valeur au Point 1
  {
    uint8_t err_loi_eau = 0;
    if ((valeur < Pt2Val) || (valeur - Pt2Val < 5)) err_loi_eau = 1;  // au moins 5° d'écart
    if (!err_loi_eau) {
      res = 0;
      Pt1Val = valeur;
      preferences_nvs.putUChar("Pt1Val", valeur);
    }
  }
  if (param == 17)  // registre 17 : Loi d'eau Point 2
  {
    uint8_t err_loi_eau = 0;
    if ((valeur < 10) || (valeur > 30)) err_loi_eau = 1;         // 10°C à 30°C
    if ((Pt1 > valeur) || (valeur - Pt1 < 10)) err_loi_eau = 1;  // au moins 10°C d'écart
    if (!err_loi_eau) {
      res = 0;
      Pt2 = valeur;
      preferences_nvs.putUChar("Pt2", valeur);
    }
  }

  if (param == 18)  // registre 18 : Loi d'eau Valeur au Point 2
  {
    uint8_t err_loi_eau = 0;
    if ((Pt1Val < valeur) || (Pt1Val - valeur < 5)) err_loi_eau = 1;  // au moins 5° d'écart
    if (!err_loi_eau) {
      res = 0;
      Pt2Val = valeur;
      preferences_nvs.putUChar("Pt2Val", valeur);
    }
  }
  if (param == 19)  // registre 19 : loi eau Tint , ecart a 20°C
    if ((valeurf >= 0.5) && (valeurf <2.5))  
    {
      res = 0;
      loi_eau_Tint = valeurf;
      preferences_nvs.putUChar("LoiTint", (uint8_t)(loi_eau_Tint*100));
    }


  if (param == 22)  // registre 22 : Min_max pid
  {
    if ((valeur >= 2) && (valeur <= 10)) {
      res = 0;
      min_max_pid = valeur;
      preferences_nvs.putUChar("MPid", min_max_pid);
    }
  }
  if (param == 23)  // registre 23 : TPACmin
  {
    if ((valeur >= 5) && (valeur <= 30) && (valeur < TPacMax)) {
      res = 0;
      TPacMin = valeur;
      preferences_nvs.putUChar("Tmin", TPacMin);
    }
  }
  if (param == 24)  // registre 24 : TPAC max
  {
    if ((valeur >= 15) && (valeur <= 40) && (TPacMin < valeur)) {
      res = 0;
      TPacMax = valeur;
      preferences_nvs.putUChar("Tmax", TPacMax);
    }
  }


  if (param == 30)  // registre 30 : index
  {
    res = 0;
    index_val = valeur;
  }
  if (param == 31)  // registre 31 : graphique temp int
  {
    res = 0;
    graphique[index_val][0] = valeur;
  }
  if (param == 32)  // registre 32 : graphique temp ext
  {
    res = 0;
    //preTransmission();
    graphique[index_val][1] = valeur;
  }
  if (param == 33)  // registre 33 : graphique temp pac
  {
    res = 0;
    //postTransmission();
    graphique[index_val][2] = valeur;
  }
  if (param == 40)  // registre 40 : mode
  {
    if ((valeur) && (valeur <= 3)) {
      res = 0;
      Mode = valeur;
      preferences_nvs.putUChar("Mode", Mode);
    }
  }
  if (param == 41)  // registre 41 : last_wifi_channel
  {
    if ((valeur) && (valeur <= 13))
    {
      res = 0;
      last_wifi_channel = valeur;
    }
  }
  if (param == 42)  // registre 42 : canal wifi preferentiel
  {
    if ((valeur) && (valeur <= 13))
    {
      res = 0;
      WIFI_CHANNEL = valeur;
      preferences_nvs.putUChar("WifiC", WIFI_CHANNEL);
    }
  }

  return res;
}


uint8_t requete_Get_String_appli(uint8_t type, String var, char *valeur)
{
  uint8_t res=1;
  int paramV = var.toInt();
  // valeur limité a 50 caractères
  
  if (paramV == 11)  // registre 11 : adresse MAC ESP_Chaudiere
  {
    res = 0;
    strncpy(valeur, mac_chaudiere, 25);
    valeur[25] = '\0';
  }

  return res;
}

uint8_t requete_Set_String_appli(int param, const char *texte)
{
  uint8_t res=1;
  IPAddress ip;

    if (param == 11)  // registre 11 : adresse Mac chaudiere
    {
      res = 0;
      preferences_nvs.putString("MacC", texte);
      strncpy(mac_chaudiere, texte, sizeof(mac_chaudiere) - 1);
      ip_websocket[sizeof(mac_chaudiere) - 1] = '\0';  // Assure la terminaison
    }

  return res;
}

// type5 : reception message ACTION par uart ou par page web
uint8_t requete_action_appli(const char *reg, const char *data)
{
  uint8_t res=1;

if (strcmp(reg, "Test1") == 0) 
    { 
      res=0; 
      requete_status(buffer_dmp, 0, 1);
      Serial.println(buffer_dmp);
    }

if (strcmp(reg, "Test2") == 0) 
    { 
      res=0; 
      uint8_t Tint_erreur = lecture_Tint(&Tint);
      Serial.println(Tint_erreur);
      Serial.println(Tint);
    }

  return res;
}


// erreur :0:ok  sinon erreur 2 à 7
uint8_t lecture_Tint(float *mesure)
{
  uint8_t Tint_erreur = 7;
  float valeur = 20;

  #ifdef ESP_THERMOMETRE

    #ifdef Temp_int_DHT22
      //dht[0].begin();

      if (digitalRead(PIN_Tint22) == HIGH || digitalRead(PIN_Tint22) == LOW)
      {
        valeur =  dht[0].readTemperature();
        if (isnan(valeur))
        {
          valeur = 20.0;
          Tint_erreur = 6;
          Serial.println("---DHT:non numérique");
        }
        else
        {
          Tint_erreur=0;
        }
      }
      else
        Serial.println("---DHT:signal non stable!");
    #endif

    #ifdef Temp_int_HDC1080
      valeur = hdc1080.readTemperature();
      if (isnan(valeur)) {
        valeur = 20.0;
        Tint_erreur = 4;
      } else {
        Tint_erreur=0;
      }
    #endif

    #ifdef Temp_int_DS18B20
      ds.requestTemperatures();
      valeur = ds.getTempCByIndex(0);
      Tint_erreur=0;
    #endif

  #endif

  if (valeur > 50) Tint_erreur = 2;
  if (valeur < -20) Tint_erreur = 3;
  Serial.printf("lecture Tint : %.2f Err:%i\n\r", valeur, Tint_erreur);
  *mesure = valeur;
  return Tint_erreur;
}



//mesure temperature exterieure
uint8_t lecture_Text(float *mesure) {
  uint8_t Text_erreur = 0;
  int16_t Val_Text = 1600;
  float valeur;

  #ifdef MODBUS
    Text_erreur = read_modbus(2, &Val_Text);  // registre 1-2-3 pour temp exterieure
    valeur = (float)Val_Text / 10;
  #else
    valeur = 18.0;

    /*#ifndef DEBUG_SANS_Sonde_Ext
        Val_Text = analogRead( PIN_Text );  // 0 à 4096
        //Serial.println(Val_Text);
      #endif*/
    // calibration
    // Text1:100(10°C) Text1Val:500
    // Text2:200(20°C) Text2Val:2000
    //valeur = ((float)(Text1Val-Val_Text)/(Text1Val-Text2Val)*(Text2-Text1) + Text1)/10;

    /*float Vmesure = ((float)Val_Text / resolutionADC) * 3.66;
      float Rntc = 15000 * Vmesure / (3.3 - Vmesure);  // Calcul de la résistance de la thermistance
      float T_kelvin = 1.0 / ((1.0 / 298.15) + (1.0 / TBeta) * log(Rntc / Therm0));    // Calcul de la température en Kelvin
      valeur = T_kelvin - 273.15;    // Conversion en °C */
    //Serial.printf("val_text:%i vmesure:%.3f rntc:%.0f T_kelvin:%.1f valeur:%.1f\n", Val_Text, Vmesure, Rntc, T_kelvin, valeur);
  #endif

  if ((valeur < -30.0) || (valeur > 60.0)) Text_erreur = 1;
  if (!Val_Text) Text_erreur = 2;

  *mesure = valeur;
  return Text_erreur;
}



void fetch_internet_temp() {
  HTTPClient http;
  String url = "http://api.open-meteo.com/v1/forecast?latitude=" LATITUDE "&longitude=" LONGITUDE "&current=temperature_2m";
  
  if (http.begin(url)) {
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      DynamicJsonDocument doc(512);
      DeserializationError error = deserializeJson(doc, payload);
      
      if (!error) {
        float temp = doc["current"]["temperature_2m"];
        if (temp > -50.0 && temp < 60.0) {
          Text = temp;
          //Serial.printf("Météo Garches : %.1f°C\n", Text);
          cpt24_Text++;
          tempE_moy24h += Text;
        }
      } else {
        Serial.printf("Erreur parsing JSON Météo : %s\n", error.c_str());
      }
    } else {
      Serial.printf("Erreur HTTP Météo (%d) : %s\n", httpCode, http.errorToString(httpCode).c_str());
    }
    http.end();
  }
}

void event_mesure_temp()  // toutes les 15 minutes : modif allumage chaudiere
{
  uint8_t i;

    #ifdef ESP_THERMOMETRE
    // --- MODE THERMOMETRE DISTANT (ESP-NOW) ---
    uint8_t Tint_erreur = lecture_Tint(&Tint);  // Mesure locale
    if (Tint_erreur) Tint=50;

    
    // Initialisation WiFi en mode Station (nécessaire pour ESP-NOW)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
      Serial.println("Error initializing ESP-NOW");
      ESP.restart();
    }

    // Préparation du Peer (Chaudière)
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo)); // Initialisation complète à zéro
    memcpy(peerInfo.peer_addr, MAC_CHAUDIERE, 6);
    peerInfo.channel = 0; // Le canal sera défini avant l'ajout
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA; // Interface WiFi Station (OBLIGATOIRE)

    // 🚀 OPTION 1 : Forcer le canal connu (plus rapide et économe en énergie)
    // Si vous connaissez le canal de votre routeur, décommentez ces lignes :
    /*
    Serial.printf("🎯 Forçage canal %d (défini dans variables.h)\n", WIFI_CHANNEL);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
    last_wifi_channel = WIFI_CHANNEL; // Pour la prochaine fois
    */

    // 🔍 OPTION 2 : Scan robuste des canaux (si le canal n'est pas connu ou change)
    bool deliverySuccess = false;
    uint8_t channels_to_try[14];
    int nb_trials = 0;
    
    // Logique de priorité intelligente :
    // 1. Si last_wifi_channel a déjà fonctionné ET est différent de WIFI_CHANNEL
    //    → Essayer last_wifi_channel en premier (il a plus de chances de marcher)
    // 2. Sinon, essayer WIFI_CHANNEL en premier
    // 3. Puis essayer l'autre
    // 4. Enfin, scanner tous les autres canaux
    
    if (last_wifi_channel > 0 && last_wifi_channel != WIFI_CHANNEL) {
      // Cas 1 : last_wifi_channel est différent de WIFI_CHANNEL
      channels_to_try[nb_trials++] = last_wifi_channel;  // Priorité 1 : dernier qui a marché
      channels_to_try[nb_trials++] = WIFI_CHANNEL;       // Priorité 2 : canal configuré
      Serial.printf("🔍 Priorité: canal %d (dernier OK), puis %d (configuré)\n", 
                    last_wifi_channel, WIFI_CHANNEL);
    } else {
      // Cas 2 : last_wifi_channel == WIFI_CHANNEL ou non initialisé
      channels_to_try[nb_trials++] = WIFI_CHANNEL;       // Priorité 1 : canal configuré
      Serial.printf("🔍 Priorité: canal %d (configuré)\n", WIFI_CHANNEL);
    }
    
    // Remplissage des autres canaux (1-13)
    for (int c = 1; c <= 13; c++) {
      // Éviter les doublons
      bool already_added = false;
      for (int i = 0; i < nb_trials; i++) {
        if (channels_to_try[i] == c) {
          already_added = true;
          break;
        }
      }
      if (!already_added) {
        channels_to_try[nb_trials++] = c;
      }
    }

    Serial.printf("🔍 Scan de %d canaux (priorité: canal %d)\n", nb_trials, last_wifi_channel);

    for (int k = 0; k < nb_trials; k++) {
      int current_channel = channels_to_try[k];
      
      // Fixer le canal
      Serial.printf("\n--- Essai canal %d ---\n", current_channel);
      esp_wifi_set_promiscuous(true);
      esp_wifi_set_channel(current_channel, WIFI_SECOND_CHAN_NONE);
      esp_wifi_set_promiscuous(false);
      
      // Vérifier que le canal a bien été changé
      uint8_t actual_channel;
      wifi_second_chan_t second;
      esp_wifi_get_channel(&actual_channel, &second);
      
      if (actual_channel != current_channel) {
        Serial.printf("⚠️ Échec changement canal (demandé:%d, actuel:%d)\n", current_channel, actual_channel);
        delay(50); // Attendre un peu plus
        esp_wifi_set_channel(current_channel, WIFI_SECOND_CHAN_NONE);
        esp_wifi_get_channel(&actual_channel, &second);
        Serial.printf("   2ème tentative: canal actuel=%d\n", actual_channel);
      } else {
        Serial.printf("✅ Canal changé: %d\n", actual_channel);
      }
      
      delay(50); // Délai pour stabilisation du canal

      // Ajouter le peer sur ce canal
      if (esp_now_is_peer_exist(MAC_CHAUDIERE)) {
        esp_now_del_peer(MAC_CHAUDIERE);
      }
      peerInfo.channel = actual_channel; // Utiliser le canal réel
      if (esp_now_add_peer(&peerInfo) != ESP_OK){
        Serial.println("❌ Échec ajout peer");
        continue;
      }
      Serial.println("✅ Peer ajouté");

      // Envoi Température
      Message_EspNow message;
      message.type = 1; // Température
      message.value = Tint;
      
      // 🔍 DIAGNOSTIC: Afficher les infos avant envoi
      Serial.printf("📤 Tentative envoi sur canal %d vers MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    actual_channel,
                    MAC_CHAUDIERE[0], MAC_CHAUDIERE[1], MAC_CHAUDIERE[2],
                    MAC_CHAUDIERE[3], MAC_CHAUDIERE[4], MAC_CHAUDIERE[5]);
      Serial.printf("   Message: Type=%d, Valeur=%.2f°C\n", message.type, message.value);
      
      esp_err_t result = esp_now_send(MAC_CHAUDIERE, (uint8_t *) &message, sizeof(message));

      if (result == ESP_OK) {
        Serial.printf("Envoye sur canal %d\n", actual_channel);
        
        // 🔔 Détection de changement de canal
        if (last_wifi_channel > 0 && last_wifi_channel != actual_channel) {
          Serial.println("\n⚠️ ========================================");
          Serial.printf("⚠️  CHANGEMENT DE CANAL DÉTECTÉ !\n");
          Serial.printf("⚠️  Ancien canal: %d → Nouveau canal: %d\n", last_wifi_channel, actual_channel);
          Serial.println("⚠️  Le routeur WiFi a probablement changé de canal");
          Serial.println("⚠️ ========================================\n");
          log_erreur(Code_erreur_wifi, last_wifi_channel, actual_channel); // Log du changement
        }
        
        last_wifi_channel = actual_channel; // Mémoriser pour la prochaine fois
        deliverySuccess = true; 
        
        // Envoi tension batterie tous les 100 cycles
        cpt_cycle_batt++;
        if (cpt_cycle_batt >= 100) {
          float Vbatt = readBatteryVoltage();
          delay(50);
          message.type = 2; // Batterie
          message.value = Vbatt;
          esp_now_send(MAC_CHAUDIERE, (uint8_t *) &message, sizeof(message));
          Serial.printf("Envoi batterie: %.2fV (cycle %d)\n", Vbatt, cpt_cycle_batt);
          cpt_cycle_batt = 0; // Réinitialiser le compteur
        }
        
        break; // On arrête le scan dès qu'un envoi réussit
      } else {
        // Échec d'envoi : supprimer le peer avant d'essayer le canal suivant
        esp_now_del_peer(MAC_CHAUDIERE);
      }
      delay(20); 
    }
    
    // Vérification du résultat de l'envoi
    if (!deliverySuccess) {
      Serial.println("ERREUR: Echec envoi ESP-NOW sur tous les canaux");
      log_erreur(Code_erreur_esp_now, 10, 0); // Code erreur ESP-NOW
    } else {
      Serial.printf("Succes envoi ESP-NOW (canal %d)\n", last_wifi_channel);
    }
    
    // Deep Sleep
    Serial.println("Go to Deep Sleep"); // consomme du temps
    Serial.flush(); 
    delay(20); 

    uint64_t sleep_time = (uint64_t)periode_cycle * 60 * 1000000;
    if (mode_rapide==12)
    sleep_time = (uint64_t)periode_cycle * 1000000;
    esp_sleep_enable_timer_wakeup(sleep_time);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_REVEIL, 0); // Réveil par bouton (0 = bas)
    esp_deep_sleep_start();
  
  #endif


  #ifdef ESP_CHAUDIERE
    // --- MODE CHAUDIERE ---
    // Récupération de la température extérieure par internet
    fetch_internet_temp();

    // Tint est mise à jour par les requêtes distantes (RTint)
    
    // activation ou desactivation chaudiere
    maj_etat_chaudiere();

    // enregistrement valeur pour graphique
    compteur_graph++;
    if (compteur_graph >= skip_graph)  // 1 valeur sur x
    {
      compteur_graph = 0;
      for (i = NB_Val_Graph - 1; i; i--) {
        graphique[i][0] = graphique[i - 1][0];
        graphique[i][1] = graphique[i - 1][1];
        graphique[i][2] = graphique[i - 1][2];
      }
      graphique[0][0] = round(Tint * 10);
      graphique[0][1] = round(Text * 10);

      graphique[0][2] = chaudiere*5+10;  // 15:arret 20:marche
    }
  #endif
}


float loi_deau(float temp_ext, float temp_cons, float *Tloi) {
  float temp_obj;
  *Tloi = (temp_ext - Pt1) / (Pt2 - Pt1) * (Pt2Val - Pt1Val) + Pt1Val;
  // adaptation a la consigne
  temp_obj = *Tloi + (temp_cons - 20) * loi_eau_Tint;
  if (temp_obj < TPacMin) temp_obj = TPacMin;
  if (temp_obj > TPacMax) temp_obj = TPacMax;
  return temp_obj;
}


float readBatteryVoltage() {
  // Lecture ADC (0-4095) sur PIN_Vbatt
  // Sur ESP32 DevKit V1, l'ADC est calibré par défaut
  int raw = analogRead(PIN_Vbatt);
  
  // Conversion:
  // raw / 4095.0 * 3.3V (tension ref approx) * 2 (pont diviseur) * 1.1 (facteur corection empirique souvent nécessaire sur ESP32)
  // On commence sans facteur 1.1 pour tester
  float voltage = (raw / 4095.0) * 3.3 * 2; 
  return voltage;
}

#ifdef ESP_CHAUDIERE
// Callback reception ESP-NOW
void OnDataRecv(const esp_now_recv_info_t * info, const uint8_t *incomingData, int len) {
  // 🔍 DIAGNOSTIC: Afficher infos de réception
  Serial.println("\n📥 ========== RECEPTION ESP-NOW ==========");
  Serial.printf("   Source MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                info->src_addr[0], info->src_addr[1], info->src_addr[2],
                info->src_addr[3], info->src_addr[4], info->src_addr[5]);
  
  // Afficher le canal WiFi actuel
  uint8_t current_channel;
  wifi_second_chan_t second;
  esp_wifi_get_channel(&current_channel, &second);
  Serial.printf("   Canal WiFi actuel: %d\n", current_channel);
  Serial.printf("   Taille reçue: %d octets\n", len);
  
  Message_EspNow receivedMessage;
  memcpy(&receivedMessage, incomingData, sizeof(receivedMessage));

  Serial.print("   Type: "); Serial.print(receivedMessage.type);
  Serial.print(" | Valeur: "); Serial.println(receivedMessage.value);
  Serial.println("=========================================\n");

  if (receivedMessage.type == 1) { // Temperature
    Tint = receivedMessage.value;
    if ((Tint > 50.01f) || (Tint < 49.99f))
    {
      last_remote_temp_time = millis();
      cpt24_Tint++;
      tempI_moy24h += Tint;
    }
    Serial.printf("✅ Tint mise à jour: %.2f°C\n", Tint);
    
    // Mise à jour regulation
    maj_etat_chaudiere();
  }
  else if (receivedMessage.type == 2) { // Batterie
    Vbatt_Th = receivedMessage.value;
    Serial.printf("✅ Vbatt_Th mise à jour: %.2fV\n", Vbatt_Th);
  }
  else {
    Serial.printf("⚠️ Type de message inconnu: %d\n", receivedMessage.type);
  }
}
#endif
